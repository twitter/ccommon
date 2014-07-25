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

#include <mem/cc_slabs.h>

#include <mem/cc_items.h>
#include <mem/cc_settings.h>
#include <cc_debug.h>
#include <cc_log.h>
#include <cc_mm.h>

#include <stdlib.h>

/*
 * Struct containing information regarding the slab structure on the heap
 */
struct slab_heapinfo {
    uint8_t         *base;       /* prealloc base */
    uint8_t         *curr;       /* prealloc start */
    uint32_t        nslab;       /* # slab allocated */
    uint32_t        max_nslab;   /* max # slab allowed */
    struct slab     **slab_table;/* table of all slabs */
    struct slab_tqh slab_lruq;   /* lru slab q */
};

/* collection of slabs bucketed by slabclass */
struct slabclass slabclass[SLABCLASS_MAX_IDS];

uint8_t slabclass_max_id;                 /* maximum slabclass id */
static struct slab_heapinfo heapinfo;     /* info of all allocated slabs */
/*pthread_mutex_t slab_lock;*/            /* lock protecting slabclass and heapinfo */

#define SLAB_RAND_MAX_TRIES         50
#define SLAB_LRU_MAX_TRIES          50
#define SLAB_LRU_UPDATE_INTERVAL    1

static struct item *slab_2_item(struct slab *slab, uint32_t idx, size_t size);
static void slab_slabclass_init(void);
static void slab_slabclass_deinit(void);
static rstatus_t slab_heapinfo_init(void);
static void slab_heapinfo_deinit(void);
static void slab_hdr_init(struct slab *slab, uint8_t id);
static bool slab_heap_full(void);
static struct slab *slab_heap_alloc(void);
static void slab_table_update(struct slab *slab);
static struct slab *slab_table_rand(void);
static struct slab *slab_lruq_head();
static void slab_lruq_append(struct slab *slab);
static void slab_lruq_remove(struct slab *slab);
static struct slab *slab_get_new(void);
static void _slab_link_lruq(struct slab *slab);
static void _slab_unlink_lruq(struct slab *slab);
static void slab_evict_one(struct slab *slab);
static struct slab *slab_evict_rand(void);
static struct slab *slab_evict_lru(int id);
static void slab_add_one(struct slab *slab, uint8_t id);
static rstatus_t slab_get(uint8_t id);
static struct item *slab_get_item_from_freeq(uint8_t id);
static struct item *_slab_get_item(uint8_t id);
static void slab_put_item_into_freeq(struct item *it);
static void _slab_put_item(struct item *it);

size_t
slab_size(void)
{
    /* Currently, SLAB_HDR_SIZE == 32 */
    return settings.slab_size - SLAB_HDR_SIZE;
}

void
slab_print(void)
{
    uint8_t id;
    struct slabclass *p;

    log_stderr("slab size: %zu\nslab header size: %zu\nitem header size: %zu\n"
	   "total memory: %zu\n", settings.slab_size, SLAB_HDR_SIZE,
	   ITEM_HDR_SIZE, settings.maxbytes);

    /* Print out details for each slab class */
    for (id = SLABCLASS_MIN_ID; id <= slabclass_max_id; id++) {
        p = &slabclass[id];

	log_stderr("class: %hhu\nitems: %u\nsize: %zu\ndata: %zu\nslac: %zu\n",
	       id, p->nitem, p->size, p->size - ITEM_HDR_SIZE,
	       slab_size() - p->nitem * p->size);
    }
}

void
slab_acquire_refcount(struct slab *slab)
{
    log_stderr("acquiring refcount on slab with id %hhu refcount %hu",
	       slab->id, slab->refcount);
    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/
    ASSERT(slab->magic == SLAB_MAGIC);
    slab->refcount++;
    log_stderr("refcount now %hu", slab->refcount);
    if(slab->refcount > 100) {
	exit(1);
    }
}

void
slab_release_refcount(struct slab *slab)
{
    log_stderr("releasing refcount on slab with id %hhu refcount %hu",
	       slab->id, slab->refcount);
    /*ASSERT(pthread_mutex_trylock(&cache_lock) != 0);*/
    ASSERT(slab->magic == SLAB_MAGIC);
    ASSERT(slab->refcount > 0);
    slab->refcount--;
    log_stderr("refcount now %hu", slab->refcount);
}

