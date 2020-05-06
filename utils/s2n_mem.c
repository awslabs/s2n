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

#define  _DEFAULT_SOURCE 1
#include <features.h>

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "error/s2n_errno.h"

#include "utils/s2n_blob.h"
#include "utils/s2n_mem.h"
#include "utils/s2n_safety.h"

static long page_size = 4096;
static bool initialized = false;

static int s2n_mem_init_impl(void);
static int s2n_mem_cleanup_impl(void);
static int s2n_mem_free_no_mlock_impl(void *ptr, uint32_t size);
static int s2n_mem_free_mlock_impl(void *ptr, uint32_t size);
static int s2n_mem_malloc_no_mlock_impl(void **ptr, uint32_t requested, uint32_t *allocated);
static int s2n_mem_malloc_mlock_impl(void **ptr, uint32_t requested, uint32_t *allocated);

static s2n_mem_init_callback s2n_mem_init_cb = s2n_mem_init_impl;
static s2n_mem_cleanup_callback s2n_mem_cleanup_cb = s2n_mem_cleanup_impl;
static s2n_mem_malloc_callback s2n_mem_malloc_cb = s2n_mem_malloc_mlock_impl;
static s2n_mem_free_callback s2n_mem_free_cb = s2n_mem_free_mlock_impl;

static int s2n_mem_init_impl(void)
{
    GUARD(page_size = sysconf(_SC_PAGESIZE));
    if (getenv("S2N_DONT_MLOCK")) {
        s2n_mem_malloc_cb = s2n_mem_malloc_no_mlock_impl;
        s2n_mem_free_cb = s2n_mem_free_no_mlock_impl;
    }
    return S2N_SUCCESS;
}

static int s2n_mem_cleanup_impl(void)
{
    page_size = 4096;
    s2n_mem_malloc_cb = s2n_mem_malloc_no_mlock_impl;
    s2n_mem_free_cb = s2n_mem_free_no_mlock_impl;
    return S2N_SUCCESS;
}

static int s2n_mem_free_mlock_impl(void *ptr, uint32_t size)
{
    int munlock_rc = munlock(ptr, size);
    free(ptr);
    GUARD(munlock_rc);

    return S2N_SUCCESS;
}

static int s2n_mem_free_no_mlock_impl(void *ptr, uint32_t size)
{
    free(ptr);

    return S2N_SUCCESS;
}

static int s2n_mem_malloc_mlock_impl(void **ptr, uint32_t requested, uint32_t *allocated)
{
    notnull_check(ptr);

    /* Page aligned allocation required for mlock */
    uint32_t allocate;

    GUARD(s2n_align_to(requested, page_size, &allocate));

    *ptr = NULL;
    S2N_ERROR_IF(posix_memalign(ptr, page_size, allocate) != 0, S2N_ERR_ALLOC);
    *allocated = allocate;

/*
** We disable MAD_DONTDUMP when fuzz-testing or using the address sanitizer because
** both need to be able to dump pages to function. It's how they map heap output.
*/
#if defined(MADV_DONTDUMP) && !defined(S2N_ADDRESS_SANITIZER) && !defined(S2N_FUZZ_TESTING)
    if (madvise(*ptr, *allocated, MADV_DONTDUMP) != 0) {
        GUARD(s2n_mem_free_no_mlock_impl(*ptr, *allocated));
        S2N_ERROR(S2N_ERR_MADVISE);
    }
#endif

    if (mlock(*ptr, *allocated) != 0) {
        /* When mlock fails, no memory will be locked, so we don't use munlock on free */
        GUARD(s2n_mem_free_no_mlock_impl(*ptr, *allocated));
        S2N_ERROR(S2N_ERR_MLOCK);
    }

    S2N_ERROR_IF(*ptr == NULL, S2N_ERR_ALLOC);

    return S2N_SUCCESS;
}

static int s2n_mem_malloc_no_mlock_impl(void **ptr, uint32_t requested, uint32_t *allocated)
{
    *ptr = malloc(requested);
    S2N_ERROR_IF(*ptr == NULL, S2N_ERR_ALLOC);
    *allocated = requested;

    return S2N_SUCCESS;
}

int s2n_mem_set_callbacks(s2n_mem_init_callback mem_init_callback, s2n_mem_cleanup_callback mem_cleanup_callback,
                          s2n_mem_malloc_callback mem_malloc_callback, s2n_mem_free_callback mem_free_callback)
{
    S2N_ERROR_IF(initialized == true, S2N_ERR_INITIALIZED);

    notnull_check(mem_init_callback);
    notnull_check(mem_cleanup_callback);
    notnull_check(mem_malloc_callback);
    notnull_check(mem_free_callback);

    s2n_mem_init_cb = mem_init_callback;
    s2n_mem_cleanup_cb = mem_cleanup_callback;
    s2n_mem_malloc_cb = mem_malloc_callback;
    s2n_mem_free_cb = mem_free_callback;

    return S2N_SUCCESS;
}

