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

#include <stdint.h>
#include <stdlib.h>

#include <cc_debug.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <cc_queue.h>
#include <cc_string.h>

#include <cc_mbuf.h>

static uint32_t nfree_mq;   /* # free mbuf */
static struct mq free_mq; /* free mbuf q */

static size_t mbuf_chunk_size; /* mbuf chunk size - header + data (const) */
static size_t mbuf_offset;     /* mbuf offset in chunk (const) */

static struct mbuf *
_mbuf_get(void)
{
    struct mbuf *mbuf;
    uint8_t *buf;

    if (!STAILQ_EMPTY(&free_mq)) {
        ASSERT(nfree_mq > 0);

        mbuf = STAILQ_FIRST(&free_mq);
        nfree_mq--;
        STAILQ_REMOVE_HEAD(&free_mq, next);

        ASSERT(mbuf->magic == MBUF_MAGIC);
        goto done;
    }

    buf = cc_alloc(mbuf_chunk_size);
    if (buf == NULL) {
        return NULL;
    }

    /*
     * mbuf header is at the tail end of the mbuf. This enables us to catch
     * buffer overrun early by asserting on the magic value during get or
     * put operations
     *
     *   <------------- mbuf_chunk_size ------------->
     *   +-------------------------------------------+
     *   |       mbuf data          |  mbuf header   |
     *   |     (mbuf_offset)        | (struct mbuf)  |
     *   +-------------------------------------------+
     *   ^           ^        ^     ^^
     *   |           |        |     ||
     *   \           |        |     |\
     *   mbuf->start \        |     | mbuf->end (one byte past valid bound)
     *                mbuf->rpos    \
     *                        \      mbuf
     *                        mbuf->wpos (one byte past valid byte)
     *
     */
    mbuf = (struct mbuf *)(buf + mbuf_offset);
    mbuf->magic = MBUF_MAGIC;

done:
    STAILQ_NEXT(mbuf, next) = NULL;
    return mbuf;
}

struct mbuf *
mbuf_get(void)
{
    struct mbuf *mbuf;
    uint8_t *buf;

    mbuf = _mbuf_get();
    if (mbuf == NULL) {
        return NULL;
    }

    mbuf->end = (uint8_t *)mbuf;
    mbuf->start = mbuf->end - mbuf_offset;
    mbuf->rpos = mbuf->start;
    mbuf->wpos = mbuf->start;

    log_debug(LOG_VVERB, "get mbuf %p", mbuf);

    return mbuf;
}

static void
mbuf_free(struct mbuf *mbuf)
{
    uint8_t *buf;

    log_debug(LOG_VVERB, "put mbuf %p len %d", mbuf, mbuf->wpos - mbuf->rpos);

    ASSERT(STAILQ_NEXT(mbuf, next) == NULL);
    ASSERT(mbuf->magic == MBUF_MAGIC);

    buf = (uint8_t *)mbuf - mbuf_offset;
    cc_free(buf);
}

void
mbuf_put(struct mbuf *mbuf)
{
    log_debug(LOG_VVERB, "put mbuf %p len %d", mbuf, mbuf->wpos - mbuf->rpos);

    ASSERT(STAILQ_NEXT(mbuf, next) == NULL);
    ASSERT(mbuf->magic == MBUF_MAGIC);

    nfree_mq++;
    STAILQ_INSERT_HEAD(&free_mq, mbuf, next);
}

/*
 * Rewind the mbuf by discarding any of the read or unread data that it
 * might hold.
 */
void
mbuf_reset(struct mbuf *mbuf)
{
    mbuf->rpos = mbuf->start;
    mbuf->wpos = mbuf->start;
}

/*
 * Return the size of available data in mbuf. Mbuf cannot contain more than
 * 2^32 bytes (4G).
 */
uint32_t
mbuf_rsize(struct mbuf *mbuf)
{
    ASSERT(mbuf->wpos >= mbuf->rpos);

    return (uint32_t)(mbuf->wpos - mbuf->rpos);
}

/*
 * Return the remaining space size for any new data in mbuf. Mbuf cannot
 * contain more than 2^32 bytes (4G).
 */
