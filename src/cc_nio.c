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

#include <cc_nio.h>

#include <cc_debug.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <cc_pool.h>
#include <cc_util.h>

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define NIO_MODULE_NAME "ccommon::nio"

FREEPOOL(conn_pool, connq, conn);
struct conn_pool connp;

void
conn_setup(void)
{
    log_debug(LOG_INFO, "set up the %s module", NIO_MODULE_NAME);
    log_debug(LOG_DEBUG, "conn size %zu", sizeof(struct conn));
}

void
conn_teardown(void)
{
    log_debug(LOG_INFO, "tear down the %s module", NIO_MODULE_NAME);
}

void
conn_reset(struct conn *conn)
{
    conn->sd = 0;
    conn->family = 0;
    conn->addrlen = 0;
    conn->addr = NULL;

    conn->recv_nbyte = 0;
    conn->send_nbyte = 0;

    conn->recv_active = false;
    conn->send_active = false;
    conn->recv_ready = false;
    conn->send_ready = false;

    conn->mode = 0;
    conn->state = 0;
    conn->flags = 0;

    conn->err = 0;
}

struct conn *
conn_create(void)
{
    return cc_alloc(sizeof(struct conn));
}

void
conn_destroy(struct conn *conn)
{
    cc_free(conn->addr);
    cc_free(conn);
}

/*
 * try reading nbyte bytes from conn and place the data in buf
 * EINTR is continued, EAGAIN is explicitly flagged in return, other errors are
 * returned as a generic error/failure.
 */
