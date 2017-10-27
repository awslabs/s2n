/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <openssl/err.h>
#include "tls/s2n_config.h"
#include "utils/s2n_asn1_time.h"
#include "utils/s2n_safety.h"

#include "openssl/ocsp.h"
#include "s2n_connection.h"

/* one day, boringssl, may add ocsp stapling support, let's future proof this a bit, by grabbing a definition
 * that would have to be there when they add support */
#if defined(OPENSSL_IS_BORINGSSL) && !defined(OCSP_RESPONSE_STATUS_SUCCESSFUL)
#define S2N_OCSP_STAPLING_SUPPORTED 0
#else
#define S2N_OCSP_STAPLING_SUPPORTED 1
#endif /* defined(OPENSSL_IS_BORINGSSL) && !defined(OCSP_RESPONSE_STATUS_SUCCESSFUL) */

uint8_t s2n_x509_ocsp_stapling_supported(void) {
    return S2N_OCSP_STAPLING_SUPPORTED;
}

void s2n_x509_trust_store_init(struct s2n_x509_trust_store *store) {
    store->trust_store = NULL;
}

uint8_t s2n_x509_trust_store_has_certs(struct s2n_x509_trust_store *store) {
    return store->trust_store ? (uint8_t) 1 : (uint8_t) 0;
}

int s2n_x509_trust_store_from_ca_file(struct s2n_x509_trust_store *store, const char *ca_file, const char *path) {
    s2n_x509_trust_store_cleanup(store);

    store->trust_store = X509_STORE_new();
    int err_code = X509_STORE_load_locations(store->trust_store, ca_file, path);

    if (!err_code) {
        s2n_x509_trust_store_cleanup(store);
        return -1;
    }

    X509_STORE_set_flags(store->trust_store, X509_VP_FLAG_DEFAULT);

    return 0;
}

void s2n_x509_trust_store_cleanup(struct s2n_x509_trust_store *store) {
    if (store->trust_store) {
        X509_STORE_free(store->trust_store);
        store->trust_store = NULL;
    }
}

int s2n_x509_validator_init_no_checks(struct s2n_x509_validator *validator) {
    validator->trust_store = NULL;
    validator->cert_chain = NULL;
    validator->validate_certificates = 0;
    validator->check_stapled_ocsp = 0;

    return 0;
}

int s2n_x509_validator_init(struct s2n_x509_validator *validator, struct s2n_x509_trust_store *trust_store, uint8_t check_ocsp) {
    notnull_check(trust_store);
    validator->trust_store = trust_store;

    validator->validate_certificates = 1;
    validator->check_stapled_ocsp = check_ocsp;

    validator->cert_chain = NULL;
    if (validator->trust_store->trust_store) {
        validator->cert_chain = sk_X509_new_null();
    }

    return 0;
}

void s2n_x509_validator_cleanup(struct s2n_x509_validator *validator) {
    if (validator->cert_chain) {
        X509 *cert = NULL;
        while ((cert = sk_X509_pop(validator->cert_chain))) {
            X509_free(cert);
        }

        sk_X509_free(validator->cert_chain);
    }

    validator->trust_store = NULL;
    validator->validate_certificates = 0;
}

/*
 * For each name in the cert. Iterate them. Call the callback. If one returns true, then consider it validated,
 * if none of them return true, the cert is considered invalid.
 */
static uint8_t verify_host_information(struct s2n_x509_validator *validator, struct s2n_connection *conn, X509 *public_cert) {
    uint8_t verified = 0;

    /* Check SubjectAltNames before CommonName as per RFC 6125 6.4.4 */
    STACK_OF(GENERAL_NAME) *names_list = X509_get_ext_d2i(public_cert, NID_subject_alt_name, NULL, NULL);
    GENERAL_NAME *current_name = NULL;
    while (!verified && names_list && (current_name = sk_GENERAL_NAME_pop(names_list))) {
        const char *name = (const char *) M_ASN1_STRING_data(current_name->d.ia5);
        size_t name_len = (size_t) M_ASN1_STRING_length(current_name->d.ia5);

        verified = conn->verify_host_fn(name, name_len, conn->data_for_verify_host);
    }

    GENERAL_NAMES_free(names_list);

    /* if none of those were valid, go to the common name. */
    if (!verified) {
        X509_NAME *subject_name = X509_get_subject_name(public_cert);
        if (subject_name) {
            int next_idx = 0, curr_idx = -1;
            while ((next_idx = X509_NAME_get_index_by_NID(subject_name, NID_commonName, curr_idx)) >= 0) {
                curr_idx = next_idx;
            }

            if (curr_idx >= 0) {
                ASN1_STRING *common_name =
                        X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subject_name, curr_idx));

                if (common_name) {
                    char peer_cn[255];
                    static size_t peer_cn_size = sizeof(peer_cn);
                    memset_check(&peer_cn, 0, peer_cn_size);
                    if (ASN1_STRING_type(common_name) == V_ASN1_UTF8STRING) {
                        size_t len = (size_t) ASN1_STRING_length(common_name);

                        lte_check(len, sizeof(peer_cn) - 1);
                        memcpy_check(peer_cn, ASN1_STRING_data(common_name), len);
                        verified = conn->verify_host_fn(peer_cn, len, conn->data_for_verify_host);
                    }
                }
            }
        }
    }

    return verified;
}