uint32_t
mbuf_wsize(struct mbuf *mbuf)
{
    ASSERT(mbuf->end >= mbuf->wpos);

    return (uint32_t)(mbuf->end - mbuf->wpos);
}

/*
 * Return the maximum available space size for data in any mbuf. Mbuf cannot
 * contain more than 2^32 bytes (4G).
 */
size_t
mbuf_capacity(void)
{
    return mbuf_offset;
}

/*
 * Insert mbuf at the tail of the mq
 */
void
mbuf_insert(struct mq *mq, struct mbuf *mbuf)
{
    STAILQ_INSERT_TAIL(mq, mbuf, next);
    log_debug(LOG_VVERB, "insert mbuf %p len %d", mbuf, mbuf->wpos - mbuf->rpos);
}

/*
 * Remove mbuf from the mq
 */
void
mbuf_remove(struct mq *mq, struct mbuf *mbuf)
{
    log_debug(LOG_VVERB, "remove mbuf %p len %d", mbuf, mbuf->wpos - mbuf->rpos);

    STAILQ_REMOVE(mq, mbuf, mbuf, next);
    STAILQ_NEXT(mbuf, next) = NULL;
}

/*
 * Copy n bytes from memory area rpos to mbuf.
 *
 * The memory areas should not overlap and the mbuf should have
 * enough space for n bytes.
 */
void
mbuf_copy(struct mbuf *mbuf, uint8_t *rpos, size_t n)
{
    if (n == 0) {
        return;
    }

    /* mbuf has space for n bytes */
    ASSERT(!mbuf_full(mbuf) && n <= mbuf_size(mbuf));

    /* no overlapping copy */
    ASSERT(rpos < mbuf->start || rpos >= mbuf->end);

    cc_memcpy(mbuf->wpos, rpos, n);
    mbuf->wpos += n;
}

/*
 * Split mbuf h into h and t by copying data from h to t. Before
 * the copy, we invoke a precopy handler cb that will copy a predefined
 * string to the head of t.
 *
 * Return new mbuf t, if the split was successful.
 */
struct mbuf *
mbuf_split(struct mq *mq, uint8_t *rpos, mbuf_copy_t cb, void *cbarg)
{
    struct mbuf *mbuf, *nbuf;
    size_t size;

    ASSERT(!STAILQ_EMPTY(mq));

    mbuf = STAILQ_LAST(mq, mbuf, next);
    ASSERT(rpos >= mbuf->rpos && rpos <= mbuf->wpos);

    nbuf = mbuf_get();
    if (nbuf == NULL) {
        return NULL;
    }

    if (cb != NULL) {
        /* precopy nbuf */
        cb(nbuf, cbarg);
    }

    /* copy data from mbuf to nbuf */
    size = (size_t)(mbuf->wpos - rpos);
    mbuf_copy(nbuf, rpos, size);

    /* adjust mbuf */
    mbuf->wpos = rpos;

    log_debug(LOG_VVERB, "split into mbuf %p len %"PRIu32" and nbuf %p len "
              "%"PRIu32" copied %zu bytes", mbuf, mbuf_length(mbuf), nbuf,
              mbuf_length(nbuf), size);

    return nbuf;
}

void
mbuf_init(size_t chunk_size)
{
    nfree_mq = 0;
    STAILQ_INIT(&free_mq);

    mbuf_chunk_size = chunk_size;
    mbuf_offset = mbuf_chunk_size - MBUF_HDR_SIZE;
    ASSERT(mbuf_offset > 0 && mbuf_offset < mbuf_chunk_size);

    log_debug(LOG_DEBUG, "mbuf: chunk size %zu, hdr size %d, offset %zu",
              mbuf_chunk_size, MBUF_HDR_SIZE, mbuf_offset);
}

void
mbuf_deinit(void)
{
    while (!STAILQ_EMPTY(&free_mq)) {
        struct mbuf *mbuf = STAILQ_FIRST(&free_mq);
        mbuf_remove(&free_mq, mbuf);
        mbuf_free(mbuf);
        nfree_mq--;
    }
    ASSERT(nfree_mbufq == 0);
}
