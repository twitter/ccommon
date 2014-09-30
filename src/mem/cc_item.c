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

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <cc_bstring.h>
#include <cc_debug.h>
#include <cc_log.h>
#include <cc_print.h>
#include <hash/cc_hash_table.h>
#include <mem/cc_mem_globals.h>
#include <mem/cc_slab.h>

#include <mem/cc_item.h>

struct hash_table mem_hash_table;
static uint64_t cas_id;                         /* unique cas id */

static uint64_t item_next_cas(void);
static bool item_expired(struct item *it);
static void item_acquire_refcount(struct item *it);
static void item_release_refcount(struct item *it);
static void item_free(struct item *it);
static void skip_space(const uint8_t **str, size_t *len);
static bool strtoull_len(const uint8_t *str, uint64_t *out, size_t len);

static struct item *_item_alloc(uint8_t nkey, rel_time_t exptime, uint32_t nbyte);
static void _item_link(struct item *it);
static void _item_unlink(struct item *it);
static void _item_remove(struct item *it);
static void _item_relink(struct item *it, struct item *nit);
static struct item *_item_get(const uint8_t *key, size_t nkey);
static void _item_set(struct item *it);
static item_cas_result_t _item_cas(struct item *it);
static item_add_result_t _item_add(struct item *it);
static item_replace_result_t _item_replace(struct item *it);
static item_annex_result_t _item_append(struct item *it, bool contig);
static item_annex_result_t _item_prepend(struct item *it);
static item_delta_result_t _item_delta(uint8_t *key, size_t nkey, bool incr,
				       uint64_t delta);
static void item_append_same_id(struct item *oit, struct item *it, uint32_t
				total_nbyte);
static void item_prepend_same_id(struct item *oit, struct item *it, uint32_t
				 total_nbyte);

#if defined CC_HAVE_CHAINED && CC_HAVE_CHAINED == 1
static void item_prepare_tail(struct item *nit);
static bool item_is_contained(struct item *it, struct item *candidate);
#endif

#define INCR_MAX_STORAGE_LEN 24
#define GET_ITEM_MAX_TRIES   50

rstatus_t
item_init(void)
{
    log_info("item header size: %zu\n", ITEM_HDR_SIZE);

    /*pthread_mutex_init(&cache_lock, NULL);*/

    cas_id = 0ULL;

    return CC_OK;
}

rstatus_t
item_hash_init(uint32_t hash_power)
{
    return hash_table_init(hash_power, &mem_hash_table);
}

uint8_t
item_id(struct item *it)
{
    return item_2_slab(it)->id;
}

uint8_t *
item_data(struct item *it)
{
    uint8_t *data;

    ASSERT(it->magic == ITEM_MAGIC);

    if (item_is_raligned(it)) {
        data = (uint8_t *)it + slab_item_size(item_id(it)) - it->nbyte;
    } else {
        data = it->data + it->nkey + CAS_LEN(it);
    }

    return data;
}

void
item_hdr_init(struct item *it, uint32_t offset)
{
    ASSERT(offset >= SLAB_HDR_SIZE && offset < mem_settings.slab_size.val.uint32_val);

#if defined HAVE_ASSERT_PANIC && HAVE_ASSERT_PANIC == 1 || defined HAVE_ASSERT_LOG && HAVE_ASSERT_LOG == 1
    it->magic = ITEM_MAGIC;
#endif
    it->offset = offset;
    it->refcount = 0;
    it->flags = 0;
#if defined CC_HAVE_CHAINED && CC_HAVE_CHAINED == 1
    it->next_node = NULL;
    it->head = NULL;
#endif
}

/*
 * It may be possible to reallocate individual nodes of chained items, so that
 * evicting a slab does not necessarily evict all items with nodes in that
 * slab; this is a possibility worth considering later
 */
#if defined CC_HAVE_CHAINED && CC_HAVE_CHAINED == 1
void
item_reuse(struct item *it)
{
    struct item *prev;
    struct slab *evicted = item_2_slab(it);

    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/
    ASSERT(it->magic == ITEM_MAGIC);
    ASSERT(!item_is_slabbed(it));
    ASSERT(item_is_linked(it->head));
    ASSERT(it->head->refcount == 0);

    /* Unlink head of item */
    it->head->flags &= ~ITEM_LINKED;
    hash_table_remove(item_key(it->head), it->head->nkey, &mem_hash_table);

    /* Free all nodes */
    for(prev = it = it->head; prev != NULL; prev = it) {
	if(it != NULL) {
	    it = it->next_node;
	}

	/* Remove any chaining structure */
	prev->next_node = NULL;
	prev->head = NULL;

	/* If the node is not in the slab we are evicting, free it */
	if(item_2_slab(prev) != evicted) {
	    item_free(prev);
	}
    }
}
#else
void
item_reuse(struct item *it)
{
    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/
    ASSERT(it->magic == ITEM_MAGIC);
    ASSERT(!item_is_slabbed(it));
    ASSERT(item_is_linked(it->head));
    ASSERT(it->refcount == 0);

    hash_table_remove(item_key(it), it->nkey, &mem_hash_table);

    log_verb("reuse %s item %s at offset %d with id %hhu",
	    item_expired(it) ? "expired" : "evicted", item_key(it),
	    it->offset, item_id(it));
}
#endif

