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

#include <buffer/cc_buf.h>

#include <cc_mm.h>
#include <cc_pool.h>

#define BUF_MODULE_NAME "ccommon::buf"

uint32_t buf_size = BUF_SIZE;

FREEPOOL(buf_pool, bufq, buf);
static struct buf_pool bufp;

static bool buf_init = false;
static bool bufp_init = false;

void
buf_setup(uint32_t size)
{
    log_info("setting up the %s module", BUF_MODULE_NAME);

    buf_size = size;

    if (buf_init) {
        log_warn("%s was already setup, overwriting", BUF_MODULE_NAME);
    }

    buf_init = true;

    log_info("buf: size %zu");
}

void
buf_teardown(void)
{
    log_info("tear down the %s module", BUF_MODULE_NAME);

    if (!buf_init) {
        log_warn("%s was not setup but is being torn down", BUF_MODULE_NAME);
    }

    buf_init = false;
}

void
buf_pool_create(uint32_t max)
{
    if (bufp_init) {
        log_warn("buf pool has already been created, ignoring");
    } else {
        uint32_t i;
        struct buf *buf;

        log_info("creating buf pool: max %"PRIu32, max);

        FREEPOOL_CREATE(&bufp, max);
        bufp_init = true;

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
            buf = buf_create();
            if (buf == NULL) {
                log_crit("cannot preallocate buf pool, OOM. abort");
                exit(EXIT_FAILURE);
            }
            buf->free = 1;
            FREEPOOL_RETURN(&bufp, buf, next);
        }
    }
}

void
buf_pool_destroy(void)
{
    struct buf *buf, *nbuf;

    if (bufp_init) {
        log_info("destroying buf pool: free %"PRIu32, bufp.nfree);

        FREEPOOL_DESTROY(buf, nbuf, &bufp, next, buf_destroy);
        bufp_init = false;
    } else {
        log_warn("buf pool was never created, ignoring destroy");
    }
}

struct buf *
buf_borrow(void)
{
    struct buf *buf;

    FREEPOOL_BORROW(buf, &bufp, next, buf_create);

    if (buf == NULL) {
        log_warn("borrow buf failed, OOM or over limit");
        return NULL;
    }

    buf_reset(buf);

    log_verb("borrow buf %p", buf);

    return buf;
}

void
buf_return(struct buf **buf)
{
    struct buf *elm;

    if (buf == NULL || (elm = *buf) == NULL || elm->free) {
        return;
    }

    ASSERT(STAILQ_NEXT(elm, next) == NULL);
    ASSERT(elm->wpos <= elm->end);
    ASSERT(elm->end - (uint8_t *)elm == buf_size);

    log_verb("return buf %p", elm);

    elm->free = 1;
    FREEPOOL_RETURN(&bufp, elm, next);

    *buf = NULL;
}

void
buf_reset(struct buf *buf)
{
    STAILQ_NEXT(buf, next) = NULL;
    buf->free = 0;
    buf->rpos = buf->wpos = buf->begin;
}

struct buf *
buf_create(void)
{
    struct buf *buf;

    if ((buf = cc_alloc(buf_size)) == NULL) {
        log_info("buf creation failed due to OOM");
        return NULL;
    }

    buf->end = (uint8_t *)buf + buf_size;
    STAILQ_NEXT(buf, next) = NULL;
    buf->rpos = buf->wpos = buf->begin;

    log_verb("created buf %p capacity %"PRIu32, buf, buf_capacity(buf));

    return buf;
}

void
buf_destroy(struct buf **buf)
{
    if (buf == NULL || *buf == NULL) {
        return;
    }

    log_verb("destroy buf %p capacity %"PRIu32, *buf, buf_capacity(*buf));

    cc_free(*buf);

    *buf = NULL;
}

uint32_t
buf_rsize(struct buf *buf)
{
    ASSERT(buf != NULL);
    ASSERT(buf->rpos <= buf->wpos);

    return (uint32_t)(buf->wpos - buf->rpos);
}

uint32_t
buf_wsize(struct buf *buf)
{
    ASSERT(buf != NULL);
    ASSERT(buf->wpos <= buf->end);

    return (uint32_t)(buf->end - buf->wpos);
}

uint32_t
buf_capacity(struct buf *buf)
{
    ASSERT(buf != NULL);
    ASSERT(buf->begin <= buf->end);

    return (uint32_t)(buf->end - buf->begin);
}

void
buf_lshift(struct buf *buf)
{
    ASSERT(buf != NULL);

    uint32_t size = buf_rsize(buf);

    if (size > 0) {
        cc_memmove(buf->begin, buf->rpos, size);
    }

    buf->rpos = buf->begin;
    buf->wpos = buf->begin + size;
}

void
buf_rshift(struct buf *buf)
{
    ASSERT(buf != NULL);

    uint32_t size = buf_rsize(buf);

    if (size > 0) {
        cc_memmove(buf->end - size, buf->rpos, size);
    }

    buf->rpos = buf->end - size;
    buf->wpos = buf->end;
}

uint32_t
buf_read(uint8_t *dst, uint32_t count, struct buf *buf)
{
    ASSERT(buf != NULL && dst != NULL);

    uint32_t read_len;

    read_len = buf_rsize(buf) < count ? buf_rsize(buf) : count;

    cc_memmove(dst, buf->rpos, read_len);
    buf->rpos += read_len;

    if (buf_rsize(buf) == 0) {
        buf_lshift(buf);
    }

    return read_len;
}

uint32_t
buf_write(uint8_t *src, uint32_t count, struct buf *buf)
{
    ASSERT(buf != NULL && src != NULL);

    uint32_t write_len = buf_wsize(buf) < count ? buf_wsize(buf) : count;

    cc_memcpy(buf->wpos, src, write_len);
    buf->wpos += write_len;

    return write_len;
}

uint32_t
buf_read_bstring(struct buf *buf, struct bstring *bstr)
{
    return buf_read(bstr->data, bstr->len, buf);
}

uint32_t
buf_write_bstring(struct buf *buf, const struct bstring *bstr)
{
    return buf_write(bstr->data, bstr->len, buf);
}
