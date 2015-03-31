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

#include <buffer/cc_fbuf.h>

#include <cc_debug.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <cc_pool.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FBUF_MODULE_NAME "ccommon::fbuf"


/*
 * fbuf header is at the tail end of the fbuf. This enables us to catch
 * buffer overrun early by asserting on the magic value during get or
 * put operations
 *
 *   <------------- fbuf_chunk_size ------------->
 *   +-------------------------------------------+
 *   |       fbuf body          |  fbuf header   |
 *   |     (fbuf_offset)        | (struct fbuf)  |
 *   +-------------------------------------------+
 *   ^           ^        ^     ^^
 *   |           |        |     ||
 *   \           |        |     |\
 *   fbuf->start \        |     | fbuf->end (one byte past valid bound)
 *                fbuf->rpos    \
 *                        \      fbuf
 *                        fbuf->wpos (one byte past valid byte)
 *
 */

FREEPOOL(fbuf_pool, fbufq, fbuf);
struct fbuf_pool fbufp;

static bool fbuf_init = false;
static bool fbufp_init = false;

static uint32_t fbuf_chunk_size = FBUF_SIZE; /* chunk size (all inclusive) */
static uint32_t fbuf_offset;     /* fbuf offset/data capacity (const) */

struct fbuf *
fbuf_create(void)
{
    struct fbuf *fbuf;
    uint8_t *buf;

    buf = (uint8_t *)cc_alloc(fbuf_chunk_size);
    if (buf == NULL) {
        log_info("fbuf creation failed due to OOM");

        return NULL;
    }

    fbuf = (struct fbuf *)(buf + fbuf_offset);
#if defined CC_ASSERT_PANIC || defined CC_ASSERT_LOG
    fbuf->magic = FBUF_MAGIC;
#endif
    fbuf->end = (uint8_t *)fbuf;
    fbuf->start = buf;
    STAILQ_NEXT(fbuf, next) = NULL;

    log_verb("created fbuf %p capacity %d", fbuf, fbuf->end - fbuf->start);

    return fbuf;
}

/*
 * free an fbuf (assuming it has already been unlinked and not corrupted)
 */
void
fbuf_destroy(struct fbuf **fbuf)
{
    uint8_t *buf = (uint8_t *)*fbuf;

    if (buf == NULL) {
        return;
    }

    log_verb("destroy fbuf %p capacity", buf);


    buf = buf - fbuf_offset;
    cc_free(buf);

    *fbuf = NULL;
}

/*
 * reset the fbuf by discarding the read or unread data that it might hold
 */
void
fbuf_reset(struct fbuf *fbuf)
{
    STAILQ_NEXT(fbuf, next) = NULL;
    fbuf->free = false;
    fbuf->rpos = fbuf->start;
    fbuf->wpos = fbuf->start;
}

/*
 * size of available/unread data in the fbuf- always less than 2^32(4G) bytes
 */
uint32_t
fbuf_rsize(struct fbuf *fbuf)
{
    ASSERT(fbuf->wpos >= fbuf->rpos);

    return (uint32_t)(fbuf->wpos - fbuf->rpos);
}

/*
 * size of remaining writable space in the fbuf- always less than 2^32(4G) bytes
 */
uint32_t
fbuf_wsize(struct fbuf *fbuf)
{
    ASSERT(fbuf->end >= fbuf->wpos);

    return (uint32_t)(fbuf->end - fbuf->wpos);
}

/*
 * total capacity of any fbuf, which are fixed sized and 2^32(4G) bytes at most
 */
uint32_t
fbuf_capacity(void)
{
    return fbuf_offset;
}

/*
 * insert the fbuf at the tail of the fbuf queue
 */
void
fbuf_insert(struct mq *mq, struct fbuf *fbuf)
{
    STAILQ_INSERT_TAIL(mq, fbuf, next);
    log_verb("insert fbuf %p len %d", fbuf, fbuf->wpos - fbuf->rpos);
}

/*
 * remove the fbuf from the fbuf queue
 */
void
fbuf_remove(struct mq *mq, struct fbuf *fbuf)
{
    log_verb("remove fbuf %p len %d", fbuf, fbuf->wpos - fbuf->rpos);

    STAILQ_REMOVE(mq, fbuf, fbuf, next);
    STAILQ_NEXT(fbuf, next) = NULL;
}

/*
 * move all pointers and any content to the left
 */
void
fbuf_lshift(struct fbuf *fbuf)
{
    uint32_t sz;

    sz = fbuf_rsize(fbuf);
    if (sz > 0) {
        cc_memmove(fbuf->start, fbuf->rpos, sz);
    }
    fbuf->rpos = fbuf->start;
    fbuf->wpos = fbuf->start + sz;
}