uint8_t item_slabid(uint8_t nkey, uint32_t nbyte)
{
    /* Calculate total size of item, get slab id using the size */
    return slab_id(item_ntotal(nkey, nbyte, mem_settings.use_cas.val.bool_val));
}

struct item *
item_alloc(uint8_t nkey, rel_time_t exptime, uint32_t nbyte)
{
    struct item *it;

    /*pthread_mutex_lock(&cache_lock);*/
    it = _item_alloc(nkey, exptime, nbyte);
    /*pthread_mutex_unlock(&cache_lock);*/

    return it;
}

void
item_remove(struct item *it)
{
    /*pthread_mutex_lock(&cache_lock);*/
    _item_remove(it);
    /*pthread_mutex_unlock(&cache_lock);*/
}

struct item *
item_get(const uint8_t *key, size_t nkey)
{
    struct item *it;

    /*pthread_mutex_lock(&cache_lock);*/
    it = _item_get(key, nkey);
    /*pthread_mutex_unlock(&cache_lock);*/

    return it;
}

void
item_set(struct item *it)
{
    /*pthread_mutex_lock(&cache_lock);*/
    _item_set(it);
    /*pthread_mutex_unlock(&cache_lock);*/
}

item_cas_result_t
item_cas(struct item *it)
{
    item_cas_result_t ret;

    /*pthread_mutex_lock(&cache_lock);*/
    ret = _item_cas(it);
    /*pthread_mutex_unlock(&cache_lock);*/

    return ret;
}

item_add_result_t
item_add(struct item *it)
{
    item_add_result_t ret;

    /*pthread_mutex_lock(&cache_lock);*/
    ret = _item_add(it);
    /*pthread_mutex_unlock(&cache_lock);*/

    return ret;
}

item_replace_result_t
item_replace(struct item *it)
{
    item_replace_result_t ret;

    /*pthread_mutex_lock(&cache_lock);*/
    ret = _item_replace(it);
    /*pthread_mutex_unlock(&cache_lock);*/

    return ret;
}

item_annex_result_t
item_append(struct item *it)
{
    item_annex_result_t ret;
    /*pthread_mutex_lock(&cache_lock);*/
    ret = _item_append(it, false);
    /*pthread_mutex_unlock(&cache_lock);*/

    return ret;
}

item_annex_result_t
item_prepend(struct item *it)
{
    item_annex_result_t ret;
    /*pthread_mutex_lock(&cache_lock);*/
    ret = _item_prepend(it);
    /*pthread_mutex_unlock(&cache_lock);*/

    return ret;
}

item_delta_result_t
item_delta(uint8_t *key, size_t nkey, bool incr, uint64_t delta)
{
    item_delta_result_t ret;

    /*pthread_mutex_lock(&cache_lock);*/
    ret = _item_delta(key, nkey, incr, delta);
    /*pthread_mutex_unlock(&cache_lock);*/

    return ret;
}

/*
 * Unlink an item and remove it (if its refcount drops to zero).
 */
item_delete_result_t
item_delete(uint8_t *key, size_t nkey)
{
    item_delete_result_t ret = DELETE_OK;
    struct item *it;

    /*pthread_mutex_lock(&cache_lock);*/
    it = _item_get(key, nkey);
    ASSERT(it->head == it);
    if (it != NULL) {
        _item_unlink(it);
        _item_remove(it);
    } else {
        ret = DELETE_NOT_FOUND;
    }
    /*pthread_mutex_unlock(&cache_lock);*/

    return ret;
}

/* Chaining specific functions */
#if defined CC_HAVE_CHAINED && CC_HAVE_CHAINED == 1
uint32_t
item_num_nodes(struct item *it)
{
    uint32_t num_nodes = 0;

    ASSERT(it != NULL);

    for(; it != NULL; it = it->next_node, ++num_nodes);
    return num_nodes;
}

item_annex_result_t
item_append_contig(struct item *it)
{
    item_annex_result_t ret;
    /*pthread_mutex_lock(&cache_lock);*/
    ret = _item_append(it, true);
    /*pthread_mutex_unlock(&cache_lock);*/

    return ret;
}

struct item *
ichain_tail(struct item *it)
{
    ASSERT(it != NULL);
    for(; it->next_node != NULL; it = it->next_node);
    return it;
}

void
ichain_remove_item(struct item *it, struct item *node) {
    struct item *iter, *prev = NULL;

    /*pthread_mutex_lock(&cache_lock);*/
    for(iter = it; iter != NULL; iter = iter->next_node) {
	if(iter == node) {
	    /* found node to be removed */
	    if(iter == it) {
		/* Node is the first node in the item */

		if(node->next_node == NULL) {
		    /* node is the only node in the item */
		    _item_unlink(node);
		    item_free(node);
		    return;
		}

		/* removing head, need to copy head over to second node */
		cc_memcpy(it->next_node, it, ITEM_HDR_SIZE);
		_item_relink(it, it->next_node);
	    } else {
		ASSERT(prev != NULL);
		prev->next_node = node->next_node;
		item_free(node);
	    }
	}

	prev = iter;
    }
    /*pthread_mutex_unlock(&cache_lock);*/
}
#endif

