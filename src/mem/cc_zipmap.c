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

#include <mem/cc_zipmap.h>

#include <mem/cc_items.h>
#include <mem/cc_mem_interface.h>
#include <cc_debug.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <cc_string.h>

#define ZMAP_FOREACH(_map, _entry, _entry_num)		          \
    for((_entry_num) = 0, (_entry) = zmap_get_entry_ptr(_map);    \
        (_entry_num) < (_map)->len;                               \
        ++(_entry_num), (_entry) = zmap_entry_next(_entry))

static void *zmap_get_entry_ptr(struct zmap *zmap);
static struct zmap_entry *zmap_entry_next(struct zmap_entry *entry);
static struct zmap_entry *zmap_lookup_raw(struct zmap *zmap, void *key, uint8_t nkey);
static void zmap_add_raw(struct item *it, struct zmap *zmap, void *skey, uint8_t nskey, void *val, uint32_t nval, uint8_t flags);
static void zmap_delete_raw(struct item *it, struct zmap *zmap, struct zmap_entry *zmap_entry);
static size_t zmap_new_entry_size(uint8_t nskey, uint32_t nval);
static void zmap_set_raw(struct item *it, struct zmap *zmap, void *skey, uint8_t nskey, void *val, uint32_t nval, uint8_t flags);
static void zmap_replace_raw(struct item *it, struct zmap *zmap, struct zmap_entry *entry, void *val, uint32_t nval, uint8_t flags);

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

    log_stderr("zmap header size: %d zmap entry header size: %d", ZMAP_HDR_SIZE, ZMAP_ENTRY_HDR_SIZE);

    store_key(primary_key, nkey, &header, ZMAP_HDR_SIZE);
}

zmap_set_result_t
zmap_set(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, void *val, uint32_t nval)
{
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_SET_NOT_FOUND;
    }

    zmap_set_raw(it, zmap, skey, nskey, val, nval, 0);
    item_remove(it);
    return ZMAP_SET_OK;
}

zmap_set_result_t
zmap_set_multiple(void *pkey, uint8_t npkey, zmap_key_val_vector_t *pairs)
{
    uint32_t i;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_SET_NOT_FOUND;
    }

    for(i = 0; i < pairs->len; ++i) {
	zmap_set_raw(it, zmap, pairs->key_val_pairs[i].key,
		     pairs->key_val_pairs[i].nkey, pairs->key_val_pairs[i].val,
		     pairs->key_val_pairs[i].nval, 0);
    }

    item_remove(it);
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

    zmap_set_raw(it, zmap, skey, nskey, &val, sizeof(int64_t), ENTRY_IS_NUMERIC);
    item_remove(it);
    return ZMAP_SET_OK;
}

zmap_set_result_t
zmap_set_multiple_numeric(void *pkey, uint8_t npkey, zmap_key_numeric_vector_t *pairs)
{
    uint32_t i;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_SET_NOT_FOUND;
    }

    for(i = 0; i < pairs->len; ++i) {
	zmap_set_raw(it, zmap, pairs->key_numeric_pairs[i].key,
		     pairs->key_numeric_pairs[i].nkey,
		     &(pairs->key_numeric_pairs[i].val),
		     sizeof(int64_t), ENTRY_IS_NUMERIC);
    }

    item_remove(it);
    return ZMAP_SET_OK;
}

zmap_add_result_t
zmap_add(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, void *val, uint32_t nval)
{
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_ADD_NOT_FOUND;
    }

    if(zmap_lookup_raw(zmap, skey, nskey) != NULL) {
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

    if(zmap_lookup_raw(zmap, skey, nskey) != NULL) {
	/* key already exists in zipmap */
	item_remove(it);
	return ZMAP_ADD_EXISTS;
    }

    zmap_add_raw(it, zmap, skey, nskey, &val, sizeof(int64_t), ENTRY_IS_NUMERIC);
    item_remove(it);
    return ZMAP_ADD_OK;
}

