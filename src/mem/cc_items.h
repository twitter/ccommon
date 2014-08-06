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

#ifndef _CC_ITEMS_H_
#define _CC_ITEMS_H_

#include <cc_debug.h>
#include <cc_define.h>
#include <cc_time.h>
#include <cc_queue.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern struct hash_table mem_hash_table;

typedef enum item_flags {
    ITEM_LINKED  = 1,  /* item in hash */
    ITEM_CAS     = 2,  /* item has cas */
    ITEM_SLABBED = 4,  /* item in free q */
    ITEM_RALIGN  = 8,  /* item data (payload) is right-aligned */
#if defined CC_CHAINED && CC_CHAINED == 1
    ITEM_CHAINED = 16, /* item is chained */
#endif
} item_flags_t;

/*
 * Item operation return types
 */
typedef enum item_set_result {
    SET_OK
} item_set_result_t;

typedef enum item_cas_result {
    CAS_OK,
    CAS_EXISTS,
    CAS_NOT_FOUND
} item_cas_result_t;

typedef enum item_add_result {
    ADD_OK,
    ADD_EXISTS
} item_add_result_t;

typedef enum item_replace_result {
    REPLACE_OK,
    REPLACE_NOT_FOUND
} item_replace_result_t;

typedef enum item_annex_result {
    ANNEX_OK,
    ANNEX_NOT_FOUND,
    ANNEX_OVERSIZED,
    ANNEX_EOM
} item_annex_result_t;

typedef enum item_delete_result {
    DELETE_OK,
    DELETE_NOT_FOUND
} item_delete_result_t;

typedef enum item_delta_result {
    DELTA_OK,
    DELTA_NOT_FOUND,
    DELTA_NON_NUMERIC,
    DELTA_EOM,
    DELTA_CHAINED,
} item_delta_result_t;

/*
 * Every item chunk in the cache starts with an header (struct item)
 * followed by item data. An item is essentially a chunk of memory
 * carved out of a slab. Every item is owned by its parent slab
 *
 * Items are either linked or unlinked. When item is first allocated and
 * has no data, it is unlinked. When data is copied into an item, it is
 * linked into hash and lru q (ITEM_LINKED). When item is deleted either
 * explicitly or due to flush or expiry, it is moved in the free q
 * (ITEM_SLABBED). The flags ITEM_LINKED and ITEM_SLABBED are mutually
 * exclusive and when an item is unlinked it has neither of these flags
 *
 *   <-----------------------item size------------------>
 *   +---------------+----------------------------------+
 *   |               |                                  |
 *   |  item header  |          item payload            |
 *   | (struct item) |         ...      ...             |
 *   +---------------+-------+-------+------------------+
 *   ^               ^       ^       ^
 *   |               |       |       |
 *   |               |       |       |
 *   |               |       |       |
 *   |               |       |       \
 *   |               |       |       item_data()
 *   |               |       \
 *   \               |       item_key()
 *   item            \
 *                   item->end, (if enabled) item_get_cas()
 *
 * item->end is followed by:
 * - 8-byte cas, if ITEM_CAS flag is set
 * - key with terminating '\0', length = item->nkey + 1
 * - data with no terminating '\0'
 */

/* TODO: move next_node and head for more compact memory alignment */
struct item {
#if defined HAVE_ASSERT_PANIC && HAVE_ASSERT_PANIC == 1 || defined HAVE_ASSERT_LOG && HAVE_ASSERT_LOG == 1
    uint32_t          magic;      /* item magic (const) */
#endif
    TAILQ_ENTRY(item) i_tqe;      /* link in the free q */
    SLIST_ENTRY(item) h_sle;      /* link in hash */
    rel_time_t        atime;      /* last access time in secs */
    rel_time_t        exptime;    /* expiry time in secs */
    uint32_t          nbyte;      /* data size */
    uint32_t          offset;     /* offset of item in slab */
    uint32_t          dataflags;  /* data flags opaque to the server */
    uint16_t          refcount;   /* # concurrent users of item */
    uint8_t           flags;      /* item flags */
    uint8_t           id;         /* slab class id */
    uint8_t           nkey;       /* key length */
#if defined CC_CHAINED && CC_CHAINED == 1
    struct item       *next_node; /* next node, if item is chained */
    struct item       *head;      /* head node */
#endif
    char              end[1];     /* item data */
};

SLIST_HEAD(item_slh, item);
TAILQ_HEAD(item_tqh, item);

#define ITEM_MAGIC      0xfeedface
#define ITEM_HDR_SIZE   offsetof(struct item, end)
#define CRLF            "\x0d\x0a"
#define CRLF_LEN        (uint32_t)(sizeof(CRLF) - 1)

/*
 * An item chunk is the portion of the memory carved out from the slab
 * for an item. An item chunk contains the item header followed by data
 * item.
 *
 * The smallest item data is actually a single byte key with a zero byte value
 * which internally is of sizeof("k"), as key is stored with terminating '\0'.
 * If cas is enabled, then item payload should have another 8-byte for cas.
 *
 * The largest item data is actually the room left in the slab_size()
 * slab, after the item header has been factored out
 */
#define ITEM_MIN_PAYLOAD_SIZE  (sizeof("k") + sizeof(uint64_t))
#define ITEM_MIN_CHUNK_SIZE \
    MC_ALIGN(ITEM_HDR_SIZE + ITEM_MIN_PAYLOAD_SIZE, MC_ALIGNMENT)

