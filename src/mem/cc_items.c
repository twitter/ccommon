/*
 * twemcache - Twitter memcached.
 * Copyright (c) 2012, Twitter, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of the Twitter nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "cc_items.h"
#include "cc_settings.h"
#include "cc_assoc.h"
#include "cc_util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern struct settings settings;

/*pthread_mutex_t cache_lock;*/                     /* lock protecting lru q and hash */
static uint64_t cas_id;                         /* unique cas id */

/*
 * Returns the next cas id for a new item. Minimum cas value
 * is 1 and the maximum cas value is UINT64_MAX
 */
static uint64_t
item_next_cas(void)
{
    if (settings.use_cas) {
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
    assert(it->magic == ITEM_MAGIC);

    return (it->exptime > 0 && it->exptime < time_now()) ? true : false;
}

/*
 * Initialize cache_lock and cas_id
 */
void
item_init(void)
{
    fprintf(stderr, "item header size: %zu\n", ITEM_HDR_SIZE);

    /*pthread_mutex_init(&cache_lock, NULL);*/

    cas_id = 0ULL;
}

void
item_deinit(void)
{
}

/*
 * Get start location of item payload
 */
char *
item_data(struct item *it)
{
    char *data;

    assert(it->magic == ITEM_MAGIC);

    if (item_is_raligned(it)) {
        data = (char *)it + slab_item_size(it->id) - it->nbyte;
    } else {
        data = it->end + it->nkey + 1; /* 1 for terminal '\0' in key */
        if (item_has_cas(it)) {
            data += sizeof(uint64_t);
        }
    }

    return data;
}

/*
 * Get the slab that contains this item.
 */
struct slab *
item_2_slab(struct item *it)
{
    struct slab *slab;

    assert(it->magic == ITEM_MAGIC);
    assert(it->offset < settings.slab_size);

    slab = (struct slab *)((uint8_t *)it - it->offset);

    assert(slab->magic == SLAB_MAGIC);

    return slab;
}

/*
 * With the current system, the refcount for each slab might not be completely
 * accurate, since if there are 2 items chained in the same slab, the number of
 * references to the slab increases twice when the item is referenced once.
 * However, this should not cause any issues as of right now, since we only use
 * refcount to see if anybody is using the slab.
 */

/*
 * Increment the number of references to the given item
 */
static void
item_acquire_refcount(struct item *it)
{
    /*assert(pthread_mutex_trylock(&cache_lock) != 0);*/
    assert(it->magic == ITEM_MAGIC);

    it->refcount++;

    for(; it != NULL; it = it->next_node) {
	slab_acquire_refcount(item_2_slab(it));
    }
}

/*
 * Decrement the number of references to the given item
 */
static void
item_release_refcount(struct item *it)
{
    /*assert(pthread_mutex_trylock(&cache_lock) != 0);*/
    assert(it->magic == ITEM_MAGIC);
    assert(it->refcount > 0);

    it->refcount--;

    for(; it != NULL; it = it->next_node) {
	slab_release_refcount(item_2_slab(it));
    }
}

/*
 * Initialize item header
 */
void
item_hdr_init(struct item *it, uint32_t offset, uint8_t id)
{
    assert(offset >= SLAB_HDR_SIZE && offset < settings.slab_size);

    it->magic = ITEM_MAGIC;
    it->offset = offset;
    it->id = id;
    it->refcount = 0;
    it->flags = 0;
    it->next_node = NULL;
    it->head = NULL;
}

/*
 * Free an item by putting it on the free queue for the slab class
 */
static void
item_free(struct item *it)
{
    assert(it->magic == ITEM_MAGIC);
    assert(!item_is_linked(it));

    fprintf(stderr, "Freeing item %s...\n", item_key(it));

    /* Keep two pointers to the chain of items, one to do the freeing (prev) and
       the other to keep a handle on the rest of the chain (it) */
    struct item *prev = it;

    for (prev = it; prev != NULL; prev = it) {
	/* Advance it, if it is not already at the end of the chain */
	if(it != NULL) {
	    it = it->next_node;
	}

	assert(!item_is_linked(prev));
	assert(!item_is_slabbed(prev));
	assert(prev->refcount == 0);

	/* Free prev */
	prev->flags &= ~ITEM_CHAINED;
	prev->next_node = NULL;
	prev->head = NULL;
	slab_put_item(prev);
    }
}

/*
 * Make an item with zero refcount available for reuse by unlinking
 * it from the hash.
 *
 * Don't free the item yet because that would make it unavailable
 * for reuse.
 */

/*
 * It may be possible to reallocate individual nodes of chained items, so that
 * evicting a slab does not necessarily evict all items with nodes in that
 * slab; this is a possibility worth considering later
 */
void
item_reuse(struct item *it)
{
    struct item *prev;
    struct slab *evicted = item_2_slab(it);

    /*assert(pthread_mutex_trylock(&cache_lock) != 0);*/
    assert(it->magic == ITEM_MAGIC);
    assert(!item_is_slabbed(it));
    assert(item_is_linked(it->head));
    assert(it->head->refcount == 0);

    it->head->flags &= ~ITEM_LINKED;
    assoc_delete(item_key(it->head), it->head->nkey);

    for(prev = it = it->head; prev != NULL; prev = it) {
	if(it != NULL) {
	    it = it->next_node;
	}

	prev->next_node = NULL;
	prev->head = NULL;

	/* If the node is not in the slab we are evicting, free it */
	if(item_2_slab(prev) != evicted) {
	    item_free(prev);
	}
    }

    fprintf(stderr, "reuse %s item %s at offset %d with id %hhu",
	    item_expired(it) ? "expired" : "evicted", item_key(it),
	    it->offset, it->id);
}

uint8_t item_slabid(uint8_t nkey, uint32_t nbyte)
{
    size_t ntotal;
    uint8_t id;

    ntotal = item_ntotal(nkey, nbyte, settings.use_cas);

    id = slab_id(ntotal);

    return id;
}

/*
 * Allocate an item. We allocate an item by consuming the next free item from
 * the item's slab class.
 */

static struct item *
_item_alloc(char *key, uint8_t nkey, uint32_t dataflags, rel_time_t
	    exptime, uint32_t nbyte)
{
    struct item *it = NULL;
    struct item *current_node, *prev_node = NULL;
    uint8_t id;

    do {
	/* Get slab id based on nbyte, and nkey if first node */
	id = slab_id(item_ntotal(
			 (it == NULL) ? nkey : 0, nbyte,
			 (it == NULL) ? settings.use_cas : false));

	/* If remaining data cannot fit into a single item, allocate the
	   biggest item possible */
	if (id == SLABCLASS_INVALID_ID) {
	    id = slabclass_max_id;
	}

	current_node = slab_get_item(id);

	if(current_node == NULL) {
	    /* Could not successfully allocate item(s) */
	    fprintf(stderr, "server error on allocating item in slab %hhu\n",
		    id);
	    return NULL;
	}

	if(it == NULL) {
	    it = current_node;
	}

	assert(current_node->id == id);
	assert(!item_is_linked(current_node));
	assert(!item_is_slabbed(current_node));
	assert(current_node->offset != 0);
	assert(current_node->refcount == 0);
	assert(current_node->next_node == NULL);
	assert(current_node->head == NULL);

	/* Default behavior is to set the chained flag to true, we double check
	   for this after the loop finishes */
	current_node->flags |= ITEM_CHAINED;
	current_node->head = it;

	current_node->dataflags = dataflags;
	current_node->exptime = exptime;

	if(current_node == it) {
	    /* If this is the first node in the chain */
	    current_node->nkey = nkey;
	} else {
	    current_node->nkey = 0;
	}

	/* set key ('\0' by default) */
	*(item_key(current_node)) = '\0';

	/* Set nbyte equal to either the number of bytes left or the number
	   of bytes that can fit in the item, whichever is less */
	current_node->nbyte =
	    (nbyte < slab_item_size(id) - ITEM_HDR_SIZE - current_node->nkey - 1) ?
	    nbyte : slab_item_size(id) - ITEM_HDR_SIZE - current_node->nkey - 1;

	nbyte -= current_node->nbyte;
	fprintf(stderr, "bytes allocated for this node: %u\n", current_node->nbyte);

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

    if(settings.use_cas) {
	it->flags |= ITEM_CAS;
    }

    /* copy item key */
    memcpy(item_key(it), key, nkey);
    *(item_key(it) + nkey) = '\0';

    item_set_cas(it, 0);

    fprintf(stderr, "alloc item %s at offset %u with id %hhu expiry %u "
	    " refcount %hu\n", item_key(it), it->offset, it->id, it->exptime,
	    it->refcount);

    return it;
}

struct item *
item_alloc(char *key, uint8_t nkey, uint32_t dataflags,
           rel_time_t exptime, uint32_t nbyte)
{
    struct item *it;

    /*pthread_mutex_lock(&cache_lock);*/
    it = _item_alloc(key, nkey, dataflags, exptime, nbyte);
    /*pthread_mutex_unlock(&cache_lock);*/

    return it;
}

/*
 * Link an item into the hash table. In the case of a chained item, only the
 * head gets linked.
 */
static void
_item_link(struct item *it)
{
    assert(it->magic == ITEM_MAGIC);
    assert(!item_is_linked(it));
    assert(!item_is_slabbed(it));
    assert(it->nkey != 0);
    assert(it->head == it);

    fprintf(stderr, "link item %s at offset %u with flags %hhu id %hhu\n",
	    item_key(it), it->offset, it->flags, it->id);

    it->flags |= ITEM_LINKED;
    item_set_cas(it, item_next_cas());

    assoc_insert(it);
    //item_link_q(it, true);
}

/*
 * Unlinks an item from the hash table. Free an unlinked
 * item if it's refcount is zero.
 */
static void
_item_unlink(struct item *it)
{
    assert(it->magic == ITEM_MAGIC);
    assert(it->head == it);

    fprintf(stderr, "unlink item %s at offset %u with flags %hhu id %hhu\n",
	    item_key(it), it->offset, it->flags, it->id);

    if (item_is_linked(it)) {
        it->flags &= ~ITEM_LINKED;

        assoc_delete(item_key(it), it->nkey);

        //item_unlink_q(it);

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
    assert(it->magic == ITEM_MAGIC);
    assert(!item_is_slabbed(it));
    assert(it->head == it);

    fprintf(stderr, "remove item %s at offset %u with flags %hhu id %hhu "
	    "refcount %hu\n", item_key(it), it->offset, it->flags, it->id,
	    it->refcount);

    if (it->refcount != 0) {
        item_release_refcount(it);
    }

    if (it->refcount == 0 && !item_is_linked(it)) {
        item_free(it);
    }
}

void
item_remove(struct item *it)
{
    /*pthread_mutex_lock(&cache_lock);*/
    _item_remove(it);
    /*pthread_mutex_unlock(&cache_lock);*/
}

/*
 * Replace one item with another in the hash table.
 */
static void
_item_relink(struct item *it, struct item *nit)
{
    assert(it->magic == ITEM_MAGIC);
    assert(!item_is_slabbed(it));
    assert(it->head == it);

    assert(nit->magic == ITEM_MAGIC);
    assert(!item_is_slabbed(nit));
    assert(nit->head == nit);

    fprintf(stderr, "relink item %s at offset %u id %hhu with one at offset "
	    "%u id %hhu\n", item_key(it), it->offset, it->id, nit->offset,
	    nit->id);

    _item_unlink(it);
    _item_link(nit);
}

/*** TODO: consider putting expired items that we encounter here directly into
     the free queue ***/

/*
 * Return an item if it hasn't been marked as expired, lazily expiring
 * item as-and-when needed
 *
 * When a non-null item is returned, it's the callers responsibily to
 * release refcount on the item
 */
static struct item *
_item_get(const char *key, size_t nkey)
{
    struct item *it;

    it = assoc_find(key, nkey);
    if (it == NULL) {
	fprintf(stderr, "get item %s not found\n", key);
        return NULL;
    }

    assert(it->head == it);

    if (it->exptime != 0 && it->exptime <= time_now()) {
        _item_unlink(it);
	fprintf(stderr, "get item %s expired and nuked\n", key);
        return NULL;
    }

    if (settings.oldest_live != 0 && settings.oldest_live <= time_now() &&
        it->atime <= settings.oldest_live) {
        _item_unlink(it);
	fprintf(stderr, "item %s nuked\n", key);
        return NULL;
    }

    item_acquire_refcount(it);

    fprintf(stderr, "Refcounts: ");
    for(struct item *iter = it; iter != NULL; iter = iter->next_node) {
	fprintf(stderr, "%hu ", iter->refcount);
    }
    fprintf(stderr, "\n");

    fprintf(stderr, "get item %s found at offset %u with flags %hhu id %hhu\n",
	    item_key(it), it->offset, it->flags, it->id);

    return it;
}

struct item *
item_get(const char *key, size_t nkey)
{
    struct item *it;

    /*pthread_mutex_lock(&cache_lock);*/
    it = _item_get(key, nkey);
    /*pthread_mutex_unlock(&cache_lock);*/

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
    char *key;
    struct item *oit;

    assert(it->head == it);

    key = item_key(it);
    oit = _item_get(key, it->nkey);
    if (oit == NULL) {
        _item_link(it);
    } else {
        _item_relink(oit, it);
        _item_remove(oit);
    }

    fprintf(stderr, "store item %s at offset %u with flags %hhu id %hhu\n",
	    item_key(it), it->offset, it->flags, it->id);
}

void
item_set(struct item *it)
{
    /*pthread_mutex_lock(&cache_lock);*/
    _item_set(it);
    /*pthread_mutex_unlock(&cache_lock);*/
}

/*
 * Perform a check-and-set; check if the item has been updated since last time
 * the client fetched it; if no, then replace it.
 */
static item_cas_result_t
_item_cas(struct item *it)
{
    char *key;
    struct item *oit;

    key = item_key(it);
    oit = _item_get(key, it->nkey);
    if (oit == NULL) {
	return CAS_NOT_FOUND;
    }

    /* oit is not NULL, some item was found */
    if (item_get_cas(it) != item_get_cas(oit)) {
	fprintf(stderr, "cas mismatch %llu != %llu on item %s\n",
		item_get_cas(oit), item_get_cas(it), item_key(it));
	_item_remove(oit);
	return CAS_EXISTS;
    }

    _item_relink(oit, it);
    fprintf(stderr, "cas item %s at offset %u with flags %hhu id %hhu\n",
	    item_key(it), it->offset, it->flags, it->id);
    _item_remove(oit);
    return CAS_OK;
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

/*
 * Store this data, but only if the server does not already hold data for this
 * key.
 */
static item_add_result_t
_item_add(struct item *it)
{
    item_add_result_t ret;
    char *key;
    struct item *oit;

    assert(it->head == it);

    key = item_key(it);
    oit = _item_get(key, it->nkey);
    if (oit != NULL) {
        _item_remove(oit);

        ret = ADD_EXISTS;
    } else {
        _item_link(it);

        ret = ADD_OK;

	fprintf(stderr, "add item %s at offset %u with flags %hhu id %hhu\n",
		item_key(it), it->offset, it->flags, it->id);
    }

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

/*
 * Store this data, but only if the server already holds data for this key.
 */
static item_replace_result_t
_item_replace(struct item *it)
{
    item_replace_result_t ret;
    char *key;
    struct item *oit;

    assert(it->head == it);

    key = item_key(it);
    oit = _item_get(key, it->nkey);
    if (oit == NULL) {
        ret = REPLACE_NOT_FOUND;
    } else {
	fprintf(stderr, "replace oit %s at offset %u with flags %hhu id %hhu\n",
		item_key(oit), oit->offset, oit->flags, oit->id);

        _item_relink(oit, it);
        _item_remove(oit);

        ret = REPLACE_OK;
    }

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

/* Get the last node in an item chain. */
static struct item *
item_tail(struct item *it)
{
    assert(it != NULL);
    for(; it->next_node != NULL; it = it->next_node);
    return it;
}

static item_annex_result_t
_item_append(struct item *it)
{
    char *key;
    struct item *oit, *oit_tail, *nit = NULL;
    uint8_t nid;
    uint32_t total_nbyte;

    if(item_is_chained(it)) {
	return ANNEX_OVERSIZED;
    }
    assert(it->next_node == NULL);

    key = item_key(it);
    oit = _item_get(key, it->nkey);
    if(oit == NULL) {
	return ANNEX_NOT_FOUND;
    }

    assert(!item_is_slabbed(oit));

    oit_tail = item_tail(oit);

    /* Find out the new size of the tail */
    total_nbyte = oit_tail->nbyte + it->nbyte;
    nid = item_slabid(oit_tail->nkey, total_nbyte);

    if(nid == oit_tail->id && !item_is_raligned(oit_tail)) {
	/* if oit is large enough to hold the extra data and left-aligned,
	 * which is the default behavior, we copy the delta to the end of
	 * the existing data. Otherwise, allocate a new item and store the
	 * payload left-aligned.
	 */
	memcpy(item_data(oit_tail) + oit->nbyte, item_data(it), it->nbyte);

	/* Set the new data size and cas */
	oit->nbyte = total_nbyte;
	item_set_cas(oit, item_next_cas());

	/* oit refcount must be decremented */
    } else {
	/* Append command where a new item needs to be allocated */
	nit = _item_alloc(item_key(oit_tail), oit_tail->nkey,
			  oit->dataflags, oit->exptime, total_nbyte);

	if(nit == NULL) {
	    _item_remove(oit);
	    return ANNEX_EOM;
	}

	/* Regardless of whether nid == SLABCLASS_INVALID_ID or not, nit's
	   first node should be able to contain at least all of oit's tail
	   node */
	memcpy(item_data(nit), item_data(oit_tail), oit_tail->nbyte);

	if(!item_is_chained(nit)) {
	    /* Only one additional node was allocated, so the entirety of
	       it should be able to fit in the remainder of nit */
	    memcpy(item_data(nit) + oit_tail->nbyte, item_data(it),
		   it->nbyte);
	} else {
	    /* One node was not enough, so nit contains 2 nodes. */
	    assert(nit->next_node != NULL);

	    /* Two additional nodes were allocated, so fill the first one
	       then the second */
	    uint32_t nit_amount_copied = slab_item_size(nit->id)
		- ITEM_HDR_SIZE - oit_tail->nbyte - oit_tail->nkey - 1;

	    memcpy(item_data(nit) + oit_tail->nbyte, item_data(it),
		   nit_amount_copied);

	    /* Copy the remaining data into the second node */
	    memcpy(item_data(nit->next_node), item_data(it) + nit_amount_copied,
		   it->nbyte - nit_amount_copied);
	}

	if(!item_is_chained(oit)) {
	    _item_relink(oit, nit);
	    /* Both oit and nit should have refcount decremented */
	    _item_remove(nit);
	} else {
	    struct item *nit_prev;

	    assert(oit->next_node != NULL);

	    /* nit will be the new tail of oit, so set the chained flag */
	    nit->flags |= ITEM_CHAINED;
	    nit->flags &= ~ITEM_RALIGN;

	    /* set nit_prev to the second to last node of the oit chain */
	    for(nit_prev = oit; nit_prev->next_node->next_node != NULL;
		nit_prev = nit_prev->next_node);

	    /* make nit the new tail of oit */
	    nit_prev->next_node = nit;

	    /* decrement nit refcount, since we used item_alloc */
	    nit->refcount--;

	    /* set head in all new nodes */
	    for(struct item *iter = nit; iter != NULL; iter = iter->next_node) {
		iter->head = oit;
	    }

	    /* free oit_tail, since it is no longer used by anybody */
	    slab_release_refcount(item_2_slab(oit_tail));
	    item_free(oit_tail);
	}
    }

    fprintf(stderr, "annex successfully to item %s, new id %hhu\n",
	    item_key(oit), nid);

    if(oit != NULL) {
	fprintf(stderr, "removing oit\n");
	_item_remove(oit);
    }

    return ANNEX_OK;
}

item_annex_result_t
item_append(struct item *it)
{
    item_annex_result_t ret;
    /*pthread_mutex_lock(&cache_lock);*/
    ret = _item_append(it);
    /*pthread_mutex_unlock(&cache_lock);*/

    return ret;
}

static item_annex_result_t
_item_prepend(struct item *it)
{
    char *key;
    struct item *oit, *nit = NULL;
    uint8_t nid;
    uint32_t total_nbyte;

    if(item_is_chained(it)) {
	return ANNEX_OVERSIZED;
    }
    assert(it->next_node == NULL);

    key = item_key(it);
    oit = _item_get(key, it->nkey);
    if(oit == NULL) {
	return ANNEX_NOT_FOUND;
    }

    assert(!item_is_slabbed(oit));

    total_nbyte = oit->nbyte + it->nbyte;
    nid = item_slabid(oit->nkey, total_nbyte);

    if(nid == oit->id && item_is_raligned(oit)) {
	/* oit head is raligned, and can contain the new data. Simply
	   prepend the data at the beginning of oit, not needing to
	   allocate more space. */
	memcpy(item_data(oit) - it->nbyte, item_data(it), it->nbyte);
	oit->nbyte = total_nbyte;
	item_set_cas(oit, item_next_cas());
    } else if(nid != SLABCLASS_INVALID_ID) {
	/* only one larger node is needed to contain the data */
	nit = _item_alloc(item_key(oit), oit->nkey, oit->dataflags,
			  oit->exptime, total_nbyte);

	if(nit == NULL) {
	    _item_remove(oit);
	    return ANNEX_EOM;
	}

	assert(nit->next_node == NULL);

	/* Right align nit, copy over data */
	nit->flags |= ITEM_RALIGN;
	memcpy(item_data(nit), item_data(it), it->nbyte);
	memcpy(item_data(nit) + it->nbyte, item_data(oit), oit->nbyte);

	/* Replace oit with nit */
	nit->next_node = oit->next_node;
	for(struct item *iter = nit; iter != NULL; iter = iter->next_node) {
	    iter->head = nit;
	}
	_item_relink(oit, nit);
    } else {
	/* One node is not enough to contain original data + new data. First
	   allocate a node with id slabclass_max_id then another smaller
	   node to contain the head */
	nit = _item_alloc(item_key(oit), oit->nkey, oit->dataflags,
			  oit->exptime, total_nbyte);

	if(nit == NULL) {
	    _item_remove(oit);
	    return ANNEX_EOM;
	}

	assert(nit->next_node->next_node == NULL);

	if(nit->next_node->nbyte < oit->nbyte) {
	    /* nit->next cannot contain all of oit; copy the last
	       nit->next->nbyte bytes of oit to nit->next */
	    memcpy(item_data(nit->next_node), item_data(oit) + oit->nbyte -
		   nit->next_node->nbyte, nit->next_node->nbyte);

	    /* copy the rest to nit */
	    memcpy(item_data(nit), item_data(it), it->nbyte);
	    memcpy(item_data(nit) + it->nbyte, item_data(oit), oit->nbyte -
		   nit->next_node->nbyte);
	} else {
	    /* nit->next can contain all of oit; copy all of oit into
	       nit->next, along with as much of it as will fit */
	    memcpy(item_data(nit->next_node), item_data(it) + nit->nbyte,
		   it->nbyte - nit->nbyte);
	    memcpy(item_data(nit->next_node) + it->nbyte - nit->nbyte,
		   item_data(oit), oit->nbyte);

	    memcpy(item_data(nit), item_data(it), nit->nbyte);
	}

	nit->next_node->next_node = oit->next_node;
	for(struct item *iter = nit; iter != NULL; iter = iter->next_node) {
	    iter->head = nit;
	}
	_item_relink(oit, nit);
    }

    fprintf(stderr, "annex successfully to item %s\n", item_key(oit));

    if(oit != NULL) {
	_item_remove(oit);
    }

    if(nit != NULL) {
	_item_remove(nit);
    }

    return ANNEX_OK;
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

/*
 * Apply a delta value (positive or negative) to an item. (increment/decrement)
 */
static item_delta_result_t
_item_delta(char *key, size_t nkey, bool incr, uint64_t delta)
{
    item_delta_result_t ret = DELTA_OK;
    int res;
    char *ptr;
    struct item *it;
    char buf[INCR_MAX_STORAGE_LEN];
    uint64_t value;

    it = _item_get(key, nkey);
    if (it == NULL) {
        return DELTA_NOT_FOUND;
    }

    assert(it->head == it);

    /* it is not NULL, needs to have reference count decremented */

    ptr = item_data(it);

    if (!mc_strtoull_len(ptr, &value, it->nbyte)) {
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

    res = snprintf(buf, INCR_MAX_STORAGE_LEN, "%llu", value);
    assert(res < INCR_MAX_STORAGE_LEN);
    if (res > it->nbyte) { /* need to realloc */
        struct item *new_it;

        new_it = _item_alloc(item_key(it), it->nkey, it->dataflags,
                             it->exptime, res);
        if (new_it == NULL) {
	    _item_remove(it);
	    return DELTA_EOM;
        }

        memcpy(item_data(new_it), buf, res);
        _item_relink(it, new_it);
        _item_remove(it);
        it = new_it;
    } else {
        /*
         * Replace in-place - when changing the value without replacing
         * the item, we need to update the CAS on the existing item
         */
        item_set_cas(it, item_next_cas());
        memcpy(item_data(it), buf, res);
        it->nbyte = res;
    }

    _item_remove(it);

    return ret;
}

item_delta_result_t
item_delta(char *key, size_t nkey, bool incr, uint64_t delta)
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
item_delete(char *key, size_t nkey)
{
    item_delete_result_t ret = DELETE_OK;
    struct item *it;

    /*pthread_mutex_lock(&cache_lock);*/
    it = _item_get(key, nkey);
    assert(it->head == it);
    if (it != NULL) {
        _item_unlink(it);
        _item_remove(it);
    } else {
        ret = DELETE_NOT_FOUND;
    }
    /*pthread_mutex_unlock(&cache_lock);*/

    return ret;
}

uint32_t
item_total_nbyte(struct item *it) {
    uint32_t nbyte = 0;

    assert(it->head == it);

    for(; it != NULL; it = it->next_node) {
	nbyte += it->nbyte;
    }

    return nbyte;
}
