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

#pragma once

#include "api/s2n.h"

#include <openssl/x509v3.h>

/** Return TRUE for trusted, FALSE for untrusted **/
typedef uint8_t (*verify_host) (const char *host_name, size_t host_name_len, void *data);
struct s2n_connection;

/**
 * Trust store simply contains the trust store each connection should validate certs against.
 * For most use cases, you only need one of these per application.
 */
struct s2n_x509_trust_store {
    X509_STORE *trust_store;
};

/**
 * You should have one instance of this per connection.
 */
struct s2n_x509_validator {
    struct s2n_x509_trust_store *trust_store;
    STACK_OF(X509) *cert_chain;

    uint8_t validate_certificates;
    uint8_t check_stapled_ocsp;
    verify_host verify_host_fn;
    void *validation_ctx;
};

/** Initialize the trust store to empty defaults (no allocations happen here) */
void s2n_x509_trust_store_init(struct s2n_x509_trust_store *store);

/** Returns TRUE if the trust store has certificates installed, FALSE otherwise */
uint8_t s2n_x509_trust_store_has_certs(struct s2n_x509_trust_store *store);

/** Initialize trust store from a CA file. This will allocate memory, and load each cert in the file into the trust store
 *  Returns 0 on success, or S2N error codes on failure. */
int s2n_x509_trust_store_from_ca_file(struct s2n_x509_trust_store *store, const char *ca_file);

/** Cleans up, and frees any underlying memory in the trust store. */
void s2n_x509_trust_store_cleanup(struct s2n_x509_trust_store *store);

/** Initialize the validator in unsafe mode. No validity checks for OCSP, host checks, or X.509 will be performed. */
int s2n_x509_validator_init_no_checks(struct s2n_x509_validator *validator);

/** Initialize the validator in safe mode. Will use trust store to validate x.509 cerficiates, ocsp responses, and will call
 *  the verify host callback to determine if a subject name or alternative name from the cert should be trusted.
 *  Returns 0 on success, and an S2N_ERR_* on failure.
 */
int s2n_x509_validator_init(struct s2n_x509_validator *validator, struct s2n_x509_trust_store *trust_store, uint8_t check_ocsp, verify_host verify_host_fn, void *verify_ctx);

/** Cleans up underlying memory and data members. Struct can be reused afterwards. */
void s2n_x509_validator_cleanup(struct s2n_x509_validator *validator);

/**
 * Validates a certificate chain against the configured trust store, in safe mode, in unsafe mode, it will find the public cert
 * and return it but not validate the certificates. Alternative Names and Subject Name will be passed to the host verification callback.
 * The verification callback will be possiblycalled multiple times depending on how many names are found.
 * If any of those calls return TRUE, that stage of the validation will continue, otherwise once all names are tried and none matched as
 * trusted, the chain will be considered UNTRUSTED
 */
s2n_cert_validation_code s2n_x509_validator_validate_cert_chain(struct s2n_x509_validator *validator, uint8_t *cert_chain_in,
                                            uint32_t cert_chain_len,
                                            struct s2n_cert_public_key *public_key_out);

/**
 * Validates an ocsp response against the most recent certificate chain. Also verifies the timestamps on the response.
 */
s2n_cert_validation_code s2n_x509_validator_validate_cert_stapled_ocsp_response(struct s2n_x509_validator *validator,
                                                                                const uint8_t *ocsp_response, size_t size,
                                                                                struct s2n_config *config);



