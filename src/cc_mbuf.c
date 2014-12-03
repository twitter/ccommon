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

#include <cc_mbuf.h>

#include <cc_debug.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <cc_pool.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MBUF_MODULE_NAME "ccommon::mbuf"


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

FREEPOOL(mbuf_pool, mbufq, mbuf);
struct mbuf_pool mbufp;

static bool mbuf_init = false;
static bool mbufp_init = false;

static uint32_t mbuf_chunk_size = MBUF_SIZE; /* chunk size (all inclusive) */
static uint32_t mbuf_offset;     /* mbuf offset/data capacity (const) */

struct mbuf *
mbuf_create(void)
{
    struct mbuf *mbuf;
    uint8_t *buf;

    buf = (uint8_t *)cc_alloc(mbuf_chunk_size);
    if (buf == NULL) {
        log_info("mbuf creation failed due to OOM");

        return NULL;
    }

    mbuf = (struct mbuf *)(buf + mbuf_offset);
#if defined CC_ASSERT_PANIC || defined CC_ASSERT_LOG
    mbuf->magic = MBUF_MAGIC;
#endif
    mbuf->end = (uint8_t *)mbuf;
    mbuf->start = buf;
    STAILQ_NEXT(mbuf, next) = NULL;

    log_verb("created mbuf %p capacity %d", mbuf, mbuf->end - mbuf->start);

    return mbuf;
}

/*
 * free an mbuf (assuming it has already been unlinked and not corrupted)
 */
void
mbuf_destroy(struct mbuf **mbuf)
{
    uint8_t *buf = (uint8_t *)*mbuf;

    if (buf == NULL) {
        return;
    }

    log_verb("destroy mbuf %p capacity", buf);


    buf = buf - mbuf_offset;
    cc_free(buf);

    *mbuf = NULL;
}

/*
 * reset the mbuf by discarding the read or unread data that it might hold
 */
void
mbuf_reset(struct mbuf *mbuf)
{
    STAILQ_NEXT(mbuf, next) = NULL;
    mbuf->free = false;
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
    return mbuf_offset;
}

/*
 * insert the mbuf at the tail of the mbuf queue
 */
void
mbuf_insert(struct mq *mq, struct mbuf *mbuf)
{
    STAILQ_INSERT_TAIL(mq, mbuf, next);
    log_verb("insert mbuf %p len %d", mbuf, mbuf->wpos - mbuf->rpos);
}

/*
 * remove the mbuf from the mbuf queue
 */
void
mbuf_remove(struct mq *mq, struct mbuf *mbuf)
{
    log_verb("remove mbuf %p len %d", mbuf, mbuf->wpos - mbuf->rpos);

    STAILQ_REMOVE(mq, mbuf, mbuf, next);
    STAILQ_NEXT(mbuf, next) = NULL;
}

/*
 * move all pointers and any content to the left
 */
void
mbuf_lshift(struct mbuf *mbuf)
{
    uint32_t sz;

    sz = mbuf_rsize(mbuf);
    cc_memmove(mbuf->start, mbuf->rpos, sz);
    mbuf->rpos = mbuf->start;
    mbuf->wpos = mbuf->start + sz;
}

/*
 * move all pointers and any content to the right
 */
void
mbuf_rshift(struct mbuf *mbuf)
{
    uint32_t sz;

    sz = mbuf_rsize(mbuf);
    cc_memmove(mbuf->end - sz, mbuf->rpos, sz);
    mbuf->rpos = mbuf->end - sz;
    mbuf->wpos = mbuf->end;
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
mbuf_copy_bstring(struct mbuf *mbuf, const struct bstring bstr)
{
    mbuf_copy(mbuf, bstr.data, bstr.len);
}

void
mbuf_pool_create(uint32_t max)
{
    if (!mbufp_init) {
        log_info("creating mbuf pool: max %"PRIu32, max);

        FREEPOOL_CREATE(&mbufp, max);
        mbufp_init = true;
    } else {
        log_warn("mbuf pool has already been created, ignore");
    }
}

void
mbuf_pool_destroy(void)
{
    struct mbuf *mbuf, *nbuf;

    if (mbufp_init) {
        log_info("destroying mbuf pool: free %"PRIu32, mbufp.nfree);

        FREEPOOL_DESTROY(mbuf, nbuf, &mbufp, next, mbuf_destroy);
        mbufp_init = false;
    } else {
        log_warn("mbuf pool was never created, ignore");
    }
}

/*
 * borrow a fully initialized mbuf
 */
struct mbuf *
mbuf_borrow(void)
{
    struct mbuf *mbuf;

    FREEPOOL_BORROW(mbuf, &mbufp, next, mbuf_create);

    if (mbuf == NULL) {
        log_debug("borrow mbuf failed: OOM or over limit");
        return NULL;
    }

    mbuf_reset(mbuf);

    log_verb("borrow mbuf %p", mbuf);

    return mbuf;
}

/*
 * return an mbuf to the pool
 */
void
mbuf_return(struct mbuf **mbuf)
{
    struct mbuf *buf = *mbuf;

    if (buf == NULL || buf->free) {
        return;
    }

    ASSERT(STAILQ_NEXT(buf, next) == NULL);
    ASSERT(buf->magic == MBUF_MAGIC);

    log_verb("return mbuf %p", buf);

    buf->free = true;
    FREEPOOL_RETURN(&mbufp, buf, next);

    *mbuf = NULL;
}

/*
 * initialize the mbuf module by setting the module-local constants
 */
void
mbuf_setup(uint32_t chunk_size)
{
    log_info("set up the %s module", MBUF_MODULE_NAME);

    mbuf_chunk_size = chunk_size;
    mbuf_offset = mbuf_chunk_size - MBUF_HDR_SIZE;
    if (mbuf_init) {
        log_warn("%s has already been setup, overwrite", MBUF_MODULE_NAME);
    }
    mbuf_init = true;
    ASSERT(mbuf_offset > 0 && mbuf_offset < mbuf_chunk_size);

    log_debug("mbuf: chunk size %zu, hdr size %d, offset %zu",
              mbuf_chunk_size, MBUF_HDR_SIZE, mbuf_offset);
}

/*
 * de-initialize the mbuf module by releasing all mbufs from the free_mq
 */
void
mbuf_teardown(void)
{
    log_info("tear down the %s module", MBUF_MODULE_NAME);

    if (!mbuf_init) {
        log_warn("%s has never been setup", MBUF_MODULE_NAME);
    }
    mbuf_init = false;
}
