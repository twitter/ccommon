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

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <cc_debug.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <cc_queue.h>
#include <cc_string.h>

#include <cc_mbuf.h>

static bool initialized = false;

static uint32_t nfree_mq;   /* # free mbuf */
static struct mq free_mq; /* free mbuf q */

static uint32_t mbuf_chunk_size; /* mbuf chunk size (all inclusive, const) */
static uint32_t mbuf_offset;     /* mbuf offset/data capacity (const) */

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
     *   |       mbuf body          |  mbuf header   |
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

/*
 * get a fully initialized mbuf
 */
struct mbuf *
mbuf_get(void)
{
    struct mbuf *mbuf;

    ASSERT(initialized == true);

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

/*
 * free an mbuf (assuming it has already been unlinked and not corrupted)
 */
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

/*
 * put an mbuf back in the free mbuf queue
 */
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
 * reset the mbuf by discarding the read or unread data that it might hold
 */
void
mbuf_reset(struct mbuf *mbuf)
{
    mbuf->rpos = mbuf->start;
    mbuf->wpos = mbuf->start;
}

/*
 * size of available/unread data in the mbuf- always less than 2^32(4G) bytes
 */
uint32_t
mbuf_rsize(struct mbuf *mbuf)
{
    ASSERT(mbuf->wpos >= mbuf->rpos);

    return (uint32_t)(mbuf->wpos - mbuf->rpos);
}

/*
 * size of remaining writable space in the mbuf- always less than 2^32(4G) bytes
 */
uint32_t
mbuf_wsize(struct mbuf *mbuf)
{
    ASSERT(mbuf->end >= mbuf->wpos);

    return (uint32_t)(mbuf->end - mbuf->wpos);
}

/*
 * total capacity of any mbuf, which are fixed sized and 2^32(4G) bytes at most
 */
uint32_t
mbuf_capacity(void)
{
    ASSERT(initialized == true);

    return mbuf_offset;
}

/*
 * insert the mbuf at the tail of the mbuf queue
 */
void
mbuf_insert(struct mq *mq, struct mbuf *mbuf)
{
    STAILQ_INSERT_TAIL(mq, mbuf, next);
    log_debug(LOG_VVERB, "insert mbuf %p len %d", mbuf, mbuf->wpos - mbuf->rpos);
}

/*
 * remove the mbuf from the mbuf queue
 */
void
mbuf_remove(struct mq *mq, struct mbuf *mbuf)
{
    ASSERT(initialized == true);

    log_debug(LOG_VVERB, "remove mbuf %p len %d", mbuf, mbuf->wpos - mbuf->rpos);

    STAILQ_REMOVE(mq, mbuf, mbuf, next);
    STAILQ_NEXT(mbuf, next) = NULL;
}

/*
 * copy n bytes from memory area addr to mbuf.
 *
 * The memory areas should not overlap and the mbuf should have
 * enough space for n bytes.
 */
void
mbuf_copy(struct mbuf *mbuf, uint8_t *addr, uint32_t n)
{
    if (n == 0) {
        return;
    }

    /* mbuf has space for n bytes */
    ASSERT(!mbuf_full(mbuf) && n <= mbuf_wsize(mbuf));

    /* no overlapping copy */
    ASSERT(addr < mbuf->start || addr >= mbuf->end);

    cc_memcpy(mbuf->wpos, addr, n);
    mbuf->wpos += n;
}

void
mbuf_copy_string(struct mbuf *mbuf, const struct string str)
{
    mbuf_copy(mbuf, str.data, str.len);
}

/*
 * split the mbuf by copying data from addr onward to a new mbuf
 * before the copy, we invoke a precopy handler cb that will copy a predefined
 * string to the head of the new mbuf
 */
struct mbuf *
mbuf_split(struct mbuf *mbuf, uint8_t *addr, mbuf_copy_t cb, void *cbarg)
{
    struct mbuf *nbuf;
    uint32_t size;

    nbuf = mbuf_get();
    if (nbuf == NULL) {
        return NULL;
    }

    if (cb != NULL) {
        /* precopy nbuf */
        cb(nbuf, cbarg);
    }

    /* copy data from mbuf to nbuf */
    size = mbuf->wpos - addr;
    mbuf_copy(nbuf, addr, size);

    /* adjust mbuf */
    mbuf->wpos = addr;

    log_debug(LOG_VVERB, "split into mbuf %p len %"PRIu32" and nbuf %p len "
              "%"PRIu32" copied %zu bytes", mbuf, mbuf_length(mbuf), nbuf,
              mbuf_length(nbuf), size);

    return nbuf;
}

/*
 * initialize the mbuf module by setting the module-local constants
 */
void
mbuf_init(uint32_t chunk_size)
{
    nfree_mq = 0;
    STAILQ_INIT(&free_mq);

    mbuf_chunk_size = chunk_size;
    mbuf_offset = mbuf_chunk_size - MBUF_HDR_SIZE;
    ASSERT(mbuf_offset > 0 && mbuf_offset < mbuf_chunk_size);

    initialized = true;

    log_debug(LOG_DEBUG, "mbuf: chunk size %zu, hdr size %d, offset %zu",
              mbuf_chunk_size, MBUF_HDR_SIZE, mbuf_offset);
}

/*
 * de-initialize the mbuf module by releasing all mbufs from the free_mq
 */
void
mbuf_deinit(void)
{
    ASSERT(initialized == true);

    while (!STAILQ_EMPTY(&free_mq)) {
        struct mbuf *mbuf = STAILQ_FIRST(&free_mq);
        mbuf_remove(&free_mq, mbuf);
        mbuf_free(mbuf);
        nfree_mq--;
    }
    ASSERT(nfree_mq == 0);
}
