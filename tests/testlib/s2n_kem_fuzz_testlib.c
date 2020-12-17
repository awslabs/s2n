/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include "s2n_testlib.h"
#include "utils/s2n_safety.h"
#include "tls/s2n_kem.h"
#include "tests/testlib/s2n_nist_kats.h"
#include "pq-crypto/s2n_pq.h"

int s2n_kem_recv_ciphertext_fuzz_test_init(const char *kat_file_path, struct s2n_kem_params *kem_params) {
    notnull_check(kat_file_path);
    notnull_check(kem_params);
    notnull_check(kem_params->kem);

    GUARD(s2n_alloc(&kem_params->private_key, kem_params->kem->private_key_length));
    FILE *kat_file = fopen(kat_file_path, "r");
    notnull_check(kat_file);
    GUARD(ReadHex(kat_file, kem_params->private_key.data, kem_params->kem->private_key_length, "sk = "));
    fclose(kat_file);

    return S2N_SUCCESS;
}

int s2n_kem_recv_ciphertext_fuzz_test(const uint8_t *buf, size_t len, struct s2n_kem_params *kem_params) {
    notnull_check(buf);
    notnull_check(kem_params);
    notnull_check(kem_params->kem);

    DEFER_CLEANUP(struct s2n_stuffer ciphertext = { 0 }, s2n_stuffer_free);
    GUARD(s2n_stuffer_alloc(&ciphertext, len));
    GUARD(s2n_stuffer_write_bytes(&ciphertext, buf, len));

    /* Don't GUARD here; this will probably fail. We should fuzz this
     * top-level function regardless of whether or not PQ is enabled. */
    int recv_ciphertext_ret = s2n_kem_recv_ciphertext(&ciphertext, kem_params);

    /* The recv_ciphertext() function may fail, but as long as PQ is enabled,
     * it should never fail due to S2N_ERR_PQ_CRYPTO (the only exception is
     * BIKE1L1R1, which may fail due to a PQ_CRYPTO error because of the way
     * that KEM's decaps function is written). */
    if (s2n_pq_is_enabled() && recv_ciphertext_ret != S2N_SUCCESS && kem_params->kem != &s2n_bike1_l1_r1) {
            ne_check(s2n_errno, S2N_ERR_PQ_CRYPTO);
    }

    /* Shared secret may have been alloc'ed in recv_ciphertext */
    GUARD(s2n_free(&kem_params->shared_secret));

    return S2N_SUCCESS;
}

int s2n_kem_recv_public_key_fuzz_test(const uint8_t *buf, size_t len, struct s2n_kem_params *kem_params) {
    notnull_check(buf);
    notnull_check(kem_params);
    notnull_check(kem_params->kem);

    DEFER_CLEANUP(struct s2n_stuffer public_key = { 0 }, s2n_stuffer_free);
    GUARD(s2n_stuffer_alloc(&public_key, len));
    GUARD(s2n_stuffer_write_bytes(&public_key, buf, len));

    /* s2n_kem_recv_public_key performs only very basic checks, like ensuring
     * that the public key size is correct. If the received public key passes,
     * we continue by calling s2n_kem_send_ciphertext to attempt to use the key
     * for encryption. */
    if (s2n_kem_recv_public_key(&public_key, kem_params) == S2N_SUCCESS) {
        DEFER_CLEANUP(struct s2n_stuffer out = {0}, s2n_stuffer_free);
        GUARD(s2n_stuffer_growable_alloc(&out, 8192));
        int send_ct_ret = s2n_kem_send_ciphertext(&out, kem_params);

        /* The KEM encaps functions are written in such a way that
         * s2n_kem_send_ciphertext() should always succeed as long
         * as PQ is enabled, even if the previously received public
         * key is not valid. If PQ is not enabled, send_ciphertext()
         * should always fail because of a PQ crypto errno. */
        if (s2n_pq_is_enabled()) {
            eq_check(send_ct_ret, S2N_SUCCESS);
        } else {
            ne_check(send_ct_ret, S2N_SUCCESS);
            eq_check(s2n_errno, S2N_ERR_PQ_CRYPTO);
        }
    }

    /* Clean up */
    GUARD(s2n_kem_free(kem_params));

    return S2N_SUCCESS;
}
