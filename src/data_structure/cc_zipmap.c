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

#include <data_structure/cc_zipmap.h>

#include <mem/cc_items.h>
#include <mem/cc_mem_interface.h>
#include <mem/cc_slabs.h>
#include <cc_debug.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <cc_string.h>

#define ZMAP_FOREACH(_map, _entry, _entry_num, _item_ptr)            \
    for((_entry_num) = 0, (_entry) = zmap_get_entry_ptr(_map);       \
        (_entry_num) < (_map)->len;                                  \
	++(_entry_num), zmap_advance_entry(&(_entry), &(_item_ptr)))

rstatus_t set_pair(void *pair, void *pkey);
rstatus_t set_numeric_pair(void *pair, void *pkey);

static void *zmap_get_entry_ptr(struct zmap *zmap);
static struct zmap_entry *zmap_entry_next(struct zmap_entry *entry);
static struct zmap_entry *zmap_lookup_raw(struct item *it, struct zmap *zmap, void *key, uint8_t nkey);
static struct zmap_entry *zmap_lookup_with_node(struct item *it, struct zmap *zmap, void *key, uint8_t nkey, struct item **node);
static void zmap_add_raw(struct item *it, struct zmap *zmap, void *skey, uint8_t nskey, void *val, uint32_t nval, uint8_t flags);
static void zmap_item_hexdump(struct item *it);
static void zmap_delete_raw(struct item *it, struct zmap *zmap, struct zmap_entry *zmap_entry, struct item *node);
static size_t zmap_new_entry_size(uint8_t nskey, uint32_t nval);
static void zmap_set_raw(struct item *it, struct zmap *zmap, void *skey, uint8_t nskey, void *val, uint32_t nval, uint8_t flags);
static void zmap_replace_raw(struct item *it, struct zmap *zmap, struct zmap_entry *entry, void *val, uint32_t nval, uint8_t flags, struct item *node);
static bool zmap_item_append(struct item *it, void *new_item_buffer, size_t entry_size);
static void zmap_advance_entry(struct zmap_entry **entry, struct item **it);
static bool zmap_check_size(uint8_t npkey, uint8_t nskey, uint32_t nval);

#if defined CC_CHAINED && CC_CHAINED == 1
static void zmap_realloc_from_tail(struct item *it, struct item *node);
static struct zmap_entry *zmap_get_prev_entry(struct item *node, struct zmap_entry *entry, bool head);
#endif

struct zmap *
item_to_zmap(struct item *it)
{
    if(it == NULL) {
	return NULL;
    }

    return (struct zmap *)item_data(it);
}

/*
 * Current implementation overwrites any item already in the cache with the same
 * primary key
 */
void
zmap_init(void *primary_key, uint8_t nkey)
{
    struct zmap header;
    header.len = 0;

    log_debug(LOG_VERB, "zmap header size: %d zmap entry header size: %d",
	       ZMAP_HDR_SIZE, ZMAP_ENTRY_HDR_SIZE);

    store_key(primary_key, nkey, &header, ZMAP_HDR_SIZE);
}

zmap_set_result_t
zmap_set(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, void *val,
	 uint32_t nval)
{
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_SET_NOT_FOUND;
    }

    if(!zmap_check_size(npkey, nskey, nval)) {
	/* set request is oversized */
	item_remove(it);
	return ZMAP_SET_OVERSIZED;
    }

    zmap_set_raw(it, zmap, skey, nskey, val, nval, 0);
    item_remove(it);
    return ZMAP_SET_OK;
}

rstatus_t
set_pair(void *pair, void *pkey)
{
    if(zmap_check_size(((buf_t *)pkey)->nbuf, ((key_val_pair_t *)pair)->nkey,
		       ((key_val_pair_t *)pair)->nval)) {
	struct item *it;
	struct zmap *zmap;

	it = item_get(((buf_t *)pkey)->buf, ((buf_t *)pkey)->nbuf);
	zmap = item_to_zmap(it);

	if(zmap == NULL) {
	    /* zmap not in cache */
	    return CC_ERROR;
	}

	zmap_set_raw(it, zmap, ((key_val_pair_t *)pair)->key,
		     ((key_val_pair_t *)pair)->nkey, ((key_val_pair_t *)pair)->val,
		     ((key_val_pair_t *)pair)->nval, 0);

	item_remove(it);
    }
    return CC_OK;
}