size_t
slab_item_size(uint8_t id) {
    ASSERT(id >= SLABCLASS_MIN_ID && id <= slabclass_max_id);

    return slabclass[id].size;
}

/*
 * Return the id of the slab which can store an item of a given size.
 *
 * Return SLABCLASS_CHAIN_ID, for large items which cannot be stored in
 * any of the configured slabs.
 */
uint8_t
slab_id(size_t size)
{
    uint8_t id, imin, imax;

    ASSERT(size != 0);

    /* binary search */
    imin = SLABCLASS_MIN_ID;
    imax = slabclass_max_id;
    while (imax >= imin) {
        id = (imin + imax) / 2;
        if (size > slabclass[id].size) {
            imin = id + 1;
        } else if (id > SLABCLASS_MIN_ID && size <= slabclass[id - 1].size) {
            imax = id - 1;
        } else {
            break;
        }
    }

    if (imin > imax) {
        /* size too big for any slab */
        return SLABCLASS_CHAIN_ID;
    }

    return id;
}

rstatus_t
slab_init(void)
{
    rstatus_t status;

    /*pthread_mutex_init(&slab_lock, NULL);*/
    slab_slabclass_init();
    status = slab_heapinfo_init();

    return status;
}

void
slab_deinit(void)
{
    slab_heapinfo_deinit();
    slab_slabclass_deinit();
}

struct item *
slab_get_item(uint8_t id)
{
    struct item *it;

    ASSERT(id >= SLABCLASS_MIN_ID && id <= slabclass_max_id);

    /*pthread_mutex_lock(&slab_lock);*/
    it = _slab_get_item(id);
    /*pthread_mutex_unlock(&slab_lock);*/

    return it;
}

void
slab_put_item(struct item *it)
{
    /*pthread_mutex_lock(&slab_lock);*/
    _slab_put_item(it);
    /*pthread_mutex_unlock(&slab_lock);*/
}

void
slab_lruq_touch(struct slab *slab, bool allocated)
{
    /*
     * Check eviction option to make sure we adjust the order of slabs only if:
     * - request comes from allocating an item & lru slab eviction is specified, or
     * - lra slab eviction is specified
     */
    if(!(allocated && settings.evict_lru)) {
	return;
    }

    /* TODO: find out if we can remove this check w/o impacting performance */
    if (slab->utime >= (time_now() - SLAB_LRU_UPDATE_INTERVAL)) {
        return;
    }

    log_stderr("update slab with id %hhu in the slab lruq", slab->id);

    /*pthread_mutex_lock(&slab_lock);*/
    _slab_unlink_lruq(slab);
    _slab_link_lruq(slab);
    /*pthread_mutex_unlock(&slab_lock);*/
}

/*
 * Get the idx^th item with a given size from the slab.
 */
static struct item *
slab_2_item(struct slab *slab, uint32_t idx, size_t size)
{
    struct item *it;
    uint32_t offset = idx * size;

    ASSERT(slab->magic == SLAB_MAGIC);
    ASSERT(offset < settings.slab_size);

    it = (struct item *)((uint8_t *)slab->data + offset);

    return it;
}

/*
 * Initialize all slabclasses.
 *
 * Every slabclass is a collection of slabs of fixed size specified by
 * --slab-size. A single slab is a collection of contiguous, equal sized
 * item chunks of a given size specified by the settings.profile array
 */
static void
slab_slabclass_init(void)
{
    uint8_t id;      /* slabclass id */
    size_t *profile; /* slab profile */

    profile = settings.profile;
    slabclass_max_id = settings.profile_last_id;

    ASSERT(slabclass_max_id <= SLABCLASS_MAX_ID);

    for (id = SLABCLASS_MIN_ID; id <= slabclass_max_id; id++) {
        struct slabclass *p; /* slabclass */
        uint32_t nitem;      /* # item per slabclass */
        size_t item_sz;      /* item size */

        nitem = slab_size() / profile[id];
        item_sz = profile[id];
        p = &slabclass[id];

        p->nitem = nitem;
        p->size = item_sz;

        p->nfree_itemq = 0;
        TAILQ_INIT(&p->free_itemq);

        p->nfree_item = 0;
        p->free_item = NULL;
    }
}

static void
slab_slabclass_deinit(void)
{
}