zmap_replace_result_t
zmap_replace(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, void *val, uint32_t nval)
{
    struct zmap_entry *entry;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_REPLACE_NOT_FOUND;
    }

    if((entry = zmap_lookup_raw(zmap, skey, nskey)) == NULL) {
	/* key does not exist in zipmap */
	item_remove(it);
	return ZMAP_REPLACE_ENTRY_NOT_FOUND;
    }

    zmap_replace_raw(it, zmap, entry, val, nval, 0);
    item_remove(it);
    return ZMAP_REPLACE_OK;
}

zmap_replace_result_t
zmap_replace_numeric(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, int64_t val)
{
    struct zmap_entry *entry;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_REPLACE_NOT_FOUND;
    }

    if((entry = zmap_lookup_raw(zmap, skey, nskey)) == NULL) {
	/* key does not exist in zipmap */
	item_remove(it);
	return ZMAP_REPLACE_ENTRY_NOT_FOUND;
    }

    zmap_replace_raw(it, zmap, entry, &val, sizeof(int64_t), ENTRY_IS_NUMERIC);
    item_remove(it);
    return ZMAP_REPLACE_OK;
}

zmap_delete_result_t
zmap_delete(void *pkey, uint8_t npkey, void *skey, uint8_t nskey)
{
    struct zmap_entry *entry;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_DELETE_NOT_FOUND;
    }

    entry = zmap_lookup_raw(zmap, skey, nskey);

    if(entry == NULL) {
	/* zmap entry is not in the zmap */
	item_remove(it);
	return ZMAP_DELETE_ENTRY_NOT_FOUND;
    }

    zmap_delete_raw(it, zmap, entry);
    item_remove(it);
    return ZMAP_DELETE_OK;
}

zmap_get_result_t
zmap_get(void *pkey, uint8_t npkey, void *skey, uint8_t nskey, void **val, uint32_t *vlen)
{
    struct zmap_entry *entry;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	return ZMAP_GET_NOT_FOUND;
    }

    entry = zmap_lookup_raw(zmap, skey, nskey);

    if(entry == NULL) {
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

    entry = zmap_lookup_raw(zmap, skey, nskey);

    item_remove(it);

    if(entry == NULL) {
	/* zmap entry is not in the zmap */
	return ZMAP_ENTRY_NOT_FOUND;
    }

    return ZMAP_ENTRY_EXISTS;
}

zmap_key_val_vector_t
zmap_get_all(void *pkey, uint8_t npkey)
{
    struct zmap_entry *iter;
    uint32_t i;
    zmap_key_val_vector_t ret;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	log_stderr("no item with that key!");
	ret.len = -1;
	ret.key_val_pairs = NULL;
	return ret;
    }

    /* Allocate space for the return value */
    /* TODO: switch from cc_alloc to using a preallocated pool */
    ret.key_val_pairs = cc_alloc(zmap->len * sizeof(struct key_val_pair));
    if(ret.key_val_pairs == NULL) {
	/* Could not allocate enough memory */
	log_stderr("Could not allocate enough memory to get all pairs!");
	ret.len = -1;
	item_remove(it);
	return ret;
    }

    ret.len = zmap->len;

    /* Copy locations and sizes of key/val buffers */
    ZMAP_FOREACH(zmap, iter, i) {
	ret.key_val_pairs[i].key = entry_key(iter);
	ret.key_val_pairs[i].val = entry_val(iter);
	ret.key_val_pairs[i].nkey = iter->nkey;
	ret.key_val_pairs[i].nval = iter->nval;
    }

    item_remove(it);
    return ret;
}

