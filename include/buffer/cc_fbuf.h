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
 * fbuf: a pool of fixed size buffers that can be chained together.
 *
 * Each fbuf can to be linked into a one-directional list/msg; and two pairs
 * of pointers: one pair for tracking the start and end of the "body" which
 * can hold data, another pair that tracks the current reand and write position.
 *
 * fbuf module allocates a number of fbufs upon initialization, or when no free
 * fbuf is available for the current request. Instead of freeing up the memory
 * when done, it is returned to the pool (fbuf_put). The bulk allocation
 * amortizes the cost of memory allocation, making it a good candidate when
 * small memory allocations are relatively expensive, or when the amount of
 * message buffer needed is relatively constant.
 */


#ifndef _CC_FBUF_H_
#define _CC_FBUF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <cc_bstring.h>
#include <cc_option.h>
#include <cc_queue.h>
#include <cc_util.h>

#include <stddef.h>
#include <stdint.h>

#define FBUF_POOLSIZE 0 /* unlimited */

/*          name            type                default             description */
#define FBUF_OPTION(ACTION)                                                             \
    ACTION( fbuf_size,      OPTION_TYPE_UINT,   str(FBUF_SIZE),     "fbuf size"        )\
    ACTION( fbuf_poolsize,  OPTION_TYPE_UINT,   str(FBUF_POOLSIZE), "fbuf pool size"   )


/* TODO(yao): create a non-pooled/chained version of fbuf */
struct fbuf {
#if defined CC_ASSERT_PANIC || defined CC_ASSERT_LOG
    uint32_t           magic;   /* fbuf magic (const) */
#endif
    bool               free;    /* free? */
    STAILQ_ENTRY(fbuf) next;    /* next fbuf */
    uint8_t            *rpos;   /* read marker */
    uint8_t            *wpos;   /* write marker */
    uint8_t            *start;  /* start of buffer (const) */
    uint8_t            *end;    /* end of buffer (const) */
};

typedef void (*fbuf_copy_t)(struct fbuf *, void *);

STAILQ_HEAD(mq, fbuf);

#define FBUF_MAGIC      0xbeadface
#define FBUF_MIN_SIZE   512
#define FBUF_MAX_SIZE   65536
#define FBUF_SIZE       16384
#define FBUF_HDR_SIZE   sizeof(struct fbuf)

static inline bool
fbuf_empty(struct fbuf *fbuf)
{
    return fbuf->rpos == fbuf->wpos ? true : false;
}

static inline bool
fbuf_full(struct fbuf *fbuf)
{
    return fbuf->wpos == fbuf->end ? true : false;
}

/**
 * functions to setup and tear down the fbuf module
 */
void fbuf_setup(uint32_t chunk_size);
void fbuf_teardown(void);

/**
 * functions that deal with memory management
 */
struct fbuf *fbuf_create(void);
void fbuf_destroy(struct fbuf **fbuf);
void fbuf_pool_create(uint32_t max);
void fbuf_pool_destroy(void);
struct fbuf *fbuf_borrow(void);
void fbuf_return(struct fbuf **fbuf);

void fbuf_reset(struct fbuf *fbuf);
uint32_t fbuf_rsize(struct fbuf *fbuf);
uint32_t fbuf_wsize(struct fbuf *fbuf);
uint32_t fbuf_capacity(void);
void fbuf_lshift(struct fbuf *fbuf);
void fbuf_rshift(struct fbuf *fbuf);
void fbuf_copy(struct fbuf *fbuf, uint8_t *addr, uint32_t n);
void fbuf_copy_bstring(struct fbuf *fbuf, const struct bstring str);

void fbuf_insert(struct mq *mq, struct fbuf *fbuf);
void fbuf_remove(struct mq *mq, struct fbuf *fbuf);

#ifdef __cplusplus
}
#endif

#endif /* _CC_FBUF_H_ */