/*
 * Returns the next cas id for a new item. Minimum cas value
 * is 1 and the maximum cas value is UINT64_MAX
 */
static uint64_t
item_next_cas(void)
{
    if (mem_settings.use_cas.val.bool_val) {
        return ++cas_id;
    }

    return 0ULL;
}

/*
 * Is this item expired?
 */
static bool
item_expired(struct item *it)
{
    ASSERT(it->magic == ITEM_MAGIC);

    return (it->exptime > 0 && it->exptime < time_now()) ? true : false;
}

/*
 * Increment the number of references to the given item
 *
 * With the current system, the refcount for each slab might not be completely
 * accurate, since if there are 2 items chained in the same slab, the number of
 * references to the slab increases twice when the item is referenced once.
 * However, this should not cause any issues as of right now, since we only use
 * refcount to see if anybody is using the slab.
 */
static void
item_acquire_refcount(struct item *it)
{
    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/
    ASSERT(it->magic == ITEM_MAGIC);

    it->refcount++;

#if defined CC_HAVE_CHAINED && CC_HAVE_CHAINED == 1
    for(; it != NULL; it = it->next_node) {
	slab_acquire_refcount(item_2_slab(it));
    }
#else
    slab_acquire_refcount(item_2_slab(it));
#endif
}

/*
 * Decrement the number of references to the given item
 */
static void
item_release_refcount(struct item *it)
{
    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/
    ASSERT(it->magic == ITEM_MAGIC);
    ASSERT(it->refcount > 0);

    it->refcount--;

#if defined CC_HAVE_CHAINED && CC_HAVE_CHAINED == 1
    for(; it != NULL; it = it->next_node) {
	slab_release_refcount(item_2_slab(it));
    }
#else
    slab_release_refcount(item_2_slab(it));
#endif
}

/*
 * Free an item by putting it on the free queue for the slab class
 */
static void
item_free(struct item *it)
{
    ASSERT(it->magic == ITEM_MAGIC);
    ASSERT(!item_is_linked(it));

#if defined CC_HAVE_CHAINED && CC_HAVE_CHAINED == 1
    /* Keep two pointers to the chain of items, one to do the freeing (prev) and
       the other to keep a handle on the rest of the chain (it) */
    struct item *prev = it;

    for (prev = it; prev != NULL; prev = it) {
	/* Advance it, if it is not already at the end of the chain */
	if(it != NULL) {
	    it = it->next_node;
	}

	ASSERT(!item_is_linked(prev));
	ASSERT(!item_is_slabbed(prev));
	ASSERT(prev->refcount == 0);

	/* Free prev */
	prev->flags &= ~ITEM_CHAINED;
	prev->next_node = NULL;
	prev->head = NULL;
	slab_put_item(prev);
    }
#else
    slab_put_item(it);
#endif
}

/* Skip spaces in str */
static void
skip_space(const uint8_t **str, size_t *len)
{
    while(*len > 0 && isspace(**str)) {
	(*str)++;
	(*len)--;
    }
}

/* Convert string to integer, used for delta */
static bool
strtoull_len(const uint8_t *str, uint64_t *out, size_t len)
{
    *out = 0ULL;

    skip_space(&str, &len);

    while (len > 0 && (*str) >= '0' && (*str) <= '9') {
        if (*out >= UINT64_MAX / 10) {
            /*
             * At this point the integer is considered out of range,
             * by doing so we convert integers up to (UINT64_MAX - 6)
             */
            return false;
        }
        *out = *out * 10 + *str - '0';
        str++;
        len--;
    }

    skip_space(&str, &len);

    if (len == 0) {
        return true;
    } else {
        return false;
    }
}

/*
 * Allocate an item. We allocate an item by consuming the next free item from
 * the item's slab class.
 */
