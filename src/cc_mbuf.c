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
#include <stddef.h>

#include <cc_debug.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <cc_queue.h>
#include <cc_string.h>

#include <cc_mbuf.h>

static uint32_t nfree_mq;   /* # free mbuf */
static struct mq free_mq; /* free mbuf q */

static size_t mbuf_chunk_size; /* mbuf chunk size (all inclusive, const) */
static size_t mbuf_offset;     /* mbuf offset/body capacity (const) */

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

        ASSERT(mbuf_get_magic(mbuf) == MBUF_MAGIC);
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
    mbuf->data.magic = MBUF_MAGIC;

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

    mbuf = _mbuf_get();
    if (mbuf == NULL) {
        return NULL;
    }

    mbuf_set_end(mbuf, (uint8_t *)mbuf);
    mbuf_set_start(mbuf, mbuf_get_end(mbuf) - mbuf_offset);
    mbuf_set_rpos(mbuf, mbuf_get_start(mbuf));
    mbuf_set_wpos(mbuf, mbuf_get_start(mbuf));

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

    log_debug(LOG_VVERB, "free mbuf %p with %zu bytes of data", mbuf,
            mbuf_rsize(mbuf));

    ASSERT(STAILQ_NEXT(mbuf, next) == NULL);
    ASSERT(mbuf_get_magic(mbuf) == MBUF_MAGIC);

    buf = (uint8_t *)mbuf - mbuf_offset;
    cc_free(buf);
}

/*
 * put an mbuf back in the free mbuf queue
 */
void
mbuf_put(struct mbuf *mbuf)
{
    log_debug(LOG_VVERB, "put mbuf %p with %zu bytes of data", mbuf,
            mbuf_rsize(mbuf));

    ASSERT(STAILQ_NEXT(mbuf, next) == NULL);
    ASSERT(mbuf_get_magic(mbuf) == MBUF_MAGIC);

    nfree_mq++;
    STAILQ_INSERT_HEAD(&free_mq, mbuf, next);
}

/*
 * reset the mbuf by discarding the read or unread data that it might hold
 */
void
mbuf_reset(struct mbuf *mbuf)
{
    mbuf_set_rpos(mbuf, mbuf_get_start(mbuf));
    mbuf_set_wpos(mbuf, mbuf_get_start(mbuf));
}

/*
 * size of available/unread data in the mbuf- always less than 2^32(4G) bytes
 */
size_t
mbuf_rsize(struct mbuf *mbuf)
{
    ASSERT(mbuf_get_wpos(mbuf) >= mbuf_get_rpos(mbuf));

    return (uint32_t)(mbuf_get_wpos(mbuf) - mbuf_get_rpos(mbuf));
}

/*
 * size of remaining writable space in the mbuf- always less than 2^32(4G) bytes
 */
size_t
mbuf_wsize(struct mbuf *mbuf)
{
    ASSERT(mbuf_get_end(mbuf) >= mbuf_get_wpos(mbuf));

    return (uint32_t)(mbuf_get_end(mbuf) - mbuf_get_wpos(mbuf));
}

/*
 * total capacity of any mbuf, which are fixed sized and 2^32(4G) bytes at most
 */
size_t
mbuf_capacity(void)
{
    return mbuf_offset;
}

/*
 * insert the mbuf at the tail of the mbuf queue
 */
void
mbuf_insert(struct mq *mq, struct mbuf *mbuf)
{
    STAILQ_INSERT_TAIL(mq, mbuf, next);
    log_debug(LOG_VVERB, "insert mbuf %p with %zu bytes of data", mbuf,
            mbuf_rsize(mbuf));
}

/*
 * remove the mbuf from the mbuf queue
 */
void
mbuf_remove(struct mq *mq, struct mbuf *mbuf)
{
    log_debug(LOG_VVERB, "remove mbuf %p with %zu bytes of data", mbuf,
            mbuf_rsize(mbuf));

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
mbuf_copy(struct mbuf *mbuf, uint8_t *addr, size_t n)
{
    if (n == 0) {
        return;
    }

    /* mbuf has space for n bytes */
    ASSERT(!mbuf_full(mbuf) && n <= mbuf_wsize(mbuf));

    /* no overlapping copy */
    ASSERT(addr < mbuf_get_start(mbuf) || addr >= mbuf_get_end(mbuf));

    cc_memcpy(mbuf_get_wpos(mbuf), addr, n);
    mbuf_incr_wpos(mbuf, n);
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
    size_t size;

    nbuf = mbuf_get();
    if (nbuf == NULL) {
        return NULL;
    }

    if (cb != NULL) {
        /* precopy nbuf */
        cb(nbuf, cbarg);
    }

    /* copy data from mbuf to nbuf */
    size = (size_t)(mbuf_get_wpos(mbuf) - addr);
    mbuf_copy(nbuf, addr, size);

    /* adjust mbuf */
    mbuf_set_wpos(mbuf, addr);

    log_debug(LOG_VVERB, "split into mbuf %p len %"PRIu32" and nbuf %p len "
              "%"PRIu32" copied %zu bytes", mbuf, mbuf_rsize(mbuf), nbuf,
              mbuf_rsize(nbuf), size);

    return nbuf;
}

/*
 * initialize the mbuf module by setting the module-local constants
 */
void
mbuf_init(size_t chunk_size)
{
    nfree_mq = 0;
    STAILQ_INIT(&free_mq);

    mbuf_chunk_size = chunk_size;
    mbuf_offset = mbuf_chunk_size - MBUF_HDR_SIZE;
    ASSERT(mbuf_offset > 0 && mbuf_offset < mbuf_chunk_size);

    log_debug(LOG_DEBUG, "mbuf: chunk size %zu, body size %zu, data header ",
            "size %d, total header size %d", mbuf_chunk_size, mbuf_offset,
            MBUF_DATA_SIZE, MBUF_HDR_SIZE);
}

/*
 * de-initialize the mbuf module by releasing all mbufs from the free_mq
 */
void
mbuf_deinit(void)
{
    while (!STAILQ_EMPTY(&free_mq)) {
        struct mbuf *mbuf = STAILQ_FIRST(&free_mq);
        mbuf_remove(&free_mq, mbuf);
        mbuf_free(mbuf);
        nfree_mq--;
    }
    ASSERT(nfree_mq == 0);
}