zmap_set_result_t
zmap_set_multiple(void *pkey, uint8_t npkey, struct array *pairs)
{
    uint32_t status;
    buf_t key;
    err_t err;

    key.buf = pkey;
    key.nbuf = npkey;

    status = array_each(pairs, &set_pair, &key, &err);

    if(err == CC_ERROR) {
	return ZMAP_SET_NOT_FOUND;
    }

    return ZMAP_SET_OK;
}

zmap_set_result_t
zmap_set_numeric(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, int64_t val)
{
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_SET_NOT_FOUND;
    }

    /* If even a 64 bit integer is too large to store, the cache is almost
       certainly incorrectly configured */
    ASSERT(zmap_check_size(npkey, nskey, sizeof(int64_t)));

    zmap_set_raw(it, zmap, skey, nskey, &val, sizeof(int64_t), ENTRY_IS_NUMERIC);
    item_remove(it);
    return ZMAP_SET_OK;
}

rstatus_t
set_numeric_pair(void *pair, void *pkey)
{
    struct item *it;
    struct zmap *zmap;

    ASSERT(zmap_check_size((buf_t)pkey->nbuf, (key_numeric_pair_t)pair->nkey,
			   sizeof(int64_t)));

    it = item_get(((buf_t *)pkey)->buf, ((buf_t *)pkey)->nbuf);
    zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return CC_ERROR;
    }

    zmap_set_raw(it, zmap, ((key_numeric_pair_t *)pair)->key,
		 ((key_numeric_pair_t *)pair)->nkey,
		 &(((key_numeric_pair_t *)pair)->val),
		 sizeof(int64_t), ENTRY_IS_NUMERIC);

    item_remove(it);

    return CC_OK;
}

zmap_set_result_t
zmap_set_multiple_numeric(void *pkey, uint8_t npkey, struct array *pairs)
{
    uint32_t status;
    buf_t key;
    err_t err;

    key.buf = pkey;
    key.nbuf = npkey;

    status = array_each(pairs, &set_numeric_pair, &key, &err);

    if(err == CC_ERROR) {
	return ZMAP_SET_NOT_FOUND;
    }

    return ZMAP_SET_OK;
}

zmap_add_result_t
zmap_add(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, void *val,
	 uint32_t nval)
{
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_ADD_NOT_FOUND;
    }

    if(!zmap_check_size(npkey, nskey, nval)) {
	/* set request is oversized */
	item_remove(it);
	return ZMAP_ADD_OVERSIZED;
    }

    if(zmap_lookup_raw(it, zmap, skey, nskey) != NULL) {
	/* key already exists in zipmap */
	item_remove(it);
	return ZMAP_ADD_EXISTS;
    }

    zmap_add_raw(it, zmap, skey, nskey, val, nval, 0);
    item_remove(it);
    return ZMAP_ADD_OK;
}

zmap_add_result_t
zmap_add_numeric(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, int64_t val)
{
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_ADD_NOT_FOUND;
    }

    /* If even a 64 bit integer is too large to store, the cache is almost
       certainly incorrectly configured */
    ASSERT(zmap_check_size(npkey, nskey, sizeof(int64_t)));

    if(zmap_lookup_raw(it, zmap, skey, nskey) != NULL) {
	/* key already exists in zipmap */
	item_remove(it);
	return ZMAP_ADD_EXISTS;
    }

    zmap_add_raw(it, zmap, skey, nskey, &val, sizeof(int64_t), ENTRY_IS_NUMERIC);
    item_remove(it);
    return ZMAP_ADD_OK;
}

