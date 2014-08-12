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

#ifndef _CC_ZIPMAP_H_
#define _CC_ZIPMAP_H_

#include <cc_array.h>
#include <cc_define.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct item;

typedef enum entry_flags {
    ENTRY_IS_NUMERIC = 1,
#if defined CC_CHAINED && CC_CHAINED == 1
    ENTRY_LAST_IN_NODE = 2,
#endif
} entry_flags_t;

typedef enum zmap_set_result {
    ZMAP_SET_OK,
    ZMAP_SET_NOT_FOUND,
    ZMAP_SET_OVERSIZED
} zmap_set_result_t;

typedef enum zmap_add_result {
    ZMAP_ADD_OK,
    ZMAP_ADD_NOT_FOUND,
    ZMAP_ADD_EXISTS,
    ZMAP_ADD_OVERSIZED
} zmap_add_result_t;

typedef enum zmap_replace_result {
    ZMAP_REPLACE_OK,
    ZMAP_REPLACE_NOT_FOUND,
    ZMAP_REPLACE_ENTRY_NOT_FOUND,
    ZMAP_REPLACE_OVERSIZED
} zmap_replace_result_t;

typedef enum zmap_delete_result {
    ZMAP_DELETE_OK,
    ZMAP_DELETE_NOT_FOUND,
    ZMAP_DELETE_ENTRY_NOT_FOUND
} zmap_delete_result_t;

typedef enum zmap_get_result {
    ZMAP_GET_OK,
    ZMAP_GET_NOT_FOUND,
    ZMAP_GET_ENTRY_NOT_FOUND
} zmap_get_result_t;

typedef enum zmap_exists_result {
    ZMAP_ENTRY_EXISTS,
    ZMAP_NOT_FOUND,
    ZMAP_ENTRY_NOT_FOUND
} zmap_exists_result_t;

typedef struct key_val_pair {
    void *key;
    void *val;
    uint8_t nkey;
    uint32_t nval;
} key_val_pair_t;

typedef struct buf {
    void *buf;
    uint8_t nbuf;
} buf_t;

typedef struct key_numeric_pair {
    uint64_t val;
    void *key;
    uint8_t nkey;
} key_numeric_pair_t;

typedef enum zmap_delta_result {
    ZMAP_DELTA_OK,
    ZMAP_DELTA_NOT_FOUND,
    ZMAP_DELTA_ENTRY_NOT_FOUND,
    ZMAP_DELTA_NON_NUMERIC,
    ZMAP_DELTA_OVERFLOW
} zmap_delta_result_t;

/* Header for zipmap */
struct zmap {
    uint32_t len; /* number of key-val pairs in the zipmap */
    uint8_t data[1];  /* beginning of key-val pairs */
};

/* Header for zipmap entry */
struct zmap_entry {
    uint32_t nval; /* val length, in bytes */
    uint8_t nkey; /* key length, in bytes */
    uint8_t npadding; /* padding length, in bytes */
    uint8_t flags; /* entry flags */
    uint8_t data[1]; /* entry data */
};

#define ZMAP_HDR_SIZE        offsetof(struct zmap, data)
#define ZMAP_ENTRY_HDR_SIZE  offsetof(struct zmap_entry, data)
#define ZMAP_PADDING_MAX     UCHAR_MAX

static inline bool
entry_is_numeric(struct zmap_entry *entry)
{
    return (entry->flags & ENTRY_IS_NUMERIC);
}

#if defined CC_CHAINED && CC_CHAINED == 1
static inline bool
entry_last_in_node(struct zmap_entry *entry)
{
    return (entry->flags & ENTRY_LAST_IN_NODE);
}
#endif

/* Get the location of the entry's key */
static inline uint8_t *
entry_key(struct zmap_entry *entry)
{
    return entry->data;
}

/* Get the location of the entry's val */
static inline uint8_t *
entry_val(struct zmap_entry *entry)
{
    return entry->data + entry->nkey;
}

/* Get the total size of an entry given nkey and nval */
static inline size_t
entry_ntotal(uint8_t nkey, uint32_t nval, uint8_t npadding)
{
    size_t ret = ZMAP_ENTRY_HDR_SIZE + nkey + nval + npadding;
    return ret;
}

/* Get the total size of an entry given an entry */
static inline size_t
entry_size(struct zmap_entry *entry)
{
    return entry_ntotal(entry->nkey, entry->nval, entry->npadding);
}

/* Get zmap given item */
struct zmap *item_to_zmap(struct item *it);

/* Initialize a zipmap */
void zmap_init(void *primary_key, uint8_t nkey);

/* Set key to value, creating key if it does not already exist. */
zmap_set_result_t zmap_set(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, void *val, uint32_t nval);

/* Set multiple key-val pairs. Creates keys if they do not already exist.
   Oversized or otherwise invalid requests are ignored. */
zmap_set_result_t zmap_set_multiple(void *pkey, uint8_t npkey, struct array *pairs);

/* Set key to value, where value is an integer */
zmap_set_result_t zmap_set_numeric(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, int64_t val);

/* Set multiple numeric key-val pairs. */
zmap_set_result_t zmap_set_multiple_numeric(void *pkey, uint8_t npkey, struct array *pairs);

/* Set the key to value, but only if it does not already exist. */
zmap_add_result_t zmap_add(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, void *val, uint32_t nval);

/* Set the key to numeric value, but only if it does not already exist. */
zmap_add_result_t zmap_add_numeric(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, int64_t val);

/* Set the key to value, but only if the key already exists */
zmap_replace_result_t zmap_replace(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, void *val, uint32_t nval);

/* Set the key to numeric value, but only if the key already exists */
zmap_replace_result_t zmap_replace_numeric(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, int64_t val);

/* Delete entry with given key from the zipmap */
zmap_delete_result_t zmap_delete(void *pkey, uint8_t npkey, void *skey, uint8_t nskey);

/* Search a key and retrieve the pointer and len of the associated value. */
zmap_get_result_t zmap_get(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, void **val, uint32_t *vlen);

/* Does this key exist in the zipmap? */
zmap_exists_result_t zmap_exists(void *pkey, uint8_t npkey, void *skey, uint8_t nskey);

/* Get all key-val pairs in the zipmap. len = -1 if the request fails. */
struct array zmap_get_all(void *pkey, uint8_t npkey);

/* Get all keys in the zipmap. len = -1 if the request fails. */
struct array zmap_get_keys(void *pkey, uint8_t npkey);

/* Get all vals in the zipmap. len = -1 if the request fails. */
struct array zmap_get_vals(void *pkey, uint8_t npkey);

/* Get values associated with the provided keys. len = -1 if the request fails;
   a buffer will be NULL if the key is not found. */
struct array zmap_get_multiple(void *pkey, uint8_t npkey, struct array *keys);

/* Get the number of elements in the zipmap. Returns -1 if the zipmap is not found */
int32_t zmap_len(void *pkey, uint8_t npkey);

/* apply a delta to a zipmap entry */
zmap_delta_result_t zmap_delta(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, int64_t delta);

#endif /* _CC_ZIPMAP_H_ */