#if defined CC_HAVE_CHAINED && CC_HAVE_CHAINED == 1
static struct item *
_item_alloc(uint8_t nkey, rel_time_t exptime, uint32_t nbyte)
{
    struct item *it = NULL, *current_node, *prev_node = NULL;
    uint8_t id;
    uint32_t i;

    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/

    do {
	/* Get slab id based on nbyte, and nkey if first node */
	id = slab_id(item_ntotal(
			 (it == NULL) ? nkey : 0, nbyte,
			 (it == NULL) ? mem_settings.use_cas.val.bool_val : false));

	/* If remaining data cannot fit into a single item, allocate the
	   biggest item possible */
	if (id == SLABCLASS_CHAIN_ID) {
	    id = slabclass_max_id;
	}

	for(i = 0; i < GET_ITEM_MAX_TRIES; ++i) {
	    current_node = slab_get_item(id);

	    if(current_node == NULL) {
		/* Could not successfully allocate item(s) */
		log_warn("server error on allocating item in slab %hhu",
			   id);
		return NULL;
	    }

	    if(!item_is_contained(it, current_node)) {
		break;
	    }
	}

	if(it == NULL) {
	    it = current_node;
	}

	ASSERT(!item_is_linked(current_node));
	ASSERT(!item_is_slabbed(current_node));
	ASSERT(current_node->offset != 0);
	ASSERT(current_node->refcount == 0);
	ASSERT(current_node->next_node == NULL);
	ASSERT(current_node->head == NULL);

	/* Default behavior is to set the chained flag to true, we double check
	   for this after the loop finishes */
	current_node->flags |= ITEM_CHAINED;
	current_node->head = it;

	current_node->exptime = exptime;

	if(current_node == it) {
	    /* If this is the first node in the chain */
	    current_node->nkey = nkey;
	} else {
	    current_node->nkey = 0;
	}

	/* Set nbyte equal to either the number of bytes left or the number
	   of bytes that can fit in the item, whichever is less */
	current_node->nbyte =
	    (nbyte < slab_item_size(id) - ITEM_HDR_SIZE - current_node->nkey) ?
	    nbyte : slab_item_size(id) - ITEM_HDR_SIZE - current_node->nkey;

	nbyte -= current_node->nbyte;

	if(prev_node != NULL) {
	    prev_node->next_node = current_node;
	}
	prev_node = current_node;
    } while(nbyte != 0);

    item_acquire_refcount(it);

    if(it->next_node == NULL) {
	it->flags &= ~ITEM_CHAINED;
    }

    /* In a chained item, the head is always right aligned to allow for easy
       prepending */
    if(item_is_chained(it)) {
	it->flags |= ITEM_RALIGN;
    }

    if(mem_settings.use_cas.val.bool_val) {
	it->flags |= ITEM_CAS;
    }

    item_set_cas(it, 0);

    log_verb("alloc item at offset %u with id %hhu expiry %u "
	      " refcount %hu", it->offset, item_id(it), it->exptime,
	      it->refcount);

    return it;
}
#else
static struct item *
_item_alloc(uint8_t nkey, rel_time_t exptime, uint32_t nbyte)
{
    struct item *it;
    uint8_t id;

    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/

    id = slab_id(item_ntotal(nkey, nbyte, mem_settings.use_cas.val.bool_val));

    if(id == SLABCLASS_CHAIN_ID) {
	log_notice("No id large enough to contain that item!");
	return NULL;
    }

    ASSERT(id >= SLABCLASS_MIN_ID && id <= SLABCLASS_MAX_ID);

    it = slab_get_item(id);

    if(it == NULL) {
	/* Could not successfully allocate item */
	log_warn("server error on allocating item in slab %hhu", id);
	return NULL;
    }

    ASSERT(!item_is_linked(it));
    ASSERT(!item_is_slabbed(it));
    ASSERT(it->offset != 0);
    ASSERT(it->refcount == 0);

    item_acquire_refcount(it);

    it->flags = mem_settings.use_cas.val.bool_val ? ITEM_CAS : 0;
    it->nbyte = nbyte;
    it->exptime = exptime;
    it->nkey = nkey;
    item_set_cas(it, 0);

    log_verb("alloc item at offset %u with id %hhu expiry %u "
	    " refcount %hu", it->offset, item_id(it), it->exptime,
	    it->refcount);

    return it;
}
#endif

/*
 * Link an item into the hash table. In the case of a chained item, only the
 * head gets linked.
 */
static void
_item_link(struct item *it)
{
    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/
    ASSERT(it->magic == ITEM_MAGIC);
    ASSERT(!item_is_linked(it));
    ASSERT(!item_is_slabbed(it));
    ASSERT(it->nkey != 0);

    log_verb("link item %s at offset %u with flags %hhu id %hhu",
	      item_key(it), it->offset, it->flags, item_id(it));

    it->flags |= ITEM_LINKED;
    item_set_cas(it, item_next_cas());

    hash_table_insert(it, &mem_hash_table);
}

/*
 * Unlinks an item from the hash table. Free an unlinked item if its refcount is
 * zero.
 */
static void
_item_unlink(struct item *it)
{
    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/
    ASSERT(it->magic == ITEM_MAGIC);
    ASSERT(it->head == it);

    log_verb("unlink item %s at offset %u with flags %hhu id %hhu",
	      item_key(it), it->offset, it->flags, item_id(it));

    if (item_is_linked(it)) {
        it->flags &= ~ITEM_LINKED;

        hash_table_remove(item_key(it), it->nkey, &mem_hash_table);

        if (it->refcount == 0) {
            item_free(it);
        }
    }
}

/*
 * Decrement the refcount on an item. Free an unlinked item if its refcount
 * drops to zero.
 */
static void
_item_remove(struct item *it)
{
    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/
    ASSERT(it->magic == ITEM_MAGIC);
    ASSERT(!item_is_slabbed(it));

    log_verb("remove item %s at offset %u with flags %hhu id %hhu "
	      "refcount %hu", item_key(it), it->offset, it->flags, item_id(it),
	    it->refcount);

    if (it->refcount != 0) {
        item_release_refcount(it);
    }

    if (it->refcount == 0 && !item_is_linked(it)) {
        item_free(it);
    }
}