zmap_buffer_vector_t
zmap_get_keys(void *pkey, uint8_t npkey)
{
    struct zmap_entry *iter;
    uint32_t i;
    zmap_buffer_vector_t ret;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	log_stderr("no item with that key!");
	ret.len = -1;
	ret.bufs = NULL;
	return ret;
    }

    /* Allocate space for the return value */
    /* TODO: switch from cc_alloc to using a preallocated pool */
    ret.bufs = cc_alloc(zmap->len * sizeof(struct buf));
    if(ret.bufs == NULL) {
	/* Could not allocate enough memory */
	log_stderr("Could not allocate enough memory to get all keys!");
	ret.len = -1;
	item_remove(it);
	return ret;
    }

    ret.len = zmap->len;

    /* Copy over keys */
    ZMAP_FOREACH(zmap, iter, i) {
	ret.bufs[i].buf = entry_key(iter);
	ret.bufs[i].nbuf = iter->nkey;
    }

    item_remove(it);
    return ret;
}

zmap_buffer_vector_t
zmap_get_vals(void *pkey, uint8_t npkey)
{
    struct zmap_entry *iter;
    uint32_t i;
    zmap_buffer_vector_t ret;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	log_stderr("no item with that key!");
	ret.len = -1;
	ret.bufs = NULL;
	return ret;
    }

    /* Allocate space for the return value */
    /* TODO: switch from cc_alloc to using a preallocated pool */
    ret.bufs = cc_alloc(zmap->len * sizeof(struct buf));
    if(ret.bufs == NULL) {
	/* Could not allocate enough memory */
	log_stderr("Could not allocate enough memory to get all keys!");
	ret.len = -1;
	item_remove(it);
	return ret;
    }

    ret.len = zmap->len;

    /* Copy over vals */
    ZMAP_FOREACH(zmap, iter, i) {
	ret.bufs[i].buf = entry_val(iter);
	ret.bufs[i].nbuf = iter->nval;
    }

    item_remove(it);
    return ret;
}