/*
 * move all pointers and any content to the right
 */
void
fbuf_rshift(struct fbuf *fbuf)
{
    uint32_t sz;

    sz = fbuf_rsize(fbuf);
    if (sz > 0) {
        cc_memmove(fbuf->end - sz, fbuf->rpos, sz);
    }
    fbuf->rpos = fbuf->end - sz;
    fbuf->wpos = fbuf->end;
}

/*
 * copy n bytes from memory area addr to fbuf.
 *
 * The memory areas should not overlap and the fbuf should have
 * enough space for n bytes.
 */
void
fbuf_copy(struct fbuf *fbuf, uint8_t *addr, uint32_t n)
{
    if (n == 0) {
        return;
    }

    /* fbuf has space for n bytes */
    ASSERT(!fbuf_full(fbuf) && n <= fbuf_wsize(fbuf));

    /* no overlapping copy */
    ASSERT(addr < fbuf->start || addr >= fbuf->end);

    cc_memcpy(fbuf->wpos, addr, n);
    fbuf->wpos += n;
}

void
fbuf_copy_bstring(struct fbuf *fbuf, const struct bstring bstr)
{
    fbuf_copy(fbuf, bstr.data, bstr.len);
}

void
fbuf_pool_create(uint32_t max)
{
    if (!fbufp_init) {
        uint32_t i;
        struct fbuf *buf;

        log_info("creating fbuf pool: max %"PRIu32, max);

        FREEPOOL_CREATE(&fbufp, max);
        fbufp_init = true;

        /**
         * NOTE: Right now I decide to preallocate if max != 0
         * whether we want an option where memory is capped but
         * not preallocated is a question for future exploration
         * So far I see no point of that.
         */
        if (max == 0) {
            return;
        }

        for (i = 0; i < max; ++i) {
            buf = fbuf_create();
            if (buf == NULL) {
                log_crit("cannot preallocate fbuf pool due to OOM, abort");
                exit(EXIT_FAILURE);
            }
            buf->free = true;
            FREEPOOL_RETURN(&fbufp, buf, next);
        }
    } else {
        log_warn("fbuf pool has already been created, ignore");
    }
}

void
fbuf_pool_destroy(void)
{
    struct fbuf *fbuf, *nbuf;

    if (fbufp_init) {
        log_info("destroying fbuf pool: free %"PRIu32, fbufp.nfree);

        FREEPOOL_DESTROY(fbuf, nbuf, &fbufp, next, fbuf_destroy);
        fbufp_init = false;
    } else {
        log_warn("fbuf pool was never created, ignore");
    }
}

/*
 * borrow a fully initialized fbuf
 */
struct fbuf *
fbuf_borrow(void)
{
    struct fbuf *fbuf;

    FREEPOOL_BORROW(fbuf, &fbufp, next, fbuf_create);

    if (fbuf == NULL) {
        log_debug("borrow fbuf failed: OOM or over limit");
        return NULL;
    }

    fbuf_reset(fbuf);

    log_verb("borrow fbuf %p", fbuf);

    return fbuf;
}

/*
 * return an fbuf to the pool
 */
void
fbuf_return(struct fbuf **fbuf)
{
    struct fbuf *buf = *fbuf;

    if (buf == NULL || buf->free) {
        return;
    }

    ASSERT(STAILQ_NEXT(buf, next) == NULL);
    ASSERT(buf->magic == FBUF_MAGIC);

    log_verb("return fbuf %p", buf);

    buf->free = true;
    FREEPOOL_RETURN(&fbufp, buf, next);

    *fbuf = NULL;
}

/*
 * initialize the fbuf module by setting the module-local constants
 */
void
fbuf_setup(uint32_t chunk_size)
{
    log_info("set up the %s module", FBUF_MODULE_NAME);

    fbuf_chunk_size = chunk_size;
    fbuf_offset = fbuf_chunk_size - FBUF_HDR_SIZE;
    if (fbuf_init) {
        log_warn("%s has already been setup, overwrite", FBUF_MODULE_NAME);
    }
    fbuf_init = true;
    ASSERT(fbuf_offset > 0 && fbuf_offset < fbuf_chunk_size);

    log_info("fbuf: chunk size %zu, hdr size %d, offset %zu",
              fbuf_chunk_size, FBUF_HDR_SIZE, fbuf_offset);
}

/*
 * de-initialize the fbuf module by releasing all fbufs from the free_mq
 */
void
fbuf_teardown(void)
{
    log_info("tear down the %s module", FBUF_MODULE_NAME);

    if (!fbuf_init) {
        log_warn("%s has never been setup", FBUF_MODULE_NAME);
    }
    fbuf_init = false;
}