/*
 * Replace one item with another in the hash table.
 */
static void
_item_relink(struct item *it, struct item *nit)
{
    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/
    ASSERT(it->magic == ITEM_MAGIC);
    ASSERT(!item_is_slabbed(it));

    ASSERT(nit->magic == ITEM_MAGIC);
    ASSERT(!item_is_slabbed(nit));

    log_verb("relink item %s at offset %u id %hhu with one at offset "
	      "%u id %hhu", item_key(it), it->offset, item_id(it), nit->offset,
	      item_id(nit));

    _item_unlink(it);
    _item_link(nit);
}

/*
 * Return an item if it hasn't been marked as expired, lazily expiring
 * item as-and-when needed
 *
 * When a non-null item is returned, it's the callers responsibility to
 * release refcount on the item
 */
static struct item *
_item_get(const uint8_t *key, size_t nkey)
{
    struct item *it;

    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/

    it = hash_table_find(key, nkey, &mem_hash_table);
    if (it == NULL) {
        return NULL;
    }

    if (it->exptime != 0 && it->exptime <= time_now()) {
        _item_unlink(it);
        return NULL;
    }

    item_acquire_refcount(it);

    log_debug(LOG_VVERB, "get item %s found at offset %u with flags %hhu id %hhu",
	      item_key(it), it->offset, it->flags, item_id(it));

    return it;
}

/*
 * Links it into the hash table. If there is already an item with the same key
 * in the hash table, that item is unlinked and removed, and it is linked in
 * its place.
 */
static void
_item_set(struct item *it)
{
    uint8_t *key;
    struct item *oit;

    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/

    key = item_key(it);
    oit = _item_get(key, it->nkey);

    if (oit == NULL) {
        _item_link(it);
    } else {
        _item_relink(oit, it);
        _item_remove(oit);
    }

    log_debug(LOG_VVERB, "store item %s at offset %u with flags %hhu id %hhu",
    	      item_key(it), it->offset, it->flags, item_id(it));
}

/*
 * Perform a check-and-set; check if the item has been updated since last time
 * the client fetched it; if no, then replace it.
 */
static item_cas_result_t
_item_cas(struct item *it)
{
    uint8_t *key;
    struct item *oit;

    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/

    key = item_key(it);
    oit = _item_get(key, it->nkey);
    if (oit == NULL) {
	return CAS_NOT_FOUND;
    }

    /* oit is not NULL, some item was found */
    if (item_get_cas(it) != item_get_cas(oit)) {
	log_debug(LOG_VVERB, "cas mismatch %llu != %llu on item %s",
		item_get_cas(oit), item_get_cas(it), item_key(it));
	_item_remove(oit);
	return CAS_EXISTS;
    }

    _item_relink(oit, it);
    log_debug(LOG_VVERB, "cas item %s at offset %u with flags %hhu id %hhu",
	      item_key(it), it->offset, it->flags, item_id(it));
    _item_remove(oit);
    return CAS_OK;
}

/*
 * Store this data, but only if the server does not already hold data for this
 * key.
 */
static item_add_result_t
_item_add(struct item *it)
{
    item_add_result_t ret;
    uint8_t *key;
    struct item *oit;

    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/

    key = item_key(it);
    oit = _item_get(key, it->nkey);
    if (oit != NULL) {
        _item_remove(oit);

        ret = ADD_EXISTS;
    } else {
        _item_link(it);

        ret = ADD_OK;

	log_debug(LOG_VVERB, "add item %s at offset %u with flags %hhu id %hhu",
		  item_key(it), it->offset, it->flags, item_id(it));
    }

    return ret;
}

/*
 * Store this data, but only if the server already holds data for this key.
 */
static item_replace_result_t
_item_replace(struct item *it)
{
    item_replace_result_t ret;
    uint8_t *key;
    struct item *oit;

    ASSERT(it->head == it);

    key = item_key(it);
    oit = _item_get(key, it->nkey);
    if (oit == NULL) {
        ret = REPLACE_NOT_FOUND;
    } else {
	log_debug(LOG_VVERB, "replace oit %s at offset %u with flags %hhu id %hhu",
		  item_key(oit), oit->offset, oit->flags, item_id(oit));

        _item_relink(oit, it);
        _item_remove(oit);

        ret = REPLACE_OK;
    }

    return ret;
}