/*
 * Initialize slab heap related info
 *
 * When prelloc is true, the slab allocator allocates the entire heap
 * upfront. Otherwise, memory for new slabsare allocated on demand. But once
 * a slab is allocated, it is never freed, though a slab could be
 * reused on eviction.
 */
static rstatus_t
slab_heapinfo_init(void)
{
    heapinfo.nslab = 0;
    heapinfo.max_nslab = settings.maxbytes / settings.slab_size;

    heapinfo.base = NULL;
    if (settings.prealloc) {
        heapinfo.base = cc_alloc(heapinfo.max_nslab * settings.slab_size);
        if (heapinfo.base == NULL) {
	    log_stderr("pre-alloc %zu bytes for %u slabs failed",
		    heapinfo.max_nslab * settings.slab_size, heapinfo.max_nslab);
            return CC_ENOMEM;
        }

	log_stderr("pre-allocated %zu bytes for %u slabs",
		settings.maxbytes, heapinfo.max_nslab);
    }
    heapinfo.curr = heapinfo.base;

    heapinfo.slab_table = cc_alloc(sizeof(*heapinfo.slab_table) * heapinfo.max_nslab);
    if (heapinfo.slab_table == NULL) {
	log_stderr("creation of slab table with %u entries failed",
		heapinfo.max_nslab);
        return CC_ENOMEM;
    }
    TAILQ_INIT(&heapinfo.slab_lruq);

    log_stderr("created slab table with %u entries", heapinfo.max_nslab);

    return CC_OK;
}

static void
slab_heapinfo_deinit(void)
{
}

/* Initialize the header of the given slab */
static void
slab_hdr_init(struct slab *slab, uint8_t id)
{
    ASSERT(id >= SLABCLASS_MIN_ID && id <= slabclass_max_id);

#if defined HAVE_ASSERT_PANIC && HAVE_ASSERT_PANIC == 1 || defined HAVE_ASSERT_LOG && HAVE_ASSERT_LOG == 1
    slab->magic = SLAB_MAGIC;
#endif
    slab->id = id;
    slab->unused = 0;
    slab->refcount = 0;
}

/* Have we used all of the space we allocated on the heap? */
static bool
slab_heap_full(void)
{
    return (heapinfo.nslab >= heapinfo.max_nslab);
}

/* Get slab from heap */
static struct slab *
slab_heap_alloc(void)
{
    struct slab *slab;

    if (settings.prealloc) {
        slab = (struct slab *)heapinfo.curr;
        heapinfo.curr += settings.slab_size;
    } else {
        slab = cc_alloc(settings.slab_size);
    }

    return slab;
}

/* Update heap info after adding given slab */
static void
slab_table_update(struct slab *slab)
{
    ASSERT(heapinfo.nslab < heapinfo.max_nslab);

    heapinfo.slab_table[heapinfo.nslab] = slab;
    heapinfo.nslab++;

    log_stderr("new slab allocated at position %u", heapinfo.nslab - 1);
}

/* Get random slab from slab table */
static struct slab *
slab_table_rand(void)
{
    uint32_t rand_idx;

    rand_idx = (uint32_t)rand() % heapinfo.nslab;
    return heapinfo.slab_table[rand_idx];
}

/* Get the head of the slab LRU queue */
static struct slab *
slab_lruq_head()
{
    return TAILQ_FIRST(&heapinfo.slab_lruq);
}

/* Append given slab to the end of the LRU queue */
static void
slab_lruq_append(struct slab *slab)
{
    log_stderr("append slab with id %hhu to lruq", slab->id);
    TAILQ_INSERT_TAIL(&heapinfo.slab_lruq, slab, s_tqe);
}

/* Remove the given slab from the LRU queue */
static void
slab_lruq_remove(struct slab *slab)
{
    log_stderr("remove slab with id %hhu from lruq", slab->id);
    TAILQ_REMOVE(&heapinfo.slab_lruq, slab, s_tqe);
}

/* Get a raw slab from the slab pool. */
static struct slab *
slab_get_new(void)
{
    struct slab *slab;

    /* If the heap has reached maximum capacity or if we are unable to allocate
       another slab, return NULL */
    if (slab_heap_full()) {
        return NULL;
    }

    slab = slab_heap_alloc();
    if (slab == NULL) {
        return NULL;
    }

    slab_table_update(slab);

    return slab;
}