#define ITEM_PAYLOAD_SIZE      32
#define ITEM_CHUNK_SIZE     \
    MC_ALIGN(ITEM_HDR_SIZE + ITEM_PAYLOAD_SIZE, MC_ALIGNMENT)


#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 2
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

/*
 * Item flag getters
 */
static inline bool
item_has_cas(struct item *it) {
    return (it->flags & ITEM_CAS);
}

static inline bool
item_is_linked(struct item *it) {
    return (it->flags & ITEM_LINKED);
}

static inline bool
item_is_slabbed(struct item *it) {
    return (it->flags & ITEM_SLABBED);
}

static inline bool
item_is_raligned(struct item *it) {
    return (it->flags & ITEM_RALIGN);
}

#if defined CC_CHAINED && CC_CHAINED == 1
static inline bool
item_is_chained(struct item *it) {
    return (it->flags & ITEM_CHAINED);
}
#endif

/*
 * Item CAS operations
 */
static inline uint64_t
item_get_cas(struct item *it)
{
    ASSERT(it->magic == ITEM_MAGIC);

    if (item_has_cas(it)) {
        return *((uint64_t *)it->end);
    }

    return 0;
}

static inline void
item_set_cas(struct item *it, uint64_t cas)
{
    ASSERT(it->magic == ITEM_MAGIC);

    if (item_has_cas(it)) {
        *((uint64_t *)it->end) = cas;
    }
}

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
#pragma GCC diagnostic pop
#endif

/*
 * Get the location of the item's key
 */
static inline char *
item_key(struct item *it)
{
    char *key;

    ASSERT(it->magic == ITEM_MAGIC);

    key = it->end;
    if (item_has_cas(it)) {
        key += sizeof(uint64_t);
    }

    return key;
}

/*
 * Get the total size of an item given key size, data size, and cas
 */
static inline size_t
item_ntotal(uint8_t nkey, uint32_t nbyte, bool use_cas)
{
    size_t ntotal;

    ntotal = use_cas ? sizeof(uint64_t) : 0;
    ntotal += ITEM_HDR_SIZE + nkey + 1 + nbyte + CRLF_LEN;

    return ntotal;
}

/*
 * Get the size of the item (header + key + data)
 */
static inline size_t
item_size(struct item *it)
{
    ASSERT(it->magic == ITEM_MAGIC);

    return item_ntotal(it->nkey, it->nbyte, item_has_cas(it));
}

/*
 * Initialize/deinitialize item related facilities
 */
rstatus_t item_init(uint32_t hash_power);
void item_deinit(void);

/* Get location of item payload */
char * item_data(struct item *it);

/* Get the slab that this item belongs to */
struct slab *item_2_slab(struct item *it);

/* Initialize the header of an item */
void item_hdr_init(struct item *it, uint32_t offset, uint8_t id);

/* Get the slab id required for an item with key size nkey and data size nbyte */
uint8_t item_slabid(uint8_t nkey, uint32_t nbyte);

/* Allocate an item with the given parameters */
struct item *item_alloc(char *key, uint8_t nkey, uint32_t dataflags,
			rel_time_t exptime, uint32_t nbyte);

/* Make an item with zero refcount available for reuse. Frees all nodes in
   different slabs, and opens up nodes in provided node's slabs for reuse. */
void item_reuse(struct item *it);

/* Decrement the refcount on an item; frees item if it is unlinked and its
   refcount drops to zero */
void item_remove(struct item *it);

/* Get linked item with given key */
struct item *item_get(const char *key, size_t nkey);

/* Links an item into the hash table; if one already exists with the same key,
   the new item replaces the old one. */
void item_set(struct item *it);

/* Check-and-set; if the item has not been updated since the client last fetched
   it, then it is replaced. */
item_cas_result_t item_cas(struct item *it);

/* Store this item, but only if the server does not already hold data for this
   key. */
item_add_result_t item_add(struct item *it);

/* Store this item, but only if the server already holds data for this key */
item_replace_result_t item_replace(struct item *it);

/* Append given item's data onto end of linked item's data */
item_annex_result_t item_append(struct item *it);

/* Prepend given item's data at beginning of linked item's data */
item_annex_result_t item_prepend(struct item *it);

/* Perform increment/decrement operation on item with given key */
item_delta_result_t item_delta(char *key, size_t nkey, bool incr, uint64_t delta);

/* Unlink an item. Removes it if its refcount drops to zero. */
item_delete_result_t item_delete(char *key, size_t nkey);

/* Get total data size of item (sum of nbyte for all nodes) */
uint64_t item_total_nbyte(struct item *it);

/* Get the maximum amount of data the given item can contain */
uint32_t item_max_nbyte(uint8_t id, uint8_t nkey);

#if defined CC_CHAINED && CC_CHAINED == 1
/* Get the number of nodes in the item */
uint32_t item_num_nodes(struct item *it);

/* Append given item's data onto end of linked item's data. If a new node needs
   to be allocated for the annex, the entirety of the addition is added to the
   new node so that the addition is stored contiguously. Useful for structures
   like zipmap, where either all or none of an entry must be in one given node */
item_annex_result_t item_append_contig(struct item *it);

/* Get the last node in an item chain. */
struct item *item_tail(struct item *it);

/* Remove the given node from the item (allows freed node to be reused).
   Does nothing if node is not in the chain belonging to it. */
void item_remove_node(struct item *it, struct item *node);
#endif

#endif