zmap_replace_result_t
zmap_replace(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, void *val,
	     uint32_t nval)
{
    struct zmap_entry *entry;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);
    struct item *node;

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_REPLACE_NOT_FOUND;
    }

    if(!zmap_check_size(npkey, nskey, nval)) {
	/* set request is oversized */
	item_remove(it);
	return ZMAP_REPLACE_OVERSIZED;
    }

    if((entry = zmap_lookup_with_node(it, zmap, skey, nskey, &node)) == NULL) {
	/* key does not exist in zipmap */
	item_remove(it);
	return ZMAP_REPLACE_ENTRY_NOT_FOUND;
    }

    zmap_replace_raw(it, zmap, entry, val, nval, 0, node);
    item_remove(it);
    return ZMAP_REPLACE_OK;
}

zmap_replace_result_t
zmap_replace_numeric(void *pkey, uint8_t npkey, void *skey, uint8_t nskey,
		     int64_t val)
{
    struct zmap_entry *entry;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);
    struct item *node;

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_REPLACE_NOT_FOUND;
    }

    /* If even a 64 bit integer is too large to store, the cache is almost
       certainly incorrectly configured */
    ASSERT(zmap_check_size(npkey, nskey, sizeof(int64_t)));

    if((entry = zmap_lookup_with_node(it, zmap, skey, nskey, &node)) == NULL) {
	/* key does not exist in zipmap */
	item_remove(it);
	return ZMAP_REPLACE_ENTRY_NOT_FOUND;
    }

    zmap_replace_raw(it, zmap, entry, &val, sizeof(int64_t),
		     ENTRY_IS_NUMERIC, node);
    item_remove(it);
    return ZMAP_REPLACE_OK;
}

zmap_delete_result_t
zmap_delete(void *pkey, uint8_t npkey, void *skey, uint8_t nskey)
{
    struct zmap_entry *entry;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);
    struct item *node;

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_DELETE_NOT_FOUND;
    }

    entry = zmap_lookup_with_node(it, zmap, skey, nskey, &node);

    if(entry == NULL) {
	/* zmap entry is not in the zmap */
	item_remove(it);
	return ZMAP_DELETE_ENTRY_NOT_FOUND;
    }

    zmap_delete_raw(it, zmap, entry, node);
    item_remove(it);
    return ZMAP_DELETE_OK;
}

zmap_get_result_t
zmap_get(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, void **val,
	 uint32_t *vlen)
{
    struct zmap_entry *entry;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_GET_NOT_FOUND;
    }

    if((entry = zmap_lookup_raw(it, zmap, skey, nskey)) == NULL) {
	/* zmap entry is not in the zmap */
	item_remove(it);
	return ZMAP_GET_ENTRY_NOT_FOUND;
    }

    *vlen = entry->nval;
    *val = entry_val(entry);
    item_remove(it);
    return ZMAP_GET_OK;
}

zmap_exists_result_t
zmap_exists(void *pkey, uint8_t npkey, void *skey, uint8_t nskey)
{
    struct zmap_entry *entry;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_NOT_FOUND;
    }

    entry = zmap_lookup_raw(it, zmap, skey, nskey);

    item_remove(it);

    if(entry == NULL) {
	/* zmap entry is not in the zmap */
	return ZMAP_ENTRY_NOT_FOUND;
    }

    return ZMAP_ENTRY_EXISTS;
}

struct array
zmap_get_all(void *pkey, uint8_t npkey)
{
    struct zmap_entry *iter;
    uint32_t i;
    struct array ret;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);
    struct item *it_iter = it;

    if(zmap == NULL) {
	/* zmap not in cache */
	ret.data = NULL;
	return ret;
    }

    /* Allocate space for the return value */
    if(array_data_alloc(&ret, zmap->len, sizeof(key_val_pair_t)) != CC_OK) {
	/* Could not allocate enough memory */
	log_debug(LOG_WARN, "Could not allocate enough memory to get all pairs!");
	item_remove(it);
	return ret;
    }

    ret.nelem = zmap->len;

    /* Copy locations and sizes of key/val buffers */
    ZMAP_FOREACH(zmap, iter, i, it_iter) {
	((key_val_pair_t *)(ret.data) + i)->key = entry_key(iter);
	((key_val_pair_t *)(ret.data) + i)->val = entry_val(iter);
	((key_val_pair_t *)(ret.data) + i)->nkey = iter->nkey;
	((key_val_pair_t *)(ret.data) + i)->nval = iter->nval;
    }

    item_remove(it);
    return ret;
}

