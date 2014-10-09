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
 * mbuf: a pool of fixed size buffers that can be chained together.
 *
 * Each mbuf can to be linked into a one-directional list/msg; and two pairs
 * of pointers: one pair for tracking the start and end of the "body" which
 * can hold data, another pair that tracks the current reand and write position.
 *
 * mbuf module allocates a number of mbufs upon initialization, or when no free
 * mbuf is available for the current request. Instead of freeing up the memory
 * when done, it is returned to the pool (mbuf_put). The bulk allocation
 * amortizes the cost of memory allocation, making it a good candidate when
 * small memory allocations are relatively expensive, or when the amount of
 * message buffer needed is relatively constant.
 */


#ifndef _CC_MBUF_H_
#define _CC_MBUF_H_

#include <cc_bstring.h>
#include <cc_option.h>
#include <cc_queue.h>
#include <cc_util.h>

#include <stddef.h>
#include <stdint.h>

#define MBUF_POOLSIZE 0 /* unlimited */

/*          name            type                default             description */
#define MBUF_OPTION(ACTION)                                                             \
    ACTION( mbuf_size,      CONFIG_TYPE_UINT,   str(MBUF_SIZE),     "mbuf size"        )\
    ACTION( mbuf_poolsize,  CONFIG_TYPE_UINT,   str(MBUF_POOLSIZE), "mbuf pool size"   )


/* TODO(yao): create a non-pooled/chained version of mbuf */
struct mbuf {
    uint32_t           magic;   /* mbuf magic (const) */
    STAILQ_ENTRY(mbuf) next;    /* next mbuf */
    uint8_t            *rpos;   /* read marker */
    uint8_t            *wpos;   /* write marker */
    uint8_t            *start;  /* start of buffer (const) */
    uint8_t            *end;    /* end of buffer (const) */
};

typedef void (*mbuf_copy_t)(struct mbuf *, void *);

STAILQ_HEAD(mq, mbuf);

#define MBUF_MAGIC      0xbeadface
#define MBUF_MIN_SIZE   512
#define MBUF_MAX_SIZE   65536
#define MBUF_SIZE       16384
#define MBUF_HDR_SIZE   sizeof(struct mbuf)

static inline bool
mbuf_empty(struct mbuf *mbuf)
{
    return mbuf->rpos == mbuf->wpos ? true : false;
}

static inline bool
mbuf_full(struct mbuf *mbuf)
{
    return mbuf->wpos == mbuf->end ? true : false;
}

void mbuf_setup(uint32_t chunk_size);
void mbuf_teardown(void);
void mbuf_reset(struct mbuf *mbuf);
struct mbuf *mbuf_create(void);
void mbuf_destroy(struct mbuf *mbuf);
uint32_t mbuf_rsize(struct mbuf *mbuf);
uint32_t mbuf_wsize(struct mbuf *mbuf);
uint32_t mbuf_capacity(void);
void mbuf_lshift(struct mbuf *mbuf);
void mbuf_rshift(struct mbuf *mbuf);
void mbuf_copy(struct mbuf *mbuf, uint8_t *addr, uint32_t n);
void mbuf_copy_bstring(struct mbuf *mbuf, const struct bstring str);

void mbuf_pool_create(uint32_t max);
void mbuf_pool_destroy(void);
struct mbuf *mbuf_borrow(void);
void mbuf_return(struct mbuf *mbuf);

void mbuf_insert(struct mq *mq, struct mbuf *mbuf);
void mbuf_remove(struct mq *mq, struct mbuf *mbuf);
#endif /* _CC_MBUF_H_ */
