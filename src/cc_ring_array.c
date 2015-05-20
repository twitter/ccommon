/*
 * ccommon - a cache common library.
 * Copyright (C) 2015 Twitter, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cc_ring_array.h>

#include <cc_bstring.h>
#include <cc_log.h>
#include <cc_mm.h>

#include <stdbool.h>

struct ring_array {
    size_t      elem_size;         /* element size */
    uint32_t    cap;               /* total capacity (# items stored + 1) */
    uint32_t    rpos;              /* read offset */
    uint32_t    wpos;              /* write offset */
    union {
        size_t  pad;               /* using a size_t member to force alignment at
                                      native word boundary */
        uint8_t data[1];           /* beginning of queue */
    };
};

#define RING_ARRAY_HDR_SIZE   offsetof(struct ring_array, data)

static inline bool
ring_array_empty(uint32_t rpos, uint32_t wpos)
{
    return rpos == wpos;
}

static inline bool
ring_array_full(uint32_t rpos, uint32_t wpos, uint32_t cap)
{
    return wpos + 1 == rpos || (wpos == cap && rpos == 0);
}

rstatus_t
ring_array_push(const void *elem, struct ring_array *arr)
{
    /**
     * Take snapshot of rpos, since another thread might be popping. Note: other
     * members of arr do not need to be saved because we assume the other thread
     * only pops and does not push; in other words, only one thread updates
     * either rpos or wpos.
     */
    uint32_t new_wpos;
    uint32_t rpos = __atomic_load_n(&(arr->rpos), __ATOMIC_RELAXED);

    if (ring_array_full(rpos, arr->wpos, arr->cap)) {
        log_debug("Could not push to ring queue %p; queue is full", arr);
        return CC_ERROR;
    }

    cc_memcpy(arr->data + (arr->elem_size * arr->wpos), elem, arr->elem_size);

    /* update wpos atomically */
    new_wpos = (arr->wpos + 1) % arr->cap;
    __atomic_store_n(&(arr->wpos), new_wpos, __ATOMIC_RELAXED);

    return CC_OK;
}

rstatus_t
ring_array_pop(void *elem, struct ring_array *arr)
{
    /* take snapshot of wpos, since another thread might be pushing */
    uint32_t new_rpos;
    uint32_t wpos = __atomic_load_n(&(arr->wpos), __ATOMIC_RELAXED);

    if (ring_array_empty(arr->rpos, wpos)) {
        log_debug("Could not pop from ring queue %p; queue is empty", arr);
        return CC_ERROR;
    }

    cc_memcpy(elem, arr->data + (arr->elem_size * arr->rpos), arr->elem_size);

    /* update rpos atomically */
    new_rpos = (arr->rpos + 1) % arr->cap;
    __atomic_store_n(&(arr->rpos), new_rpos, __ATOMIC_RELAXED);

    return CC_OK;
}

struct ring_array *
ring_array_create(size_t elem_size, uint32_t cap)
{
    struct ring_array *arr;

    arr = cc_alloc(RING_ARRAY_HDR_SIZE + elem_size * (cap + 1));

    if (arr == NULL) {
        log_error("Could not allocate memory for ring buffer cap %u "
                  "elem_size %u", cap, elem_size);
        return NULL;
    }

    arr->elem_size = elem_size;
    arr->cap = cap;
    arr->rpos = arr->wpos = 0;
    return arr;
}

void
ring_array_destroy(struct ring_array *arr)
{
    log_verb("destroying ring queue %p and freeing memory");
    cc_free(arr);
}