struct array
zmap_get_keys(void *pkey, uint8_t npkey)
{
    struct zmap_entry *iter;
    uint32_t i;
    struct array ret;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);
    struct item *it_iter = it;

    if(zmap == NULL) {
	/* zmap not in cache */
	ret.data = NULL;
	return ret;
    }

    /* Allocate space for the return value */
    if(array_data_alloc(&ret, zmap->len, sizeof(buf_t)) != CC_OK) {
	/* Could not allocate enough memory */
	log_debug(LOG_WARN, "Could not allocate enough memory to get all keys!");
	item_remove(it);
	return ret;
    }

    ret.nelem = zmap->len;

    /* Copy over keys */
    ZMAP_FOREACH(zmap, iter, i, it_iter) {
	((buf_t *)(ret.data) + i)->buf = entry_key(iter);
	((buf_t *)(ret.data) + i)->nbuf = iter->nkey;
    }

    item_remove(it);
    return ret;
}

struct array
zmap_get_vals(void *pkey, uint8_t npkey)
{
    struct zmap_entry *iter;
    uint32_t i;
    struct array ret;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);
    struct item *it_iter = it;

    if(zmap == NULL) {
	/* zmap not in cache */
	ret.data = NULL;
	return ret;
    }

    /* Allocate space for the return value */
    if(array_data_alloc(&ret, zmap->len, sizeof(buf_t)) != CC_OK) {
	/* Could not allocate enough memory */
	log_debug(LOG_WARN, "Could not allocate enough memory to get all keys!");
	item_remove(it);
	return ret;
    }

    ret.nelem = zmap->len;

    /* Copy over vals */
    ZMAP_FOREACH(zmap, iter, i, it_iter) {
	((buf_t *)(ret.data) + i)->buf = entry_val(iter);
	((buf_t *)(ret.data) + i)->nbuf = iter->nval;
    }

    item_remove(it);
    return ret;
}

struct array
zmap_get_multiple(void *pkey, uint8_t npkey, struct array *keys)
{
    struct array ret;
    uint32_t i;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	ret.data = NULL;
	return ret;
    }

    /* Allocate space for the return value */
    if(array_data_alloc(&ret, keys->nelem, sizeof(buf_t)) != CC_OK) {
	/* Could not allocate enough memory */
	log_debug(LOG_WARN, "Could not allocate enough memory to get all vals!");
	item_remove(it);
	return ret;
    }

    ret.nelem = keys->nelem;

    /* Search for each value */
    for(i = 0; i < keys->nelem; ++i) {
	struct zmap_entry *entry =
	    zmap_lookup_raw(it, zmap, ((buf_t *)(keys->data) + i)->buf,
			    ((buf_t *)(keys->data) + i)->nbuf);

	if(entry == NULL) {
	    ((buf_t *)(ret.data) + i)->buf = NULL;
	} else {
	    ((buf_t *)(ret.data) + i)->buf = entry_val(entry);
	    ((buf_t *)(ret.data) + i)->nbuf = entry->nval;
	}
    }

    return ret;
}

int32_t
zmap_len(void *pkey, uint8_t npkey)
{
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return -1;
    }

    item_remove(it);

    return zmap->len;
}