ssize_t
conn_recv(struct conn *conn, void *buf, size_t nbyte)
{
    ssize_t n;

    ASSERT(buf != NULL);
    ASSERT(nbyte > 0);
    ASSERT(conn->recv_ready);

    log_debug(LOG_VERB, "recv on sd %d, total %zu bytes", conn->sd, nbyte);

    for (;;) {
        n = cc_read(conn->sd, buf, nbyte);

        log_debug(LOG_VERB, "read on sd %d %zd of %zu", conn->sd, n, nbyte);

        if (n > 0) {
            if (n < (ssize_t) nbyte) {
                conn->recv_ready = 0;
            }
            conn->recv_nbyte += (size_t)n;
            return n;
        }

        if (n == 0) {
            conn->recv_ready = 0;
            conn->state = CONN_EOF;
            log_debug(LOG_INFO, "recv on sd %d eof rb  %zu sb %zu", conn->sd,
                      conn->recv_nbyte, conn->send_nbyte);
            return n;
        }

        /* n < 0 */
        if (errno == EINTR) {
            log_debug(LOG_VERB, "recv on sd %d not ready - EINTR", conn->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            conn->recv_ready = 0;
            log_debug(LOG_VERB, "recv on sd %d not ready - EAGAIN", conn->sd);
            return CC_EAGAIN;
        } else {
            conn->recv_ready = 0;
            conn->err = errno;
            log_error("recv on sd %d failed: %s", conn->sd, strerror(errno));
            return CC_ERROR;
        }
    }

    NOT_REACHED();

    return CC_ERROR;
}

/*
 * vector version of conn_recv, using readv to read into a mbuf array
 */
ssize_t
conn_recvv(struct conn *conn, struct array *bufv, size_t nbyte)
{
    /* TODO(yao): this is almost identical with conn_recv except for the call
     * to cc_readv. Consolidate the two?
     */
    ssize_t n;

    ASSERT(array_nelem(bufv) > 0);
    ASSERT(nbyte != 0);
    ASSERT(conn->recv_ready);

    log_debug(LOG_VERB, "recvv on sd %d, total %zu bytes", conn->sd, nbyte);

    for (;;) {
        n = cc_readv(conn->sd, bufv->data, bufv->nelem);

        log_debug(LOG_VERB, "recvv on sd %d %zd of %zu in %"PRIu32" buffers",
                  conn->sd, n, nbyte, bufv->nelem);

        if (n > 0) {
            if (n < (ssize_t) nbyte) {
                conn->recv_ready = 0;
            }
            conn->recv_nbyte += (size_t)n;
            return n;
        }

        if (n == 0) {
            log_warn("recvv on sd %d returned zero", conn->sd);
            conn->recv_ready = 0;
            return 0;
        }

        if (errno == EINTR) {
            log_debug(LOG_VERB, "recvv on sd %d not ready - eintr", conn->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            conn->recv_ready = 0;
            log_debug(LOG_VERB, "recvv on sd %d not ready - eagain", conn->sd);
            return CC_EAGAIN;
        } else {
            conn->recv_ready = 0;
            conn->err = errno;
            log_error("recvv on sd %d failed: %s", conn->sd, strerror(errno));
            return CC_ERROR;
        }
    }

    NOT_REACHED();

    return CC_ERROR;
}

/*
 * try writing nbyte to conn and store the data in buf
 * EINTR is continued, EAGAIN is explicitly flagged in return, other errors are
 * returned as a generic error/failure.
 */
ssize_t
conn_send(struct conn *conn, void *buf, size_t nbyte)
{
    ssize_t n;

    ASSERT(buf != NULL);
    ASSERT(nbyte > 0);
    ASSERT(conn->send_ready);

    log_debug(LOG_VERB, "send on sd %d, total %zu bytes", conn->sd, nbyte);

    for (;;) {
        n = cc_write(conn->sd, buf, nbyte);

        log_debug(LOG_VERB, "write on sd %d %zd of %zu", conn->sd, n, nbyte);

        if (n > 0) {
            if (n < (ssize_t) nbyte) {
                conn->send_ready = 0;
            }
            conn->send_nbyte += (size_t)n;
            return n;
        }

        if (n == 0) {
            log_warn("sendv on sd %d returned zero", conn->sd);
            conn->send_ready = 0;
            return 0;
        }

        /* n < 0 */
        if (errno == EINTR) {
            log_debug(LOG_VERB, "send on sd %d not ready - EINTR", conn->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            conn->send_ready = 0;
            log_debug(LOG_VERB, "send on sd %d not ready - EAGAIN", conn->sd);
            return CC_EAGAIN;
        } else {
            conn->send_ready = 0;
            conn->err = errno;
            log_error("sendv on sd %d failed: %s", conn->sd, strerror(errno));
            return CC_ERROR;
        }
    }

    NOT_REACHED();

    return CC_ERROR;
}

/*
 * vector version of conn_send, using writev to send an array of bufs
 */
ssize_t
conn_sendv(struct conn *conn, struct array *bufv, size_t nbyte)
{
    /* TODO(yao): this is almost identical with conn_send except for the call
     * to cc_writev. Consolidate the two? Revisit these functions when we build
     * more concrete backend systems.
     */
    ssize_t n;

    ASSERT(array_nelem(bufv) > 0);
    ASSERT(nbyte != 0);
    ASSERT(conn->send_ready);

    log_debug(LOG_VERB, "sendv on sd %d, total %zu bytes", conn->sd, nbyte);

    for (;;) {
        n = cc_writev(conn->sd, bufv->data, bufv->nelem);

        log_debug(LOG_VERB, "sendv on sd %d %zd of %zu in %"PRIu32" buffers",
                  conn->sd, n, nbyte, bufv->nelem);

        if (n > 0) {
            if (n < (ssize_t) nbyte) {
                conn->send_ready = 0;
            }
            conn->send_nbyte += (size_t)n;
            return n;
        }

        if (n == 0) {
            log_warn("sendv on sd %d returned zero", conn->sd);
            conn->send_ready = 0;
            return 0;
        }

        if (errno == EINTR) {
            log_debug(LOG_VERB, "sendv on sd %d not ready - eintr", conn->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            conn->send_ready = 0;
            log_debug(LOG_VERB, "sendv on sd %d not ready - eagain", conn->sd);
            return CC_EAGAIN;
        } else {
            conn->send_ready = 0;
            conn->err = errno;
            log_error("sendv on sd %d failed: %s", conn->sd, strerror(errno));
            return CC_ERROR;
        }
    }

    NOT_REACHED();

    return CC_ERROR;
}

void
conn_pool_create(uint32_t max)
{
    log_debug(LOG_INFO, "creating conn pool: max %"PRIu32, max);

    FREEPOOL_CREATE(&connp, max);
}

void
conn_pool_destroy(void)
{
    struct conn *conn, *tconn;

    log_debug(LOG_INFO, "destroying conn pool: free %"PRIu32, connp.nfree);

    FREEPOOL_DESTROY(conn, tconn, &connp, next, conn_destroy);
}

struct conn *
conn_borrow(void)
{
    struct conn *conn;

    FREEPOOL_BORROW(conn, &connp, next, conn_create);

    if (conn == NULL) {
        log_debug(LOG_DEBUG, "borrow conn failed: OOM");
        return NULL;
    }

    conn_reset(conn);

    log_debug(LOG_VVERB, "borrow conn %p", conn);

    return conn;
}

void
conn_return(struct conn *conn)
{
    log_debug(LOG_VVERB, "return conn *p", conn);

    FREEPOOL_RETURN(&connp, conn, next);
}