#if defined CC_HAVE_CHAINED && CC_HAVE_CHAINED == 1
/* Append for chaining enabled */
static item_annex_result_t
_item_append(struct item *it, bool contig)
{
    uint8_t *key;
    struct item *oit, *oit_tail;
    uint8_t nid;
    uint32_t total_nbyte;

    if(item_is_chained(it)) {
	return ANNEX_OVERSIZED;
    }

    ASSERT(it->next_node == NULL);

    key = item_key(it);
    oit = _item_get(key, it->nkey);
    if(oit == NULL) {
	return ANNEX_NOT_FOUND;
    }

    ASSERT(!item_is_slabbed(oit));

    /* get tail of the item */
    oit_tail = ichain_tail(oit);
    total_nbyte = oit_tail->nbyte + it->nbyte; /* get size of new tail */
    nid = item_slabid(oit_tail->nkey, total_nbyte);

    if(nid <= item_id(oit_tail) && !item_is_raligned(oit_tail)) {
	/* if oit is large enough to hold the extra data and left-aligned,
	 * which is the default behavior, we copy the delta to the end of
	 * the existing data. Otherwise, allocate a new item and store the
	 * payload left-aligned.
	 */
	item_append_same_id(oit_tail, it, total_nbyte);
    } else {
	struct item *nit;
	if(contig && nid == SLABCLASS_CHAIN_ID) {
	    /* Need to allocate new node to contiguously store new data */
	    nit = _item_alloc(0, oit->exptime, it->nbyte);

	    /* Could not allocate new tail */
	    if(nit == NULL) {
		_item_remove(oit);
		return ANNEX_EOM;
	    }

	    ASSERT(nit->next_node == NULL);
	    ASSERT(nit->refcount <= oit->refcount);

	    /* Make nit's slab's refcount match that of oit, so that when oit
	       is released nit's slab has the correct refcount */
	    while(nit->refcount < oit->refcount) {
		item_acquire_refcount(nit);
	    }

	    /* Copy over new data */
	    cc_memcpy(item_data(nit), item_data(it), it->nbyte);

	    item_prepare_tail(nit);

	    /* Make nit the new tail of oit */
	    oit_tail->next_node = nit;
	    nit->head = oit;

	    /* Set oit flag */
	    oit->flags |= ITEM_CHAINED;
	} else {
	    if(nid == SLABCLASS_CHAIN_ID) {
		/* Need to allocate new node but does not need to be stored in
		   contiguous memory */
		uint32_t nit_amt_copied;

		nit = _item_alloc(oit_tail->nkey, oit->exptime, total_nbyte);

		/* Could not allocate new tail */
		if(nit == NULL) {
		    _item_remove(oit);
		    return ANNEX_EOM;
		}

		ASSERT(item_is_chained(nit));
		ASSERT(nit->next_node != NULL);

		/* Copy over key */
		cc_memcpy(item_key(nit), item_key(oit_tail), oit_tail->nkey);

		/* Copy over current tail */
		cc_memcpy(item_data(nit), item_data(oit_tail), oit_tail->nbyte);

		nit_amt_copied = slab_item_size(item_id(nit)) - ITEM_HDR_SIZE -
		    oit_tail->nbyte - oit_tail->nkey;

		/* Copy over as much of the new data as possible to the first node */
		cc_memcpy(item_data(nit) + oit_tail->nbyte, item_data(it),
			  nit_amt_copied);

		/* Copy the remaining data into the second node */
		cc_memcpy(item_data(nit->next_node), item_data(it) + nit_amt_copied,
			  it->nbyte - nit_amt_copied);
	    } else {
		/* Simply need to allocate item in larger slabclass */
		nit = _item_alloc(oit_tail->nkey, oit->exptime, total_nbyte);

		/* Could not allocate larger item */
		if(nit == NULL) {
		    _item_remove(oit);
		    return ANNEX_EOM;
		}

		ASSERT(nit->next_node == NULL);

		/* Copy over key */
		cc_memcpy(item_key(nit), item_key(oit_tail), oit_tail->nkey);

		/* Copy over flags */
		nit->flags = oit->flags;

		/* Copy over current tail */
		cc_memcpy(item_data(nit), item_data(oit_tail), oit_tail->nbyte);

		/* Copy over new data */
		cc_memcpy(item_data(nit) + oit_tail->nbyte, item_data(it), it->nbyte);
	    }

	    if(!item_is_chained(oit)) {
		/* oit is not chained, so relink oit with nit */
		_item_relink(oit, nit);

		_item_remove(nit);
	    } else {
		/* oit is chained, set nit as the new tail */
		struct item *nit_prev, *iter;

		ASSERT(oit->next_node != NULL);

		/* Make nit's slab's refcount match that of oit, so that when oit
		   is released nit's slab has the correct refcount */
		while(nit->refcount < oit->refcount) {
		    item_acquire_refcount(nit);
		}

		item_prepare_tail(nit);

		/* set nit_prev to be the second to last node of the oit chain */
		for(nit_prev = oit; nit_prev->next_node->next_node != NULL;
		    nit_prev = nit_prev->next_node);

		/* make nit the new tail of oit */
		nit_prev->next_node = nit;

		/* set head in all new nodes */
		for(iter = nit; iter != NULL; iter = iter->next_node) {
		    iter->head = oit;
		}

		/* free oit_tail, since it is no longer used by anybody */
		slab_release_refcount(item_2_slab(oit_tail));
		item_free(oit_tail);
	    }
	}
    }

    _item_remove(oit);

    return ANNEX_OK;
}
#else
/* Append for chaining disabled */
static item_annex_result_t
_item_append(struct item *it, bool contig)
{
    uint8_t *key;
    struct item *oit, *nit;
    uint8_t nid;
    uint32_t total_nbyte;

    key = item_key(it);
    oit = _item_get(key, it->nkey);
    nit = NULL;

    if(oit == NULL) {
	return ANNEX_NOT_FOUND;
    }

    total_nbyte = oit->nbyte + it->nbyte;
    nid = item_slabid(oit->nkey, total_nbyte);

    if(nid == SLABCLASS_CHAIN_ID) {
	return ANNEX_OVERSIZED;
    }

    if(nid == item_id(oit) && !item_is_raligned(oit)) {
	item_append_same_id(oit, it, total_nbyte);
    } else {
	nit = _item_alloc(oit->nkey, oit->exptime, total_nbyte);

	if(nit == NULL) {
	    _item_remove(oit);
	    return ANNEX_EOM;
	}

	/* Copy over key */
	cc_memcpy(item_key(nit), key, oit->nkey);

	/* Copy over data */
	cc_memcpy(item_data(nit), item_data(oit), oit->nbyte);
	cc_memcpy(item_data(nit) + oit->nbyte, item_data(it), it->nbyte);

	/* Replace in hash */
	_item_relink(oit, nit);

	_item_remove(nit);
    }

    _item_remove(oit);
    return ANNEX_OK;
}
#endif

