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

/*
 * Currently this version does not support expansion; this feature will be added
 * in when the rest of the memory manager is multithreaded
 */

#include <mem/cc_hash_table.h>
#include <mem/cc_items.h>
#include <mem/cc_queue.h>
#include <mem/cc_hash.h>
#include <mem/cc_settings.h>
#include <cc_mm.h>
#include <cc_debug.h>
#include <cc_log.h>

#include <string.h>

#define HASHSIZE(_n) (1UL << (_n))
#define HASHMASK(_n) (HASHSIZE(_n) - 1)

#define HASH_DEFAULT_MOVE_SIZE  1
#define HASH_DEFAULT_POWER      16

static struct item_slh *primary_hashtable;  /* primary (main) hash table */
static uint32_t nhash_item;                 /* # items in hash table */
static uint32_t hash_power;                 /* # buckets = 2^hash_power */

static struct item_slh *hash_table_create(uint32_t table_size);
static struct item_slh *hash_table_get_bucket(const char *key, size_t nkey);

rstatus_t
hash_table_init(void)
{
    uint32_t hashtable_size;

    /* init hash table values */
    primary_hashtable = NULL;
    hash_power = settings.hash_power > 0 ? settings.hash_power :
	HASH_DEFAULT_POWER;
    nhash_item = 0;
    hashtable_size = HASHSIZE(hash_power);

    /* allocate hash table */
    primary_hashtable = hash_table_create(hashtable_size);
    if(primary_hashtable == NULL) {
	/* Not enough memory to allocate hash table */
	return CC_ENOMEM;
    }

    return CC_OK;
}

void
hash_table_deinit(void)
{
    if(primary_hashtable != NULL) {
	cc_free(primary_hashtable);
    }
}

struct item *
hash_table_find(const char *key, size_t nkey)
{
    struct item_slh *bucket;
    struct item *it;
    uint32_t depth;

    ASSERT(key != NULL && nkey != 0);

    bucket = hash_table_get_bucket(key, nkey);

    /* search bucket for item */
    for(depth = 0, it = SLIST_FIRST(bucket);
	it != NULL;
	++depth, it = SLIST_NEXT(it, h_sle)) {
	if((nkey == it->nkey) && memcmp(key, item_key(it), nkey) == 0) {
	    return it;
	}
    }

    return NULL;
}

void
hash_table_insert(struct item *it)
{
    struct item_slh *bucket;

    ASSERT(hash_table_find(item_key(it), it->nkey) == NULL);

    /* insert item at the head of the bucket */
    bucket = hash_table_get_bucket(item_key(it), it->nkey);
    SLIST_INSERT_HEAD(bucket, it, h_sle);

    ++nhash_item;
}

void
hash_table_delete(const char *key, size_t nkey)
{
    struct item_slh *bucket;
    struct item *it, *prev;

    ASSERT(hash_table_find(key, nkey) != NULL);

    bucket = hash_table_get_bucket(key, nkey);

    /* search bucket for item to be deleted */
    for(prev = NULL, it = SLIST_FIRST(bucket);
	it != NULL;
	prev = it, it = SLIST_NEXT(it, h_sle)) {
	if((nkey == it->nkey) && memcmp(key, item_key(it), nkey) == 0) {
	    break;
	}
    }

    if(prev == NULL) {
	SLIST_REMOVE_HEAD(bucket, h_sle);
    } else {
	SLIST_REMOVE_AFTER(prev, h_sle);
    }

    --nhash_item;
}

/*
 * Allocate a hash table with the given size.
 */
static struct item_slh *
hash_table_create(uint32_t table_size)
{
    struct item_slh *table;
    uint32_t i;

    table = cc_alloc(sizeof(*table) * table_size);
    if(table == NULL) {
	/* Could not allocate enough memory for table */
	return NULL;
    }

    for(i = 0; i < table_size; ++i) {
	SLIST_INIT(&table[i]);
    }

    return table;
}

/*
 * Obtain the bucket that would contain the item with the given key
 */
static struct item_slh *
hash_table_get_bucket(const char *key, size_t nkey)
{
    return &primary_hashtable[hash(key, nkey, 0) & HASHMASK(hash_power)];
}
