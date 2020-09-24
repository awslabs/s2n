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

/* Target Functions: s2n_kem_recv_public_key s2n_kem_encapsulate SIKE_P434_r2_crypto_kem_enc */

#include "tests/s2n_test.h"
#include "tests/testlib/s2n_nist_kats.h"
#include "tls/s2n_kem.h"
#include "utils/s2n_safety.h"
#include "utils/s2n_blob.h"

/* The valid_public_key in the corpus directory was generated by taking the first public
 * key (count = 0) from sike_r2.kat and prepending SIKE_P434_R2_PUBLIC_KEY_BYTES as two
 * hex-encoded bytes. This is how we would expect it to appear on the wire. */

static struct s2n_kem_params server_kem_params = { .kem = &s2n_sike_p434_r2 };

int s2n_fuzz_test(const uint8_t *buf, size_t len) {
    struct s2n_stuffer public_key = { 0 };
    GUARD(s2n_stuffer_growable_alloc(&public_key, 8192));
    GUARD(s2n_stuffer_write_bytes(&public_key, buf, len));

    /* Run the test, don't use GUARD since the memory needs to be cleaned up. */
    if (s2n_kem_recv_public_key(&public_key, &server_kem_params) == S2N_SUCCESS) {
        /* s2n_kem_recv_public_key performs only very basic validation on the public key,
         * like ensuring the length is correct. If that succeeds, we want to follow
         * up with s2n_kem_send_ciphertext, where we actually attempt to use the key
         * to perform encryption. */
        struct s2n_stuffer out = { 0 };
        GUARD(s2n_stuffer_growable_alloc(&out, 8192));

        /* The PQ KEM functions are written in such a way that s2n_kem_send_ciphertext
         * should always succeed, even if the public key is not valid. */
        GUARD(s2n_kem_send_ciphertext(&out, &server_kem_params));
        GUARD(s2n_stuffer_free(&out));
    }

    GUARD(s2n_stuffer_free(&public_key));
    GUARD(s2n_kem_free(&server_kem_params));

    return S2N_SUCCESS;
}

S2N_FUZZ_TARGET(NULL, s2n_fuzz_test, NULL)