zmap_buffer_vector_t
zmap_get_multiple(void *pkey, uint8_t npkey, zmap_buffer_vector_t *keys)
{
    zmap_buffer_vector_t ret;
    uint32_t i;
    struct item *it = item_get(pkey, npkey);
    struct zmap *zmap = item_to_zmap(it);

    if(zmap == NULL) {
	/* zmap not in cache */
	log_stderr("no item with that key!");
	ret.len = -1;
	ret.bufs = NULL;
	return ret;
    }

    /* Allocate space for the return value */
    /* TODO: switch from cc_alloc to using a preallocated pool */
    ret.bufs = cc_alloc(keys->len *sizeof(struct buf));
    if(ret.bufs == NULL) {
	/* Could not allocate enough memory */
	log_stderr("Could not allocate enough memory to get all vals!");
	ret.len = -1;
	item_remove(it);
	return ret;
    }

    ret.len = keys->len;

    /* Search for each value */
    for(i = 0; i < keys->len; ++i) {
	struct zmap_entry *entry =
	    zmap_lookup_raw(zmap, keys->bufs[i].buf, keys->bufs[i].nbuf);

	if(entry == NULL) {
	    ret.bufs[i].buf = NULL;
	} else {
	    ret.bufs[i].buf = entry_val(entry);
	    ret.bufs[i].nbuf = entry->nval;
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

    loga_hexdump(zmap, 100, "zmap_len hexdump");
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

    entry = zmap_lookup_raw(zmap, skey, nskey);

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
zmap_lookup_raw(struct zmap *zmap, void *key, uint8_t nkey)
{
    struct zmap_entry *iter;
    uint32_t i;

    ASSERT(zmap != NULL);

    ZMAP_FOREACH(zmap, iter, i) {
	log_stderr("checking entry %u", i);
	log_stderr("iter->nkey: %hhu nkey: %hhu key: %s iter key: %s", iter->nkey, nkey, key, entry_key(iter));
	log_stderr("zmap addr: %p iter addr: %p", zmap, iter);
	loga_hexdump(iter, 40, "iter hexdump");
	if(nkey == iter->nkey && cc_memcmp(key, entry_key(iter), nkey) == 0) {
	    /* Match found */
	    log_stderr("match found!");
	    return iter;
	}
    }

    return NULL;
}

/* Adds the given key/val to the zipmap */
static void
zmap_add_raw(struct item *it, struct zmap *zmap, void *skey, uint8_t nskey, void *val, uint32_t nval, uint8_t flags)
{
    struct zmap_entry new_entry_header;
    size_t entry_size = zmap_new_entry_size(nskey, nval);
    void *new_item_buffer;
    char it_key[UCHAR_MAX];
    uint8_t it_nkey;

    ASSERT(it != NULL);
    it_nkey = it->nkey;
    cc_memcpy(it_key, item_key(it), it_nkey);

    loga_hexdump(zmap, 100, "add_raw - before:");

    /* TODO: switch over from cc_alloc to using a preallocated memory pool */
    new_item_buffer = cc_alloc(entry_size);
    if(new_item_buffer == NULL) {
	/* Item cannot be added because not enough memory */
	log_stderr("Cannot add item; not enough memory!");
	return;
    }

    new_entry_header.nkey = nskey;
    new_entry_header.nval = nval;
    new_entry_header.npadding = entry_size - ZMAP_ENTRY_HDR_SIZE - nskey - nval;
    new_entry_header.flags = flags;

    /* Form new entry by copying over data */
    cc_memcpy(new_item_buffer, &new_entry_header, ZMAP_ENTRY_HDR_SIZE);
    cc_memcpy(new_item_buffer + ZMAP_ENTRY_HDR_SIZE, skey, nskey);
    cc_memcpy(new_item_buffer + ZMAP_ENTRY_HDR_SIZE + nskey, val, nval);

    append_val(item_key(it), it->nkey, new_item_buffer, entry_size);

    /* After append, it and zmap are invalidated */
    it = item_get(it_key, it_nkey);
    zmap = item_to_zmap(it);

    /* TODO: extend functionality to chaining */
    ASSERT(!item_is_chained(it));

    ++(zmap->len);
}

/* Delete the given entry from the zipmap */
static void
zmap_delete_raw(struct item *it, struct zmap *zmap, struct zmap_entry *zmap_entry)
{
    struct zmap_entry *iter;
    uint32_t i, amt_to_move = 0;
    bool found_deleted = false;

    ASSERT(zmap != NULL);
    ASSERT(zmap_entry != NULL);
    ASSERT(zmap->len > 0);

    /* Shift everything down */

    /* Calculate amount needed to shift */
    ZMAP_FOREACH(zmap, iter, i) {
	if(found_deleted) {
	    amt_to_move += entry_size(iter);
	}

	if(iter == zmap_entry) {
	    found_deleted = true;
	}
    }

    /* Adjust item size */
    it->nbyte -= entry_size(zmap_entry);

    cc_memmove(zmap_entry, zmap_entry_next(zmap_entry), amt_to_move);

    /* Adjust len */
    --(zmap->len);
}

static size_t
zmap_new_entry_size(uint8_t nskey, uint32_t nval)
{
    size_t ret = ZMAP_ENTRY_HDR_SIZE + nskey + nval;

    /* round ret up to the next highest word */
    ret += (sizeof(uint32_t) - (ret % sizeof(uint32_t)));

    return ret;
}

static void
zmap_set_raw(struct item *it, struct zmap *zmap, void *skey, uint8_t nskey, void *val, uint32_t nval, uint8_t flags)
{
    struct zmap_entry *entry;

    ASSERT(zmap != NULL);

    entry = zmap_lookup_raw(zmap, skey, nskey);

    if(entry != NULL) {
	/* key already exists in zipmap, overwrite its value */
	zmap_replace_raw(it, zmap, entry, val, nval, flags);
    } else {
	/* key does not exist in zipmap, add it */
	zmap_add_raw(it, zmap, skey, nskey, val, nval, flags);
    }
}

static void
zmap_replace_raw(struct item *it, struct zmap *zmap, struct zmap_entry *entry, void *val, uint32_t nval, uint8_t flags)
{
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
	zmap_add_raw(it, zmap, entry_key(entry), entry->nkey, val, nval, flags);
	zmap_delete_raw(it, zmap, entry);
    }
}
