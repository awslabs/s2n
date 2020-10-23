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

/* Target Functions: s2n_kem_recv_ciphertext s2n_kem_decapsulate BIKE1_L1_R1_crypto_kem_dec */

#include "tests/s2n_test.h"
#include "tests/testlib/s2n_testlib.h"
#include "tls/s2n_kem.h"
#include "utils/s2n_safety.h"

#define KAT_FILE_NAME "../unit/kats/bike_r1.kat"

/* This fuzz test uses the first private key (count = 0) from tests/unit/kats/bike_r1.kat.
 * A valid ciphertext to provide to s2n_kem_recv_ciphertext (as it would have appeared on
 * the wire) was generated by taking the corresponding KAT ciphertext (count = 0) and
 * prepending BIKE1_L1_R1_CIPHERTEXT_BYTES as two hex-encoded bytes. */
static struct s2n_kem_params kem_params = { .kem = &s2n_bike1_l1_r1 };

int s2n_fuzz_init(int *argc, char **argv[]) {
    GUARD(s2n_kem_recv_ciphertext_fuzz_test_init(KAT_FILE_NAME, &kem_params));
    return S2N_SUCCESS;
}

int s2n_fuzz_test(const uint8_t *buf, size_t len) {
    GUARD(s2n_kem_recv_ciphertext_fuzz_test(buf, len, &kem_params));
    return S2N_SUCCESS;
}

S2N_FUZZ_TARGET(s2n_fuzz_init, s2n_fuzz_test, NULL)