#if defined CC_HAVE_CHAINED && CC_HAVE_CHAINED == 1
static item_annex_result_t
_item_prepend(struct item *it)
{
    uint8_t *key;
    struct item *oit, *nit = NULL;
    uint8_t nid;
    uint32_t total_nbyte;

    if(item_is_chained(it)) {
	return ANNEX_OVERSIZED;
    }
    ASSERT(it->next_node == NULL);

    /* Get item to be prepended to */
    key = item_key(it);
    oit = _item_get(key, it->nkey);
    if(oit == NULL) {
	return ANNEX_NOT_FOUND;
    }

    ASSERT(!item_is_slabbed(oit));

    /* Get new number of bytes and id */
    total_nbyte = oit->nbyte + it->nbyte;
    nid = item_slabid(oit->nkey, total_nbyte);

    if(nid == item_id(oit) && item_is_raligned(oit)) {
	/* oit head is raligned, and can contain the new data. Simply
	   prepend the data at the beginning of oit, not needing to
	   allocate more space. */
	item_prepend_same_id(oit, it, total_nbyte);
    } else if(nid != SLABCLASS_CHAIN_ID) {
	/* Additional data can fit in a single larger head node */
	struct item *iter;

	nit = _item_alloc(oit->nkey, oit->exptime, total_nbyte);

	if(nit == NULL) {
	    /* Could not allocate larger node */
	    _item_remove(oit);
	    return ANNEX_EOM;
	}

	/* Right align nit, copy over data */
	nit->flags |= ITEM_RALIGN;
	cc_memcpy(item_key(nit), item_key(oit), oit->nkey);
	cc_memcpy(item_data(nit), item_data(it), it->nbyte);
	cc_memcpy(item_data(nit) + it->nbyte, item_data(oit), oit->nbyte);

	/* Replace oit with nit in all nodes */
	ASSERT(nit->next_node == NULL);

	nit->next_node = oit->next_node;
	for(iter = nit; iter != NULL; iter = iter->next_node) {
	    iter->head = nit;
	}

	_item_relink(oit, nit);
    } else {
	struct item *iter, *nit_second;
	uint32_t nit_second_nbyte = slab_item_size(slabclass_max_id) -
	    ITEM_HDR_SIZE;

	nit_second = _item_alloc(0, oit->exptime, nit_second_nbyte);

	if(nit_second == NULL) {
	    _item_remove(oit);
	    return ANNEX_EOM;
	}

	ASSERT(!item_is_chained(nit_second));
	ASSERT(nit_second->next_node == NULL);

	nit = _item_alloc(oit->nkey, oit->exptime, total_nbyte - nit_second_nbyte);

	if(nit == NULL) {
	    _item_remove(oit);
	    _item_remove(nit_second);
	    return ANNEX_EOM;
	}

	ASSERT(!item_is_chained(nit));
	ASSERT(nit->next_node == NULL);
	ASSERT(nit->nbyte <= it->nbyte);

	/* copy over oit key to nit key */
	cc_memcpy(item_key(nit), item_key(oit), oit->nkey);

	/* copy the first nit->nbyte bytes of it to nit */
	cc_memcpy(item_data(nit), item_data(it), nit->nbyte);

	/* Copy the rest to nit_second */
	cc_memcpy(item_data(nit_second), item_data(it) + nit->nbyte,
		  it->nbyte - nit->nbyte);
	cc_memcpy(item_data(nit_second) + it->nbyte - nit->nbyte,
		  item_data(oit), oit->nbyte);

	/* Reassemble item */
	nit->next_node = nit_second;
	nit_second->next_node = oit->next_node;
	for(iter = nit; iter != NULL; iter = iter->next_node) {
	    iter->head = nit;
	}
	_item_relink(oit, nit);
    }

    if(oit != NULL) {
	_item_remove(oit);
    }

    if(nit != NULL) {
	_item_remove(nit);
    }

    return ANNEX_OK;
}
#else
static item_annex_result_t
_item_prepend(struct item *it)
{
    uint8_t *key;
    struct item *oit, *nit = NULL;
    uint8_t nid;
    uint32_t total_nbyte;

    key = item_key(it);
    oit = _item_get(key, it->nkey);
    if(oit == NULL) {
	return ANNEX_NOT_FOUND;
    }

    ASSERT(!item_is_slabbed(oit));

    total_nbyte = oit->nbyte + it->nbyte;
    nid = item_slabid(oit->nkey, total_nbyte);

    if(nid == item_id(oit) && item_is_raligned(oit)) {
	/* oit head is raligned, and can contain the new data. Simply
	   prepend the data at the beginning of oit, not needing to
	   allocate more space */
	item_prepend_same_id(oit, it, total_nbyte);
    } else if(nid != SLABCLASS_CHAIN_ID) {
	nit = _item_alloc(oit->nkey, oit->exptime, total_nbyte);

	if(nit == NULL) {
	    _item_remove(oit);
	    return ANNEX_EOM;
	}

	/* Right align nit, copy over key and data */
	nit->flags |= ITEM_RALIGN;
	cc_memcpy(item_key(nit), item_key(oit), oit->nkey);
	cc_memcpy(item_data(nit), item_data(it), it->nbyte);
	cc_memcpy(item_data(nit) + it->nbyte, item_data(oit), oit->nbyte);

	_item_relink(oit, nit);
    } else {
	_item_remove(oit);
	return ANNEX_OVERSIZED;
    }

    if(oit != NULL) {
	_item_remove(oit);
    }

    if(nit != NULL) {
	_item_remove(nit);
    }

    return ANNEX_OK;
}
#endif

