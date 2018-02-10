/*
 * Copyright 2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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
#include "utils/s2n_array.h"
#include "utils/s2n_blob.h"
#include "utils/s2n_mem.h"
#include "utils/s2n_safety.h"

#define S2N_INITIAL_ARRAY_SIZE 16

static int s2n_array_embiggen(struct s2n_array *array, uint32_t capacity)
{
    struct s2n_blob mem;
    void *tmp = array->elements;

    GUARD(s2n_alloc(&mem, array->element_size * capacity));
    GUARD(s2n_blob_zero(&mem));

    size_t size = array->element_size * array->num_of_elements;

    array->capacity = capacity;
    array->elements = (void *) mem.data;

    /* Copy and free exisiting elements for non-empty array */
    if (size != 0) {
        memcpy_check(mem.data, (uint8_t *) tmp, size);
        mem.data = (void *) tmp;
        mem.size = size;
        GUARD(s2n_free(&mem));
    }

    return 0;
}

struct s2n_array *s2n_array_new(size_t element_size)
{
    struct s2n_blob mem;
    struct s2n_array *array;

    GUARD_PTR(s2n_alloc(&mem, sizeof(struct s2n_array)));

    array = (void *) mem.data;
    array->capacity = 0;
    array->num_of_elements = 0;
    array->element_size = element_size;
    array->elements = NULL;

    GUARD_PTR(s2n_array_embiggen(array, S2N_INITIAL_ARRAY_SIZE));

    return array;
}

void *s2n_array_add(struct s2n_array *array)
{
    void *element;

    if (array->num_of_elements >= array->capacity) {
        /* Embiggen the array */
        GUARD_PTR(s2n_array_embiggen(array, array->capacity * 2));
    }

    element = (uint8_t *) array->elements + array->element_size * array->num_of_elements;
    array->num_of_elements++;

    return element;
}

void *s2n_array_get(struct s2n_array *array, uint32_t index)
{
    void *element = NULL;

    if (index < array->num_of_elements) {
        element = (uint8_t *) array->elements + array->element_size * index;
    }

    return element;
}

int s2n_array_free(struct s2n_array *array)
{
    struct s2n_blob mem;

    /* Free the elements */
    mem.data = (void *) array->elements;
    mem.size = array->capacity * array->element_size;
    GUARD(s2n_free(&mem));

    /* And finally the array */
    mem.data = (void *) array;
    mem.size = sizeof(struct s2n_array);
    GUARD(s2n_free(&mem));

    return 0;
}
