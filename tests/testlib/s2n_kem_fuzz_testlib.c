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

int s2n_kem_recv_ciphertext_fuzz_test_init(const char *kat_file_path, struct s2n_kem_params *kem_params) {
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
    /* Because of the way BIKE1_L1_R1's decapsulation function is written, this test will not work for that KEM. */
    ENSURE_POSIX(kem_params->kem != &s2n_bike1_l1_r1, S2N_ERR_KEM_UNSUPPORTED_PARAMS);

    struct s2n_stuffer ciphertext = { 0 };
    GUARD(s2n_stuffer_growable_alloc(&ciphertext, 8192));
    GUARD(s2n_stuffer_write_bytes(&ciphertext, buf, len));

    if (s2n_kem_recv_ciphertext(&ciphertext, kem_params) != S2N_SUCCESS) {
        /* The PQ KEM functions are written in such a way that kem->decapsulate should
         * never fail, even if the ciphertext is not valid. If s2n_kem_recv_ciphertext
         * failed, we still want to check decapsulate. */
        if (kem_params->shared_secret.allocated == 0) {
            GUARD(s2n_alloc(&kem_params->shared_secret, kem_params->kem->shared_secret_key_length));
        }

        GUARD(kem_params->kem->decapsulate(kem_params->shared_secret.data, ciphertext.blob.data, kem_params->private_key.data));
    }

    GUARD(s2n_stuffer_free(&ciphertext));
    GUARD(s2n_free(&kem_params->shared_secret));

    return S2N_SUCCESS;
}
