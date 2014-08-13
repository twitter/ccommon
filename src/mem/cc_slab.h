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

#ifndef _CC_SLABS_H_
#define _CC_SLABS_H_

#include <cc_define.h>
#include <cc_queue.h>
#include <cc_time.h>
#include <mem/cc_item.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * Every slab (struct slab) in the twemcache starts with a slab header
 * followed by slab data. The slab data is essentially a collection of
 * contiguous, equal sized items (struct item)
 *
 * An item is owned by a slab and a slab is owned by a slabclass
 *
 *   <------------------------ slab_size ------------------------->
 *   +---------------+--------------------------------------------+
 *   |  slab header  |              slab data                     |
 *   | (struct slab) |      (contiguous equal sized items)        |
 *   +---------------+--------------------------------------------+
 *   ^               ^
 *   |               |
 *   \               |
 *   slab            \
 *                   slab->data
 *
 * Note: keep struct slab 8-byte aligned so that item chunks always start on
 *       8-byte aligned boundary.
 */
struct slab {
#if defined HAVE_ASSERT_PANIC && HAVE_ASSERT_PANIC == 1 || defined HAVE_ASSERT_LOG && HAVE_ASSERT_LOG == 1
    uint32_t          magic;    /* slab magic (const) */
#endif
    uint8_t           id;       /* slabclass id */
    uint8_t           unused;   /* unused */
    uint16_t          refcount; /* # concurrent users */
    TAILQ_ENTRY(slab) s_tqe;    /* link in slab lruq */
    rel_time_t        utime;    /* last update time in secs */
    uint32_t          padding;  /* unused */
    uint8_t           data[1];  /* opaque data */
};

TAILQ_HEAD(slab_tqh, slab);

#define SLAB_MAGIC      0xdeadbeef
#define SLAB_HDR_SIZE   offsetof(struct slab, data)
#define SLAB_MIN_SIZE   ((size_t) 512)
#define SLAB_MAX_SIZE   ((size_t) (128 * MB))
#define SLAB_SIZE       MB

/*
 * Every class (struct slabclass) is a collection of slabs that can serve
 * items of a given maximum size. Every slab in twemcache is identified by a
 * unique unsigned 8-bit id, which also identifies its owner slabclass
 *
 * Slabs that belong to a given class are reachable through slabq. Slabs
 * across all classes are reachable through the slabtable and slab lruq.
 *
 * We use free_item as a marker for the next available, unallocated item
 * in the current slab. Items that are available for reuse (i.e. allocated
 * and then freed) are kept track by free_itemq
 *
 * slabclass[]:
 *
 *  +-------------+
 *  |             |
 *  |             |
 *  |             |
 *  |   class 0   |
 *  |             |
 *  |             |
 *  |             |
 *  +-------------+
 *  |             |  ----------------------------------------------------------+
 *  |             | /                                              (last slab) |
 *  |             |/    +---------------+-------------------+    +-------------v-+-------------------+
 *  |             |     |               |                   |    |               |                   |
 *  |   class 1   |     |  slab header  |     slab data     |    |  slab header  |     slab data     |
 *  |             |     |               |                   |    |               |                   |--+
 *  |             |\    +---------------+-------------------+    +---------------+-------------------+  |
 *  |             | \                                                                                   //
 *  |             |  ----> (freeq)
 *  +-------------+
 *  |             |  -----------------+
 *  |             | /     (last slab) |
 *  |             |/    +-------------v-+-------------------+
 *  |             |     |               |                   |
 *  |   class 2   |     |  slab header  |     slab data     |
 *  |             |     |               |                   |--+
 *  |             |\    +---------------+-------------------+  |
 *  |             | \                                          //
 *  |             |  ----> (freeq)
 *  +-------------+
 *  |             |
 *  |             |
 *  .             .
 *  .    ....     .
 *  .             .
 *  |             |
 *  |             |
 *  +-------------+
 *            |
 *            |
 *            //
 */
struct slabclass {
    uint32_t         nitem;       /* # item per slab (const) */
    size_t           size;        /* item size (const) */

    uint32_t         nfree_itemq; /* # free item q */
    struct item_stqh free_itemq;  /* free item q */

    uint32_t         nfree_item;  /* # free item (in current slab) */
    struct item      *free_item;  /* next free item (in current slab) */
};

/*
 * Slabclass id is an unsigned byte. So, maximum number of slab classes
 * cannot exceeded 256
 *
 * We use id = 255 as an id reserved for chained items and id = 0 for
 * aggregation. This means that we can have at most 254 usable slab classes.
 */
#define SLABCLASS_MIN_ID        1
#define SLABCLASS_MAX_ID        (UCHAR_MAX - 1)
#define SLABCLASS_CHAIN_ID      UCHAR_MAX
#define SLABCLASS_MAX_IDS       UCHAR_MAX

extern uint8_t slabclass_max_id;

/* Return the usable space for item sized chunks that would be carved out of a
   given slab */
size_t slab_size(void);

/* Print information about all slabs */
void slab_print(void);

/* Increment the refcount on the given slab */
void slab_acquire_refcount(struct slab *slab);

/* Decrement the refcount on the given slab */
void slab_release_refcount(struct slab *slab);

/* Return the item size given a slab id */
size_t slab_item_size(uint8_t id);

/* Returns the id of the slab which can store an item of a given size. */
uint8_t slab_id(size_t size);

/* Initialize/deinitialize slab related facilities */
rstatus_t slab_init(void);
void slab_deinit(void);

/* Get an item from a slab with the provided ID */
struct item *slab_get_item(uint8_t id);

/* Put an item back into the slab (frees the item, allowing it to be used
   again) */
void slab_put_item(struct item *it);

/* Touch the slab LRU queue by moving the given slab to the tail of the slab LRU
   queue, but only if it hasn't been moved within the last
   SLAB_LRU_UPDATE_INTERVAL seconds */
void slab_lruq_touch(struct slab *slab, bool allocated);

/* Get the maximum amount of data an item can contain with the given specs */
uint32_t slab_item_max_nbyte(uint8_t id, uint8_t nkey);

/* Disable eviction */
void slab_disable_eviction(void);

/* Enable eviction */
void slab_enable_eviction(void);

#endif