zmap_delta_result_t
zmap_delta(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, int64_t delta)
{
    struct zmap_entry *entry;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap is not in cache */
	return ZMAP_DELTA_NOT_FOUND;
    }

    entry = zmap_lookup_raw(it, zmap, skey, nskey);

    if(entry == NULL) {
	/* zmap entry is not in zmap */
	item_remove(it);
	return ZMAP_DELTA_ENTRY_NOT_FOUND;
    }

    if(!entry_is_numeric(entry)) {
	/* entry is not numeric */
	item_remove(it);
	return ZMAP_DELTA_NON_NUMERIC;
    }

    if((delta > 0 && delta + *(int64_t *)entry_val(entry) < delta)
       || (delta < 0 && delta + *(int64_t *)entry_val(entry) > delta)) {
	/* delta will overflow */
	item_remove(it);
	return ZMAP_DELTA_OVERFLOW;
    }

    *(int64_t *)entry_val(entry) += delta;
    item_remove(it);
    return ZMAP_DELTA_OK;
}

/* Get the location of the first entry in the zmap. Returns NULL if there are no
   entries */
static void *
zmap_get_entry_ptr(struct zmap *zmap)
{
    return (zmap->len == 0) ? NULL : zmap->end;
}

/* Get the next entry in the zmap */
static struct zmap_entry *
zmap_entry_next(struct zmap_entry *entry)
{
    ASSERT(entry != NULL);
    return (struct zmap_entry *)((char *)entry + entry_size(entry));
}

/* Find the zipmap entry with the given key. returns NULL if doesn't exist */
static struct zmap_entry *
zmap_lookup_raw(struct item *it, struct zmap *zmap, void *key, uint8_t nkey)
{
    struct zmap_entry *iter;
    uint32_t i;

    ASSERT(zmap != NULL);

    ZMAP_FOREACH(zmap, iter, i, it) {
	if(nkey == iter->nkey && cc_memcmp(key, entry_key(iter), nkey) == 0) {
	    /* Match found */
	    return iter;
	}
    }

    return NULL;
}

static struct zmap_entry *
zmap_lookup_with_node(struct item *it, struct zmap *zmap, void *key,
		      uint8_t nkey, struct item **node)
{
    struct zmap_entry *iter;
    uint32_t i;

    ASSERT(zmap != NULL);
    ASSERT(node != NULL);

    *node = it;

    ZMAP_FOREACH(zmap, iter, i, *node) {
	if(nkey == iter->nkey && cc_memcmp(key, entry_key(iter), nkey) == 0) {
	    /* Match found */
	    return iter;
	}
    }

    return NULL;
}

/* Adds the given key/val to the zipmap */
static void
zmap_add_raw(struct item *it, struct zmap *zmap, void *skey, uint8_t nskey,
	     void *val, uint32_t nval, uint8_t flags)
{
    struct zmap_entry new_entry_header;
    size_t entry_size = zmap_new_entry_size(nskey, nval);
    void *new_item_buffer;
    char it_key[UCHAR_MAX];
    uint8_t it_nkey;
#if defined CC_CHAINED && CC_CHAINED == 1
    uint32_t num_nodes_before;
#endif

    ASSERT(it != NULL);
    it_nkey = it->nkey;
    cc_memcpy(it_key, item_key(it), it_nkey);
#if defined CC_CHAINED && CC_CHAINED == 1
    num_nodes_before = item_num_nodes(it);
#endif

    /* TODO: switch over from cc_alloc to using a preallocated memory pool */
    new_item_buffer = cc_alloc(entry_size);
    if(new_item_buffer == NULL) {
	/* Item cannot be added because not enough memory */
	log_debug(LOG_WARN, "Cannot add item; not enough memory!");
	return;
    }

    new_entry_header.nkey = nskey;
    new_entry_header.nval = nval;
    new_entry_header.npadding = entry_size - ZMAP_ENTRY_HDR_SIZE - nskey - nval;
#if defined CC_CHAINED && CC_CHAINED == 1
    new_entry_header.flags = flags | ENTRY_LAST_IN_NODE;
#else
    new_entry_header.flags = flags;
#endif

    /* Form new entry by copying over data */
    cc_memcpy(new_item_buffer, &new_entry_header, ZMAP_ENTRY_HDR_SIZE);
    cc_memcpy(new_item_buffer + ZMAP_ENTRY_HDR_SIZE, skey, nskey);
    cc_memcpy(new_item_buffer + ZMAP_ENTRY_HDR_SIZE + nskey, val, nval);

    if(zmap_item_append(it, new_item_buffer, entry_size)) {
	/* entry successfully added */

	/* After append, it and zmap pointers are invalidated since append may cause
	   item and zmap to be reallocated */
	it = item_get(it_key, it_nkey);
	zmap = item_to_zmap(it);

	++(zmap->len);

#if defined CC_CHAINED && CC_CHAINED == 1
	if(item_num_nodes(it) == num_nodes_before && zmap->len != 1) {
	    /* Need to unflag second to last entry */
	    struct item *tail = item_tail(it);
	    struct zmap_entry *iter = (num_nodes_before == 1) ?
		zmap_get_entry_ptr(zmap) : (struct zmap_entry *)item_data(tail);

	    for(; !entry_last_in_node(iter); iter = zmap_entry_next(iter));

	    iter->flags &= ~ENTRY_LAST_IN_NODE;
	}
#endif
	item_remove(it);
    }

    cc_free(new_item_buffer);
}