s2n_cert_validation_code
s2n_x509_validator_validate_cert_chain(struct s2n_x509_validator *validator, struct s2n_connection *conn, uint8_t *cert_chain_in,
                                       uint32_t cert_chain_len, struct s2n_cert_public_key *public_key_out) {

    if (validator->validate_certificates && !s2n_x509_trust_store_has_certs(validator->trust_store)) {
        return S2N_CERT_ERR_UNTRUSTED;
    }

    X509_STORE_CTX *ctx = NULL;

    struct s2n_blob cert_chain_blob = {.data = cert_chain_in, .size = cert_chain_len};
    struct s2n_stuffer cert_chain_in_stuffer;
    if (s2n_stuffer_init(&cert_chain_in_stuffer, &cert_chain_blob) < 0) {
        return S2N_CERT_ERR_INVALID;
    }
    if (s2n_stuffer_write(&cert_chain_in_stuffer, &cert_chain_blob) < 0) {
        return S2N_CERT_ERR_INVALID;
    }

    uint32_t certificate_count = 0;

    s2n_cert_validation_code err_code = S2N_CERT_ERR_INVALID;
    X509 *server_cert = NULL;

    while (s2n_stuffer_data_available(&cert_chain_in_stuffer)) {
        uint32_t certificate_size = 0;

        if (s2n_stuffer_read_uint24(&cert_chain_in_stuffer, &certificate_size) < 0) {
            goto clean_up;
        }

        if (certificate_size == 0 || certificate_size > s2n_stuffer_data_available(&cert_chain_in_stuffer)) {
            goto clean_up;
        }

        struct s2n_blob asn1cert;
        asn1cert.data = s2n_stuffer_raw_read(&cert_chain_in_stuffer, certificate_size);
        asn1cert.size = certificate_size;
        if (asn1cert.data == NULL) {
            goto clean_up;
        }

        const uint8_t *data = asn1cert.data;

        if (validator->validate_certificates) {
            /* the cert is der encoded, just convert it. */
            server_cert = d2i_X509(NULL, &data, asn1cert.size);

            if (!server_cert) {
                goto clean_up;
            }

            /* add the cert to the chain. */
            if (!sk_X509_push(validator->cert_chain, server_cert)) {
                X509_free(server_cert);
                goto clean_up;
            }
         }

        /* Pull the public key from the first certificate */
        if (certificate_count == 0) {
            /* Assume that the asn1cert is an RSA Cert */
            if (s2n_asn1der_to_public_key(&public_key_out->pkey, &asn1cert) < 0) {
                goto clean_up;
            }
            if (s2n_cert_public_key_set_cert_type(public_key_out, S2N_CERT_TYPE_RSA_SIGN) < 0) {
                goto clean_up;
            }
        }

        certificate_count++;
    }

    if (certificate_count < 1) {
        goto clean_up;
    }

    if (validator->validate_certificates) {
        X509 *leaf = sk_X509_value(validator->cert_chain, 0);
        if(!leaf) {
            err_code = S2N_CERT_ERR_INVALID;
            goto clean_up;
        }

        if(conn->verify_host_fn && !verify_host_information(validator, conn, leaf)) {
            err_code = S2N_CERT_ERR_UNTRUSTED;
            goto clean_up;
        }

        /* now that we have a chain, get the store and check against it. */
        ctx = X509_STORE_CTX_new();

        int op_code = X509_STORE_CTX_init(ctx, validator->trust_store->trust_store, leaf,
                                          validator->cert_chain);

        if (op_code <= 0) {
            goto clean_up;
        }

        uint64_t current_sys_time = 0;
        conn->config->wall_clock(conn->config->data_for_sys_clock, &current_sys_time);

        /* this wants seconds not nanoseconds */
        X509_STORE_CTX_set_time(ctx, 0, current_sys_time / 1000000000);

        op_code = X509_verify_cert(ctx);

        if (op_code <= 0) {
            op_code = X509_STORE_CTX_get_error(ctx);
            err_code = S2N_CERT_ERR_UNTRUSTED;
            goto clean_up;
        }

    }


    err_code = S2N_CERT_OK;

clean_up:
    if (ctx) {
        X509_STORE_CTX_cleanup(ctx);
    }

    s2n_stuffer_free(&cert_chain_in_stuffer);
    return err_code;
}

