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
 * Each mbuf has two parts: a part that links into a one-directional list; and
 * a data block for storing data, which has its own read/write positions. It is
 * a common practice to chain a number of mbufs together to store a message.
 *
 * mbuf module allocates a number of mbufs upon initialization or no free mbuf
 * is available for the next request. Instead of freeing up the mbuf when done,
 * it can be returned to the pool (mbuf_put). The bulk allocation amortizes the
 * cost of memory allocation, making it a good candidate for contexts where
 * small memory allocations are relatively expensive, or when the amount of
 * message buffer needed is relatively constant.
 */

#ifndef _CC_MBUF_H_
#define _CC_MBUF_H_

#include <stdint.h>
#include <stddef.h>

struct mbuf_data {
    uint32_t            magic;   /* mbuf magic (const) */
    uint8_t             *rpos;   /* read marker */
    uint8_t             *wpos;   /* write marker */
    uint8_t             *start;  /* start of mbuf body (const) */
    uint8_t             *end;    /* end of mbuf body (const) */
};

/* for STAILQ chaining */
struct mbuf {
    struct mbuf_data    data;
    STAILQ_ENTRY(mbuf)  next;    /* next mbuf */
};
STAILQ_HEAD(mq, mbuf);

typedef void (*mbuf_copy_t)(struct mbuf *, void *);


#define MBUF_MAGIC      0xdeadbeef
#define MBUF_MIN_SIZE   512
#define MBUF_MAX_SIZE   65536
#define MBUF_SIZE       16384
#define MBUF_DATA_SIZE  sizeof(struct mbuf_data)
#define MBUF_HDR_SIZE   sizeof(struct mbuf)

static inline uint32_t
mbuf_get_magic(struct mbuf *mbuf)
{
    return mbuf->data.magic;
}

static inline uint8_t *
mbuf_get_rpos(struct mbuf *mbuf)
{
    return mbuf->data.rpos;
}

static inline uint8_t *
mbuf_get_wpos(struct mbuf *mbuf)
{
    return mbuf->data.wpos;
}

static inline uint8_t *
mbuf_get_start(struct mbuf *mbuf)
{
    return mbuf->data.start;
}

static inline uint8_t *
mbuf_get_end(struct mbuf *mbuf)
{
    return mbuf->data.end;
}

static inline bool
mbuf_empty(struct mbuf *mbuf)
{
    return mbuf_get_rpos(mbuf) == mbuf_get_wpos(mbuf) ? true : false;
}

static inline bool
mbuf_full(struct mbuf *mbuf)
{
    return mbuf_get_wpos(mbuf) == mbuf_get_end(mbuf) ? true : false;
}

static inline void
mbuf_set_rpos(struct mbuf *mbuf, uint8_t *rpos)
{
    mbuf->data.rpos = rpos;
}

static inline void
mbuf_set_wpos(struct mbuf *mbuf, uint8_t *wpos)
{
    mbuf->data.wpos = wpos;
}

static inline void
mbuf_set_start(struct mbuf *mbuf, uint8_t *start)
{
    mbuf->data.start = start;
}

static inline void
mbuf_set_end(struct mbuf *mbuf, uint8_t *end)
{
    mbuf->data.end = end;
}

static inline void
mbuf_incr_rpos(struct mbuf *mbuf, size_t delta)
{
    mbuf->data.rpos += delta;
}

static inline void
mbuf_incr_wpos(struct mbuf *mbuf, size_t delta)
{
    mbuf->data.wpos += delta;
}

void mbuf_init(size_t chunk_size);
void mbuf_deinit(void);
struct mbuf *mbuf_get(void);
void mbuf_put(struct mbuf *mbuf);
void mbuf_reset(struct mbuf *mbuf);
size_t mbuf_rsize(struct mbuf *mbuf);
size_t mbuf_wsize(struct mbuf *mbuf);
size_t mbuf_capacity(void);
void mbuf_insert(struct mq *mq, struct mbuf *mbuf);
void mbuf_remove(struct mq *mq, struct mbuf *mbuf);
void mbuf_copy(struct mbuf *mbuf, uint8_t *rpos, size_t n);
struct mbuf *mbuf_split(struct mbuf *mbuf, uint8_t *addr, mbuf_copy_t cb, void *cbarg);

#endif