static void
zmap_item_hexdump(struct item *it)
{
#if defined CC_CHAINED && CC_CHAINED == 1
    uint32_t i;

    for(i = 1; it != NULL; ++i, it = it->next_node) {
	log_debug(LOG_DEBUG, "node %u addr %p", i, it);
	log_hexdump(LOG_DEBUG, it, 400, "");
	log_hexdump(LOG_DEBUG, (char *)it + 400, 400, "");
	log_hexdump(LOG_DEBUG, (char *)it + 800, 400, "");
    }
#else
    log_debug(LOG_DEBUG, "addr %p", it);
    log_hexdump(LOG_DEBUG, it, 400, "");
    log_hexdump(LOG_DEBUG, (char *)it + 400, 400, "");
    log_hexdump(LOG_DEBUG, (char *)it + 800, 400, "");
#endif
}

static void
zmap_delete_raw(struct item *it, struct zmap *zmap, struct zmap_entry *entry,
		struct item *node)
{
    struct zmap_entry *iter;

    ASSERT(zmap != NULL);
    ASSERT(zmap_entry != NULL);
    ASSERT(zmap->len > 0);

#if defined CC_CHAINED && CC_CHAINED == 1
    uint32_t deleted_size = entry_size(entry);

    if(entry_last_in_node(entry)) {
	/* Entry is last in its node */
	struct zmap_entry *new_last;
	new_last = zmap_get_prev_entry(node, entry, node == it);

	if(new_last == NULL) {
	    /* entry to be deleted is the only one in its node */
	    if(zmap->len > 1 && node != it) {
		/* entry's node is not head */
		/* node is no longer needed, remove it */
		item_remove_node(it, node);
	    } else if(zmap->len > 1 && node == it) {
		/* Node is head */
		/* Removing the only entry in the first node; need to copy over
		   as much of last node's data as will fit. Although this is
		   somewhet inefficient, it is a rare case. */
		it->nbyte = ZMAP_HDR_SIZE;

		zmap_realloc_from_tail(it, node);

		ASSERT(it->nbyte > ZMAP_HDR_SIZE);
	    } else {
		/* Node is the only one in the zipmap */
		/* len == 1, removing the only entry in the map */

		it->nbyte = ZMAP_HDR_SIZE;
	    }
	} else {
	    /* Entry is not the only one in its node; simply set the second to
	       last entry as the last */

	    new_last->flags |= ENTRY_LAST_IN_NODE;

	    node->nbyte -= deleted_size;

	    zmap_realloc_from_tail(it, node);
	}
    } else {
	/* Entry is located in the middle or is first in the node */

	/* Shift everything down */
	uint32_t amt_to_move = 0;

	/* Calculate amount needed to shift */
	for(iter = entry; !entry_last_in_node(iter);) {
	    iter = zmap_entry_next(iter);

	    amt_to_move += entry_size(iter);
	}

	/* Move entries down */
	cc_memmove(entry, zmap_entry_next(entry), amt_to_move);

	/* adjust nbyte */
	node->nbyte -= deleted_size;

	zmap_realloc_from_tail(it, node);
    }
    --(zmap->len);
#else
    /* Shift everything down */
    bool found_deleted = false;
    uint32_t i, amt_to_move = 0;
    struct item *it_iter;