/*
 * Apply a delta value (positive or negative) to an item. (increment/decrement)
 */
static item_delta_result_t
_item_delta(uint8_t *key, size_t nkey, bool incr, uint64_t delta)
{
    item_delta_result_t ret = DELTA_OK;
    int res;
    uint8_t *ptr;
    struct item *it;
    uint8_t buf[INCR_MAX_STORAGE_LEN];
    uint64_t value;

    it = _item_get(key, nkey);
    if (it == NULL) {
        return DELTA_NOT_FOUND;
    }

    /* it is not NULL, needs to have reference count decremented */

#if defined CC_HAVE_CHAINED && CC_HAVE_CHAINED == 1
    if(item_is_chained(it)) {
	_item_remove(it);
	return DELTA_CHAINED;
    }
#endif

    ptr = item_data(it);

    if (!strtoull_len(ptr, &value, it->nbyte)) {
	_item_remove(it);
	return DELTA_NON_NUMERIC;
    }

    if (incr) {
        value += delta;
    } else if (delta > value) {
        value = 0;
    } else {
        value -= delta;
    }

    res = cc_scnprintf(buf, INCR_MAX_STORAGE_LEN, "%"PRIu64, value);
    ASSERT(res < INCR_MAX_STORAGE_LEN);
    if (res > it->nbyte) { /* need to realloc */
        struct item *new_it;

        new_it = _item_alloc(it->nkey, it->exptime, res);

        if (new_it == NULL) {
	    _item_remove(it);
	    return DELTA_EOM;
        }

	cc_memcpy(item_key(new_it), item_key(it), it->nkey);
        cc_memcpy(item_data(new_it), buf, res);
        _item_relink(it, new_it);
        _item_remove(it);
        it = new_it;
    } else {
        /*
         * Replace in-place - when changing the value without replacing
         * the item, we need to update the CAS on the existing item
         */
        item_set_cas(it, item_next_cas());
        cc_memcpy(item_data(it), buf, res);
        it->nbyte = res;
    }

    _item_remove(it);

    return ret;
}

/*
 * Append nit data to oit, assuming it will already fit in oit and assuming oit
 * is left aligned.
 */
static void
item_append_same_id(struct item *oit, struct item *it, uint32_t total_nbyte)
{
    ASSERT(!item_is_raligned(oit));

    cc_memcpy(item_data(oit) + oit->nbyte, item_data(it), it->nbyte);
    oit->nbyte = total_nbyte;
    item_set_cas(oit, item_next_cas());
}

/*
 * Prepend nit data to oit, assuming it will already fit in oit and assuming oit
 * is right aligned
 */
static void
item_prepend_same_id(struct item *oit, struct item *it, uint32_t total_nbyte)
{
    ASSERT(item_is_raligned(oit));

    cc_memcpy(item_data(oit) - it->nbyte, item_data(it), it->nbyte);
    oit->nbyte = total_nbyte;
    item_set_cas(oit, item_next_cas());
}

#if defined CC_HAVE_CHAINED && CC_HAVE_CHAINED == 1
/* Prepare nit to be the tail of a chained object */
static void
item_prepare_tail(struct item *nit)
{
    /* set nit flags */
    nit->flags |= ITEM_CHAINED;

    /* Set refcount to zero */
    nit->refcount = 0;
}

/* Returns true if candidate is a node in it, false otherwise */
static bool
item_is_contained(struct item *it, struct item *candidate)
{
    if(it == NULL) {
	return false;
    }

    for(; it != NULL; it = it->next_node) {
	if(candidate == it) {
	    return true;
	}
    }
    return false;
}
#endif