int s2n_alloc(struct s2n_blob *b, uint32_t size)
{
    S2N_ERROR_IF(initialized == false, S2N_ERR_NOT_INITIALIZED);
    notnull_check(b);
    const struct s2n_blob temp = {0};
    *b = temp;
    GUARD(s2n_realloc(b, size));
    return S2N_SUCCESS;
}

/* A blob is growable if it is either explicitly marked as such, or if it contains no data */
bool s2n_blob_is_growable(const struct s2n_blob* b)
{
  return b && (b->growable || (b->data == NULL && b->size == 0 && b->allocated == 0));
}

/* Tries to realloc the requested bytes.
 * If successful, updates *b.
 * If failed, *b remains unchanged
 */
int s2n_realloc(struct s2n_blob *b, uint32_t size)
{
    S2N_ERROR_IF(initialized == false, S2N_ERR_NOT_INITIALIZED);
    notnull_check(b);
    S2N_ERROR_IF(!s2n_blob_is_growable(b), S2N_ERR_RESIZE_STATIC_BLOB);
    if (size == 0) {
        return s2n_free(b);
    }

    /* blob already has space for the request */
    if (size <= b->allocated) {

        if (size < b->size) {
            /* Zero the existing blob memory before the we release it */
            struct s2n_blob slice = {0};
            GUARD(s2n_blob_slice(b, &slice, size, b->size - size));
            GUARD(s2n_blob_zero(&slice));
        }

        b->size = size;
        return S2N_SUCCESS;
    }

    struct s2n_blob new_memory = {.data = NULL, .size = size, .allocated = 0, .growable = 1};
    if(s2n_mem_malloc_cb((void **) &new_memory.data, new_memory.size, &new_memory.allocated) != 0) {
        S2N_ERROR_PRESERVE_ERRNO();
    }

    S2N_ERROR_IF(new_memory.allocated < new_memory.size, S2N_ERR_ALLOC);
    S2N_ERROR_IF(new_memory.data == NULL, S2N_ERR_ALLOC);

    if (b->size) {
        memcpy_check(new_memory.data, b->data, b->size);
        GUARD(s2n_free(b));
    }

    *b = new_memory;
    return S2N_SUCCESS;
}

int s2n_free_object(uint8_t **p_data, uint32_t size)
{
    S2N_ERROR_IF(initialized == false, S2N_ERR_NOT_INITIALIZED);
    notnull_check(p_data);

    if (*p_data == NULL) {
        return 0;
    }
    struct s2n_blob b = {.data = *p_data, .size = size, .growable = 1};

    /* s2n_free() will call free() even if it returns error (for a growable blob).
    ** This makes sure *p_data is not used after free() */
    *p_data = NULL;

    return s2n_free(&b);
}

int s2n_dup(struct s2n_blob *from, struct s2n_blob *to)
{
    S2N_ERROR_IF(initialized == false, S2N_ERR_NOT_INITIALIZED);
    eq_check(to->size, 0);
    eq_check(to->data, NULL);
    ne_check(from->size, 0);
    ne_check(from->data, NULL);

    GUARD(s2n_alloc(to, from->size));

    memcpy_check(to->data, from->data, to->size);

    return S2N_SUCCESS;
}

int s2n_mem_init(void)
{
    GUARD(s2n_mem_init_cb());

    initialized = true;

    return S2N_SUCCESS;
}

int s2n_mem_cleanup(void)
{
    S2N_ERROR_IF(initialized == false, S2N_ERR_NOT_INITIALIZED);
    GUARD(s2n_mem_cleanup_cb());

    initialized = false;

    return S2N_SUCCESS;
}

int s2n_free(struct s2n_blob *b)
{
    S2N_ERROR_IF(initialized == false, S2N_ERR_NOT_INITIALIZED);
    S2N_ERROR_IF(!s2n_blob_is_growable(b), S2N_ERR_FREE_STATIC_BLOB);

    /* To avoid memory leaks, still free the data even if we can't unlock / wipe it */
    int zero_rc = s2n_blob_zero(b);

    GUARD(s2n_mem_free_cb(b->data, b->allocated));

    *b = (struct s2n_blob) {0};

    GUARD(zero_rc);

    return S2N_SUCCESS;
}
