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

#include "api/s2n.h"
#include "crypto/s2n_hash.h"

#include <cbmc_proof/cbmc_utils.h>
#include <cbmc_proof/proof_allocators.h>

void s2n_hash_digest_size_harness()
{
    /* Non-deterministic inputs. */
    s2n_hash_algorithm alg;
    size_t alg_size;
    uint8_t *out = bounded_malloc(alg_size);

    /* Operation under verification. */
    if(s2n_hash_digest_size(alg, out) == S2N_SUCCESS)
    {
        /* Post-conditions. */
        assert(IMPLIES(alg == S2N_HASH_NONE, *out == 0));
        assert(IMPLIES(alg == S2N_HASH_MD5, *out == MD5_DIGEST_LENGTH));
        assert(IMPLIES(alg == S2N_HASH_SHA1, *out == SHA_DIGEST_LENGTH));
        assert(IMPLIES(alg == S2N_HASH_SHA224, *out == SHA224_DIGEST_LENGTH));
        assert(IMPLIES(alg == S2N_HASH_SHA256, *out == SHA256_DIGEST_LENGTH));
        assert(IMPLIES(alg == S2N_HASH_SHA384, *out == SHA384_DIGEST_LENGTH));
        assert(IMPLIES(alg == S2N_HASH_SHA512, *out == SHA512_DIGEST_LENGTH));
        assert(IMPLIES(alg == S2N_HASH_MD5_SHA1, *out == MD5_DIGEST_LENGTH + SHA_DIGEST_LENGTH));
    }
}
