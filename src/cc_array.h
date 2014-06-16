/*
 * ccommon - a cache common library.
 * Copyright (C) 2013 Twitter, Inc.
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

#ifndef _NC_ARRAY_H_
#define _NC_ARRAY_H_

#include <stdint.h>
#include <stddef.h>

#include <cc_define.h>

typedef int (*array_compare_t)(const void *, const void *);
typedef rstatus_t (*array_each_t)(void *, void *);

struct array {
    uint32_t nalloc;    /* # allocated element */
    size_t   size;      /* element size */
    uint32_t nelem;     /* # element */
    void     *data;     /* element */
};


static inline uint32_t
array_nalloc(const struct array *arr)
{
    return arr->nalloc;
}

static inline size_t
array_size(const struct array *arr)
{
    return arr->size;
}

static inline uint32_t
array_nelem(const struct array *arr)
{
    return arr->nelem;
}

/* resets all fields in array to zero w/o freeing up any memory */
static inline void
array_reset(struct array *arr)
{
    arr->nalloc = 0;
    arr->size = 0;
    arr->nelem = 0;
    arr->data = NULL;
}

/* initialize arry of given size parameters and allocated data memory */
static inline void
array_data_assign(struct array *arr, uint32_t nalloc, size_t size, void *data)
{
    arr->nalloc = nalloc;
    arr->size = size;
    arr->nelem = 0;
    arr->data = data;
}

rstatus_t array_data_alloc(struct array *arr, uint32_t nalloc, size_t size);
void array_data_dealloc(struct array *arr);

rstatus_t array_alloc(struct array **arr, uint32_t nalloc, size_t size);
void array_dealloc(struct array **arr);

uint32_t array_idx(struct array *arr, void *elem);
void *array_push(struct array *arr);
void *array_get(struct array *arr, uint32_t idx);
void *array_last(struct array *arr);
void *array_pop(struct array *arr);
void array_sort(struct array *arr, array_compare_t compare);
uint32_t array_each(struct array *arr, array_each_t func, void *arg, err_t *err);

void array_init(uint32_t nelem);
void array_deinit(void);
#endif
