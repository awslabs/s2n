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
#include "stuffer/s2n_stuffer.h"

#include <cbmc_proof/cbmc_utils.h>
#include <cbmc_proof/proof_allocators.h>
#include <cbmc_proof/make_common_datastructures.h>

/*
 * The reason we don't have full coverage is that we only call s2n_realloc
 * with blob-data == NULL.
 */
void s2n_stuffer_resize_if_empty_harness() {
    /* Non-deterministic inputs. */
    struct s2n_stuffer *stuffer = cbmc_allocate_s2n_stuffer();
    __CPROVER_assume(s2n_stuffer_is_valid(stuffer));
    uint32_t size;

    /* Non-deterministically set initialized (in s2n_mem) to true. */
    if(nondet_bool()) {
        s2n_mem_init();
    }

    /* Save previous state. */
    struct s2n_stuffer old_stuffer = *stuffer;

    /* Operation under verification. */
    if (s2n_stuffer_resize_if_empty(stuffer, size) == S2N_SUCCESS) {
        if(size != 0 && old_stuffer.blob.data == NULL) {
            assert(stuffer->blob.growable == 1);
            assert(stuffer->blob.size == size);
            assert(stuffer->blob.allocated >= size);
        }
    } else {
        assert(stuffer->blob.size == old_stuffer.blob.size);
        assert(stuffer->write_cursor == old_stuffer.write_cursor);
        assert(stuffer->high_water_mark == old_stuffer.high_water_mark);
        assert(stuffer->alloced == old_stuffer.alloced);
        assert(stuffer->growable == old_stuffer.growable);
        assert(stuffer->tainted == old_stuffer.tainted);
    }

    /* Post-conditions. */
    assert(s2n_stuffer_is_valid(stuffer));
}