    /* Calculate amount needed to shift */
    ZMAP_FOREACH(zmap, iter, i, it_iter) {
	if(found_deleted) {
	    amt_to_move += entry_size(iter);
	}

	if(iter == entry) {
	    found_deleted = true;
	}
    }

    /* Adjust item size */
    it->nbyte -= entry_size(entry);

    cc_memmove(entry, zmap_entry_next(entry), amt_to_move);

    /* Adjust len */
    --(zmap->len);
#endif
}

static size_t
zmap_new_entry_size(uint8_t nskey, uint32_t nval)
{
    size_t ret = ZMAP_ENTRY_HDR_SIZE + nskey + nval;

    /* round ret up to the next highest word */
    if(ret % sizeof(uint32_t) != 0) {
	ret += (sizeof(uint32_t) - (ret % sizeof(uint32_t)));
    }

    return ret;
}

static void
zmap_set_raw(struct item *it, struct zmap *zmap, void *skey, uint8_t nskey,
	     void *val, uint32_t nval, uint8_t flags)
{
    struct zmap_entry *entry;
    struct item *node;

    ASSERT(zmap != NULL);

    if((entry = zmap_lookup_with_node(it, zmap, skey, nskey, &node)) != NULL) {
	/* key already exists in zipmap, overwrite its value */
	zmap_replace_raw(it, zmap, entry, val, nval, flags, node);
    } else {
	/* key does not exist in zipmap, add it */
	zmap_add_raw(it, zmap, skey, nskey, val, nval, flags);
    }
}

static void
zmap_replace_raw(struct item *it, struct zmap *zmap, struct zmap_entry *entry,
		 void *val, uint32_t nval, uint8_t flags, struct item *node)
{
    ASSERT(it != NULL);
    if(entry_size(entry) >= entry_ntotal(entry->nkey, nval, 0)
       && entry_size(entry) <= entry_ntotal(entry->nkey, nval, ZMAP_PADDING_MAX)) {
	/* existing entry is a suitable size to contain the new entry */

	/* adjust header */
	entry->npadding = entry_size(entry) - entry_ntotal(entry->nkey, nval, 0);
	entry->nval = nval;
	entry->flags = flags;

	/* Copy over value */
	cc_memcpy(entry_val(entry), val, nval);
    } else {
	/* existing entry is not large enough to contain new entry, need to
	   delete and replace */
	char entry_key_cpy[UCHAR_MAX];
	uint8_t entry_nkey;

	entry_nkey = entry->nkey;
	cc_memcpy(entry_key_cpy, entry_key(entry), entry_nkey);

	zmap_delete_raw(it, zmap, entry, node);
	zmap_add_raw(it, zmap, entry_key_cpy, entry_nkey, val, nval, flags);
    }
}

static bool
zmap_item_append(struct item *it, void *new_item_buffer, size_t entry_size)
{
    bool ret;
    struct item *appended = create_item(item_key(it), it->nkey, new_item_buffer,
					entry_size);

#if defined CC_CHAINED && CC_CHAINED == 1
    ret = (item_append_contig(appended) == ANNEX_OK);
#else
    ret = (item_append(appended) == ANNEX_OK);
#endif

    item_remove(appended);
    return ret;
}

/* Advance entry to the next entry in the zipmap. Advances it if necessary
   so that entry is contained in it */
