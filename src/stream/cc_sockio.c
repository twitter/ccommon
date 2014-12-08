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

#include <stream/cc_sockio.h>

#include <cc_debug.h>
#include <cc_define.h>
#include <cc_log.h>
#include <cc_mbuf.h>
#include <cc_mm.h>
#include <cc_pool.h>
#include <cc_util.h>
#include <channel/cc_tcp.h>

#include <limits.h>
#include <sys/uio.h>

/*
#if (IOV_MAX > 128)
#define CC_IOV_MAX 128
#else
#define CC_IOV_MAX IOV_MAX
#endif
*/

#define SOCKIO_MODULE_NAME "ccommon::sockio"

FREEPOOL(buf_sock_pool, buf_sockq, buf_sock);
struct buf_sock_pool bsp;

static bool bsp_init = false;

rstatus_t
buf_tcp_read(struct buf_sock *s)
{
    ASSERT(s != NULL);

    struct conn *c = (struct conn *)s->ch;
    channel_handler_t *h = s->hdl;
    struct mbuf *buf = s->rbuf;
    rstatus_t status = CC_OK;
    ssize_t cap, n, nbyte;

    ASSERT(c != NULL && h != NULL && buf != NULL);
    ASSERT(c->type == CHANNEL_TCP);
    ASSERT(h->recv != NULL);

    cap = mbuf_wsize(buf);

    if (cap == 0) {
        return CC_ENOMEM;
    }

    n = h->recv(c, buf->wpos, cap);
    if (n < 0) {
        if (n == CC_EAGAIN) {
            status = CC_OK;
        } else {
            status = CC_ERROR;
        }
    } else if (n == 0) {
        status = CC_ERDHUP;
        c->state = CONN_EOF;
    } else if (n == cap) {
        status = CC_ERETRY;
    } else {
        status = CC_OK;
    }
    nbyte = (n > 0) ? n : 0;
    buf->wpos += nbyte;
    c->recv_nbyte += nbyte;

    log_verb("recv %zd bytes on conn %p", n, c);

    return status;
}

rstatus_t
buf_tcp_write(struct buf_sock *s)
{
    ASSERT(s != NULL);

    struct conn *c = (struct conn *)s->ch;
    channel_handler_t *h = s->hdl;
    struct mbuf *buf = s->wbuf;
    rstatus_t status = CC_OK;
    size_t cap, n;

    ASSERT(c != NULL && h != NULL && buf != NULL);
    ASSERT(c->type == CHANNEL_TCP);
    ASSERT(h->send != NULL);

    cap = mbuf_rsize(buf);

    if (cap == 0) {
        log_verb("no data to send in buf at %p ", buf);

        return CC_EEMPTY;
    }

    n = h->send(c, buf->rpos, cap);
    if (n < 0) {
        if (n == CC_EAGAIN) {
            log_verb("send on conn returns rescuable error: EAGAIN", c);
            status = CC_EAGAIN;
        } else {
            log_info("send on conn %p returns other error: %d", c, n);
            status = CC_ERROR;
        }
    } else if (n < cap) {
        log_debug("unwritten data remain on conn %p, should retry", c);
        status = CC_ERETRY;
    } else {
        status = CC_OK;
    }
    buf->rpos += (n > 0) ? n : 0;

    log_verb("send %zd bytes on conn %p", n, c);

    return status;
}

struct buf_sock *
buf_sock_create(void)
{
    struct buf_sock *s;

    s = (struct buf_sock *)cc_alloc(sizeof(struct buf_sock));
    if (s == NULL) {
        return NULL;
    }
    STAILQ_NEXT(s, next) = NULL;
    s->owner = NULL;
    s->free = false;
    s->hdl = NULL;
    s->ch = NULL;
    s->rbuf = NULL;
    s->wbuf = NULL;

    s->ch = conn_create();
    if (s->ch == NULL) {
        goto error;
    }
    s->rbuf = mbuf_create();
    if (s->rbuf == NULL) {
        goto error;
    }
    s->wbuf = mbuf_create();
    if (s->wbuf == NULL) {
        goto error;
    }

    log_verb("created buffered socket %p", s);

    return s;

error:
    log_info("buffered socket creation failed");
    buf_sock_destroy(&s);

    return NULL;
}

void
buf_sock_destroy(struct buf_sock **buf_sock)
{
    struct buf_sock *s = *buf_sock;

    if (s == NULL) {
        return;
    }

    log_verb("destroy buffered socket %p", s);

    conn_destroy(&s->ch);
    mbuf_destroy(&s->rbuf);
    mbuf_destroy(&s->wbuf);
    cc_free(s);

    *buf_sock = NULL;
}

void
buf_sock_pool_create(uint32_t max)
{
    if (!bsp_init) {
        uint32_t i;
        struct buf_sock *s;

        log_info("creating buffered socket pool: max %"PRIu32, max);

        FREEPOOL_CREATE(&bsp, max);
        bsp_init = true;

        /* preallocating, see notes in cc_mbuf.c */
        if (max == 0) {
            return;
        }

        for (i = 0; i < max; ++i) {
            s = buf_sock_create();
            if (s == NULL) {
                log_crit("cannot preallocate buffered socket pool due to OOM, "
                        "abort");
                exit(EXIT_FAILURE);
            }
            s->free = true;
            FREEPOOL_RETURN(&bsp, s, next);
        }
    } else {
        log_warn("buffered socket pool has already been created, ignore");
    }
}

void
buf_sock_pool_destroy(void)
{
    struct buf_sock *s, *ts;

    if (bsp_init) {
        log_info("destroying buffered socket pool: free %"PRIu32, bsp.nfree);

        FREEPOOL_DESTROY(s, ts, &bsp, next, buf_sock_destroy);
        bsp_init = false;
    } else {
        log_warn("buffered socket pool was never created, ignore");
    }
}

void
buf_sock_reset(struct buf_sock *s)
{
    ASSERT(s->rbuf != NULL && s->wbuf != NULL);

    log_verb("reset buffered socket %p", s);

    STAILQ_NEXT(s, next) = NULL;
    s->owner = NULL;
    s->free = false;
    s->flag = 0;
    s->data = NULL;
    s->hdl = NULL;

    conn_reset(s->ch);
    mbuf_reset(s->rbuf);
    mbuf_reset(s->wbuf);
}

struct buf_sock *
buf_sock_borrow(void)
{
    struct buf_sock *s;

    FREEPOOL_BORROW(s, &bsp, next, buf_sock_create);
    if (s == NULL) {
        log_debug("borrow buffered socket failed: OOM or over limit");

        return NULL;
    }
    buf_sock_reset(s);

    log_verb("borrowed buffered socket %p", s);

    return s;
}

void
buf_sock_return(struct buf_sock **buf_sock)
{
    struct buf_sock *s = *buf_sock;

    if (s == NULL || s->free) {
        return;
    }

    log_verb("return buffered socket %p", s);

    s->free = true;
    FREEPOOL_RETURN(&bsp, s, next);

    *buf_sock = NULL;
}