s2n_cert_validation_code s2n_x509_validator_validate_cert_stapled_ocsp_response(struct s2n_x509_validator *validator,
                                                                                struct s2n_connection *conn,
                                                                                const uint8_t *ocsp_response_raw,
                                                                                uint32_t ocsp_response_length) {

    if (!validator->validate_certificates || !validator->check_stapled_ocsp) {
        return S2N_CERT_OK;
    }

#if !S2N_OCSP_STAPLING_SUPPORTED
    /* Default to safety */
    return S2N_CERT_UNTRUSTED;
#else

    OCSP_RESPONSE *ocsp_response = NULL;
    OCSP_BASICRESP *basic_response = NULL;

    s2n_cert_validation_code ret_val = S2N_CERT_ERR_INVALID;

    if (!ocsp_response_raw) {
        return ret_val;
    }

    ocsp_response = d2i_OCSP_RESPONSE(NULL, &ocsp_response_raw, ocsp_response_length);

    if (!ocsp_response) {
        goto clean_up;
    }

    int ocsp_status = OCSP_response_status(ocsp_response);

    if (ocsp_status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
        goto clean_up;
    }

    basic_response = OCSP_response_get1_basic(ocsp_response);
    if (!basic_response) {
        goto clean_up;
    }

    int i;

    int certs_in_chain = sk_X509_num(validator->cert_chain);
    int certs_in_ocsp = sk_X509_num(basic_response->certs);

    if (certs_in_chain >= 2 && certs_in_ocsp >= 1) {
        X509 *responder = sk_X509_value(basic_response->certs, certs_in_ocsp - 1);

        /*check to see if one of the certs in the chain is an issuer of the cert in the ocsp response.*/
        /*if so it needs to be added to the OCSP verification chain.*/
        for (i = 0; i < certs_in_chain; i++) {
            X509 *issuer = sk_X509_value(validator->cert_chain, i);
            int issuer_value = X509_check_issued(issuer, responder);

            if (issuer_value == X509_V_OK) {
                if (!OCSP_basic_add1_cert(basic_response, issuer)) {
                    goto clean_up;
                }
            }
        }
    }

    int ocsp_verify_err = OCSP_basic_verify(basic_response, validator->cert_chain, validator->trust_store->trust_store, 0);
    /* do the crypto checks on the response.*/
    if (!ocsp_verify_err) {
        ret_val = S2N_CERT_ERR_EXPIRED;
        goto clean_up;
    }

    /* for each response check the timestamps and the status. */
    for (i = 0; i < OCSP_resp_count(basic_response); i++) {
        int status_reason;
        ASN1_GENERALIZEDTIME *revtime, *thisupd, *nextupd;

        OCSP_SINGLERESP *single_response = OCSP_resp_get0(basic_response, i);
        if (!single_response) {
            goto clean_up;
        }

        ocsp_status = OCSP_single_get0_status(single_response, &status_reason, &revtime,
                                              &thisupd, &nextupd);

        uint64_t this_update = 0;
        int thisupd_err = s2n_asn1_time_to_nano_since_epoch_ticks((const char *) thisupd->data,
                                                                  (uint32_t) thisupd->length, &this_update);

        uint64_t next_update = 0;
        int nextupd_err = s2n_asn1_time_to_nano_since_epoch_ticks((const char *) nextupd->data,
                                                                  (uint32_t) nextupd->length, &next_update);

        uint64_t current_time = 0;
        int current_time_err = conn->config->wall_clock(conn->config->data_for_sys_clock, &current_time);

        if (thisupd_err || nextupd_err || current_time_err) {
            ret_val = S2N_CERT_ERR_UNTRUSTED;
            goto clean_up;
        }

        if (current_time < this_update || current_time > next_update) {
            ret_val = S2N_CERT_ERR_EXPIRED;
            goto clean_up;
        }

        switch (ocsp_status) {
            case V_OCSP_CERTSTATUS_GOOD:
                break;

            case V_OCSP_CERTSTATUS_REVOKED:
                ret_val = S2N_CERT_ERR_REVOKED;
                goto clean_up;

            case V_OCSP_CERTSTATUS_UNKNOWN:
                goto clean_up;
            default:
                goto clean_up;
        }
    }

    ret_val = S2N_CERT_OK;

    clean_up:
    if (basic_response) {
        OCSP_BASICRESP_free(basic_response);
    }

    if (ocsp_response) {
        OCSP_RESPONSE_free(ocsp_response);
    }

    return ret_val;
#endif /* S2N_OCSP_STAPLING_SUPPORTED */
}