static void
zmap_advance_entry(struct zmap_entry **entry, struct item **it)
{
    ASSERT(*entry != NULL);
    ASSERT(*it != NULL);
#if defined CC_CHAINED && CC_CHAINED == 1
    if(!entry_last_in_node(*entry)) {
	/* Entry is not last in node, advance it normally */
	*entry = zmap_entry_next(*entry);
    } else {
	/* Entry is last in node, advance to next node */
	if((*it)->next_node != NULL) {
	    *it = (*it)->next_node;
	    *entry = (struct zmap_entry *)item_data(*it);
	} else {
	    entry = NULL;
	}
    }
#else
    *entry = zmap_entry_next(*entry);
#endif
}

/* Returns true if an entry with the given parameters would be small enough to
   fit inside the zipmap, false otherwise */
static bool
zmap_check_size(uint8_t npkey, uint8_t nskey, uint32_t nval)
{
    /* Return true iff entry size <= maximum nbyte for item */
    return entry_ntotal(nskey, nval, 3) <=
	item_max_nbyte(slabclass_max_id, npkey);
}

#if defined CC_CHAINED && CC_CHAINED == 1
static void
zmap_realloc_from_tail(struct item *it, struct item *node)
{
    struct item *tail;
    struct zmap_entry *iter;

    while(((tail = item_tail(it))->nbyte <=
	  (item_max_nbyte(node->id, node->nkey) - node->nbyte)) && tail != node) {
	/* All of the tail can fit, copy it over and free it */
	cc_memcpy(item_data(node) + node->nbyte, item_data(tail), tail->nbyte);

	node->nbyte += tail->nbyte;
	item_remove_node(it, tail);

        /* unflag old last entry */
        for(iter = (node == it) ? zmap_get_entry_ptr(item_to_zmap(it)) :
		(struct zmap_entry *)item_data(node);
            !entry_last_in_node(iter); iter = zmap_entry_next(iter));
        iter->flags &= ~ENTRY_LAST_IN_NODE;
    }

    if(tail != node) {
	/* it is still not the last node, and cannot fit all of the last node's
	   data; copy over as much of it as possible */
	uint32_t index = 0;
	struct zmap_entry *new_last = NULL;

	/* Find the index from where copying should start */
	for(iter = (struct zmap_entry *)item_data(tail);
            tail->nbyte - index > (item_max_nbyte(node->id, node->nkey) -
				   node->nbyte);
	    iter = zmap_entry_next(iter)) {
	    new_last = iter;
	    index += entry_size(iter);
	}

	new_last->flags |= ENTRY_LAST_IN_NODE;

	ASSERT(tail->head != tail);

	cc_memcpy(item_data(node) + node->nbyte, item_data(tail) + index,
		  tail->nbyte - index);

	/* unflag old last entry if anything was copied */
	if(tail->nbyte - index != 0) {
	    for(iter = (node == it) ? zmap_get_entry_ptr(item_to_zmap(it)) :
		    (struct zmap_entry *)item_data(node);
		!entry_last_in_node(iter); iter = zmap_entry_next(iter));
	    iter->flags &= ~ENTRY_LAST_IN_NODE;
	}

	node->nbyte += tail->nbyte - index;
	tail->nbyte -= tail->nbyte - index;
    }
}

/* Get the entry that comes before the given entry (in the same node). Returns
   NULL if the given entry is the first in the node or not in node. head indicates
   whether or not node is the head of the zmap. */
static struct zmap_entry *
zmap_get_prev_entry(struct item *node, struct zmap_entry *entry, bool head)
{
    struct zmap_entry *iter;

    struct zmap_entry *prev = NULL;

    for(iter = head ? zmap_get_entry_ptr(item_to_zmap(node)) :
	    (struct zmap_entry *)item_data(node);
	iter != entry;
	iter = zmap_entry_next(iter)) {
	prev = iter;

	if(entry_last_in_node(prev)) {
	    /* entry not found in node */
	    prev = NULL;
	    break;
	}
    }

    return prev;
}
#endif