/* Link a slab into the LRU queue */
static void
_slab_link_lruq(struct slab *slab)
{
    slab->utime = time_now();
    slab_lruq_append(slab);
}

/* Unlink a slab from the LRU queue */
static void
_slab_unlink_lruq(struct slab *slab)
{
    slab_lruq_remove(slab);
}

/*
 * Evict a slab by evicting all the items within it. This means that the
 * items that are carved out of the slab must either be deleted from their
 * a) hash + lru Q, or b) free Q. The candidate slab itself must also be
 * delinked from its respective slab pool so that it is available for reuse.
 *
 * Eviction complexity is O(#items/slab).
 */
static void
slab_evict_one(struct slab *slab)
{
    struct slabclass *p;
    struct item *it;
    uint32_t i;

    log_stderr("@@@ slab_evict_one called");

    p = &slabclass[slab->id];

    ASSERT(slab->refcount == 0);

    /* candidate slab is also the current slab */
    if (p->free_item != NULL && slab == item_2_slab(p->free_item)) {
        p->nfree_item = 0;
        p->free_item = NULL;
    }

    /* delete slab items either from hash or free Q */
    for (i = 0; i < p->nitem; i++) {
        it = slab_2_item(slab, i, p->size);

        ASSERT(it->magic == ITEM_MAGIC);
        ASSERT(it->refcount == 0);
        ASSERT(it->offset != 0);

	/* If it is in hash, reuse it. Otherwise, take it off the free queue */
#if defined CC_CHAINED && CC_CHAINED == 1
        if(item_is_linked(it->head)) {
#else
        if(item_is_linked(it)) {
#endif
            item_reuse(it);
        } else if (item_is_slabbed(it)) {
            ASSERT(slab == item_2_slab(it));
            ASSERT(!TAILQ_EMPTY(&p->free_itemq));

            it->flags &= ~ITEM_SLABBED;

            ASSERT(p->nfree_itemq > 0);
            p->nfree_itemq--;
            TAILQ_REMOVE(&p->free_itemq, it, i_tqe);
        }
    }

    /* unlink the slab from its class */
    slab_lruq_remove(slab);

    log_stderr("@@@ slab_evict_one finishing");
}

/*
 * Get a random slab from all active slabs and evict it for new allocation.
 *
 * Note that the slab_table enables us to have O(1) lookup for every slab in
 * the system. The inserts into the table are just appends - O(1) and there
 * are no deletes from the slab_table. These two constraints allows us to keep
 * our random choice uniform.
 */
static struct slab *
slab_evict_rand(void)
{
    struct slab *slab;
    uint32_t tries;

    tries = SLAB_RAND_MAX_TRIES;
    do {
        slab = slab_table_rand();
        tries--;
    } while (tries > 0 && slab->refcount != 0);

    if (tries == 0) {
        /* all randomly chosen slabs are in use */
        return NULL;
    }

    log_stderr("random-evicting slab with id %hhu", slab->id);

    slab_evict_one(slab);

    return slab;
}

/*
 * Evict by looking into least recently used queue of all slabs.
 */
static struct slab *
slab_evict_lru(int id)
{
    struct slab *slab;
    uint32_t tries;

    for (tries = SLAB_LRU_MAX_TRIES, slab = slab_lruq_head();
         tries > 0 && slab != NULL;
         tries--, slab = TAILQ_NEXT(slab, s_tqe)) {
        if (slab->refcount == 0) {
            break;
        }
    }

    /* All LRU queue slabs were in use */
    if (tries == 0 || slab == NULL) {
        return NULL;
    }

    log_stderr("lru-evicting slab with id %hhu", slab->id);

    slab_evict_one(slab);

    return slab;
}

/*
 * All the prep work before starting to use a slab.
 */
static void
slab_add_one(struct slab *slab, uint8_t id)
{
    struct slabclass *p;
    struct item *it;
    uint32_t i, offset;

    p = &slabclass[id];

    /* initialize slab header */
    slab_hdr_init(slab, id);

    slab_lruq_append(slab);

    /* initialize all slab items */
    for (i = 0; i < p->nitem; i++) {
        it = slab_2_item(slab, i, p->size);
        offset = (uint32_t)((uint8_t *)it - (uint8_t *)slab);
        item_hdr_init(it, offset, id);
    }

    /* make this slab as the current slab */
    p->nfree_item = p->nitem;
    p->free_item = (struct item *)&slab->data[0];
}

/*
 * Get a slab. id is the slabclass the new slab will be linked into.
 *
 * We return a slab either from the:
 * 1. slab pool, if not empty. or,
 * 2. evict an active slab and return that instead.
 */
static rstatus_t
slab_get(uint8_t id)
{
    struct slab *slab;

    ASSERT(slabclass[id].free_item == NULL);
    ASSERT(TAILQ_EMPTY(&slabclass[id].free_itemq));

    slab = slab_get_new();

    /* If failed to get a slab from the slab pool, evict */
    if (slab == NULL && settings.evict_lru) {
        slab = slab_evict_lru(id);
    } else if (slab == NULL) {
        slab = slab_evict_rand();
    }

    /* Got slab, prepare it for use */
    if (slab != NULL) {
        slab_add_one(slab, id);
        return CC_OK;
    }

    return CC_ENOMEM;
}

/*
 * Get an item from the item free queue of the given slab with id.
 */
static struct item *
slab_get_item_from_freeq(uint8_t id)
{
    struct slabclass *p; /* parent slabclass */
    struct item *it;

    if (!settings.use_freeq) {
        return NULL;
    }

    p = &slabclass[id];

    /* If no items are in the free queue, return NULL */
    if (p->nfree_itemq == 0) {
        return NULL;
    }

    it = TAILQ_FIRST(&p->free_itemq);

    ASSERT(it->magic == ITEM_MAGIC);
    ASSERT(item_is_slabbed(it));
    ASSERT(!item_is_linked(it));

    /* Remove obtained item from the free queue */
    it->flags &= ~ITEM_SLABBED;
    ASSERT(p->nfree_itemq > 0);
    p->nfree_itemq--;
    TAILQ_REMOVE(&p->free_itemq, it, i_tqe);

    log_stderr("get free q item with key %s at offset %u with id %hhu",
	    item_key(it), it->offset, it->id);

    return it;
}

/*
 * Get an item from the slab with a given id. We get an item either from:
 *   1. item free Q of given slab with id. or,
 *   2. current slab.
 * If the current slab is empty, we get a new slab from the slab allocator
 * and return the next item from this new slab.
 */
static struct item *
_slab_get_item(uint8_t id)
{
    struct slabclass *p;
    struct item *it;

    p = &slabclass[id];

    /* Try to get item from free queue */
    it = slab_get_item_from_freeq(id);
    if (it != NULL) {
        return it;
    }

    /* If no items left in slab and attempt to get new slab fails return NULL */
    if (p->free_item == NULL && (slab_get(id) != CC_OK)) {
        return NULL;
    }

    /* return item from current slab */
    it = p->free_item;
    if (--p->nfree_item != 0) {
        p->free_item = (struct item *)(((uint8_t *)p->free_item) + p->size);
    } else {
        p->free_item = NULL;
    }

#if defined HAVE_ASSERT_PANIC && HAVE_ASSERT_PANIC == 1 || defined HAVE_ASSERT_LOG && HAVE_ASSERT_LOG == 1
    it->magic = ITEM_MAGIC;
#endif

    return it;
}

/*
 * Put an item back into the slab by inserting into the item free Q.
 */
static void
slab_put_item_into_freeq(struct item *it)
{
    uint8_t id = it->id;
    struct slabclass *p = &slabclass[id];

    ASSERT(id >= SLABCLASS_MIN_ID && id <= slabclass_max_id);
    ASSERT(item_2_slab(it)->id == id);
    ASSERT(!item_is_linked(it));
    ASSERT(!item_is_slabbed(it));
#if defined CC_CHAINED && CC_CHAINED == 1
    ASSERT(!item_is_chained(it));
    ASSERT(it->next_node == NULL);
#endif
    ASSERT(it->refcount == 0);
    ASSERT(it->offset != 0);

    log_stderr("put free queue item with key %s at offset %u with id %hhu",
	    item_key(it), it->offset, it->id);

    it->flags |= ITEM_SLABBED;

    p->nfree_itemq++;
    TAILQ_INSERT_HEAD(&p->free_itemq, it, i_tqe);
}

/*
 * Put an item back into the slab
 */
static void
_slab_put_item(struct item *it)
{
    slab_put_item_into_freeq(it);
}
