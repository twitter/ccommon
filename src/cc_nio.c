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
#include <event/cc_event.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define NIO_MODULE_NAME "ccommon::nio"

FREEPOOL(conn_pool, cq, conn);
static struct conn_pool cp;

static int max_backlog = 1024;

void
conn_setup(int backlog)
{
    log_debug(LOG_INFO, "set up the %s module", NIO_MODULE_NAME);
    log_debug(LOG_DEBUG, "conn size %zu", sizeof(struct conn));

    max_backlog = backlog;
}

void
conn_teardown(void)
{
    log_debug(LOG_INFO, "tear down the %s module", NIO_MODULE_NAME);
}

static int
conn_set_blocking(int sd)
{
    int flags;

    flags = fcntl(sd, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }

    return fcntl(sd, F_SETFL, flags & ~O_NONBLOCK);
}

static int
conn_set_nonblocking(int sd)
{
    int flags;

    flags = fcntl(sd, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }

    return fcntl(sd, F_SETFL, flags | O_NONBLOCK);
}

static int
conn_set_reuseaddr(int sd)
{
    int reuse;
    socklen_t len;

    reuse = 1;
    len = sizeof(reuse);

    return setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &reuse, len);
}

/*
 * Disable Nagle algorithm on TCP socket.
 *
 * This option helps to minimize transmit latency by disabling coalescing
 * of data to fill up a TCP segment inside the kernel. Sockets with this
 * option must use readv() or writev() to do data transfer in bulk and
 * hence avoid the overhead of small packets.
 */
static int
conn_set_tcpnodelay(int sd)
{
    int nodelay;
    socklen_t len;

    nodelay = 1;
    len = sizeof(nodelay);

    return setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &nodelay, len);
}

static int
conn_set_keepalive(int sd)
{
    int keepalive;
    socklen_t len;

    keepalive = 1;
    len = sizeof(keepalive);

    return setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, len);
}

static int
conn_set_linger(int sd, int timeout)
{
    struct linger linger;
    socklen_t len;

    linger.l_onoff = 1;
    linger.l_linger = timeout;

    len = sizeof(linger);

    return setsockopt(sd, SOL_SOCKET, SO_LINGER, &linger, len);
}

static int
conn_unset_linger(int sd)
{
    struct linger linger;
    socklen_t len;

    linger.l_onoff = 0;
    linger.l_linger = 0;

    len = sizeof(linger);

    return setsockopt(sd, SOL_SOCKET, SO_LINGER, &linger, len);
}

static int
conn_set_sndbuf(int sd, int size)
{
    socklen_t len;

    len = sizeof(size);

    return setsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, len);
}

static int
conn_set_rcvbuf(int sd, int size)
{
    socklen_t len;

    len = sizeof(size);

    return setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, len);
}

static int
conn_get_soerror(int sd)
{
    int status, err;
    socklen_t len;

    err = 0;
    len = sizeof(err);

    status = getsockopt(sd, SOL_SOCKET, SO_ERROR, &err, &len);
    if (status == 0) {
        errno = err;
    }

    return status;
}

static int
conn_get_sndbuf(int sd)
{
    int status, size;
    socklen_t len;

    size = 0;
    len = sizeof(size);

    status = getsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, &len);
    if (status < 0) {
        return status;
    }

    return size;
}

static int
conn_get_rcvbuf(int sd)
{
    int status, size;
    socklen_t len;

    size = 0;
    len = sizeof(size);

    status = getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, &len);
    if (status < 0) {
        return status;
    }

    return size;
}

static void
conn_maximize_sndbuf(int sd)
{
    int status, min, max, avg;

    /* start with the default size */
    min = conn_get_sndbuf(sd);
    if (min < 0) {
        return;
    }

    /* binary-search for the real maximum */
    max = 256 * MiB;

    while (min <= max) {
        avg = (min + max) / 2;
        status = conn_set_sndbuf(sd, avg);
        if (status != 0) {
            max = avg - 1;
        } else {
            min = avg + 1;
        }
    }
}



void
conn_reset(struct conn *c)
{
    c->sd = 0;

    c->recv_nbyte = 0;
    c->send_nbyte = 0;

    c->mode = 0;
    c->state = 0;
    c->flags = 0;

    c->err = 0;
}

struct conn *
conn_create(void)
{
    return cc_alloc(sizeof(struct conn));
}

void
conn_destroy(struct conn *c)
{
    cc_free(c);
}

/* no conn_open() because connections are opened by server_accept */

void
conn_close(struct conn *c)
{
    close(c->sd);
    conn_return(c);
}

void
server_close(struct conn *c)
{
    log_debug(LOG_INFO, "returning conn %p sd %d", c, c->sd);

    close(c->sd);
    conn_return(c);
}

struct conn *
server_accept(struct conn *sc)
{
    rstatus_t status;
    struct conn *c;
    int sd;

    ASSERT(sc->sd > 0);

    for (;;) {
        sd = accept(sc->sd, NULL, NULL);
        if (sd < 0) {
            if (errno == EINTR) {
                log_debug(LOG_VERB, "accept on sd %d not ready: eintr",
                        sc->sd);
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                log_debug(LOG_VERB, "accept on s %d not ready - eagain",
                        sc->sd);
                return NULL;
            }

            log_error("accept on s %d failed: %s", sc->sd, strerror(errno));
            return NULL;
        }

        break;
    }

    c = conn_borrow();
    c->sd = sd;
    if (c == NULL) {
        log_error("accept failed: cannot get connection struct");
        status = close(sd);
        if (status < 0) {
            log_error("close c %d failed, ignored: %s", sd, strerror(errno));
        }
        return NULL;
    }

    status = conn_set_nonblocking(sd);
    if (status < 0) {
        log_error("set nonblock on c %d failed, ignored: %s", sd,
                strerror(errno));
        return c;
    }

    status = conn_set_tcpnodelay(sd);
    if (status < 0) {
        log_warn("set tcp nodely on c %d failed, ignored: %s", sd,
                 strerror(errno));
    }

    log_debug(LOG_INFO, "accepted c %d on sd %d", c->sd, sc->sd);

    return c;
}

struct conn *
server_listen(struct addrinfo *ai)
{
    rstatus_t status;
    struct conn *sc;
    int sd;

    sd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sd < 0) {
        log_error("socket failed: %s", strerror(errno));
        return NULL;
    }

    status = conn_set_reuseaddr(sd);
    if (status != CC_OK) {
        log_error("reuse of sd %d failed: %s", sd, strerror(errno));
        return NULL;
    }

    status = bind(sd, ai->ai_addr, ai->ai_addrlen);
    if (status < 0) {
        log_error("bind on sd %d failed: %s", sd, strerror(errno));
        return NULL;
    }

    status = listen(sd, max_backlog);
    if (status < 0) {
        log_error("listen on sd %d failed: %s", sd, strerror(errno));
        return NULL;
    }

    status = conn_set_nonblocking(sd);
    if (status != CC_OK) {
        log_error("set nonblock on sd %d failed: %s", sd, strerror(errno));
        return NULL;
    }

    sc = conn_borrow();
    if (sc == NULL) {
        log_error("borrow conn for s %d failed: %s", sd, strerror(errno));
        status = close(sd);
        if (status < 0) {
            log_error("close s %d failed, ignored: %s", sd, strerror(errno));
        }
        return NULL;
    }
    sc->sd = sd;

    log_debug(LOG_NOTICE, "server listen setup on s %d", sc->sd);

    return sc;
}

/*
 * try reading nbyte bytes from conn and place the data in buf
 * EINTR is continued, EAGAIN is explicitly flagged in return, other errors are
 * returned as a generic error/failure.
 */
ssize_t
conn_recv(struct conn *c, void *buf, size_t nbyte)
{
    ssize_t n;

    ASSERT(buf != NULL);
    ASSERT(nbyte > 0);

    log_debug(LOG_VERB, "recv on sd %d, total %zu bytes", c->sd, nbyte);

    for (;;) {
        n = cc_read(c->sd, buf, nbyte);

        log_debug(LOG_VERB, "read on sd %d %zd of %zu", c->sd, n, nbyte);

        if (n > 0) {
            c->recv_nbyte += (size_t)n;
            return n;
        }

        if (n == 0) {
            c->state = CONN_EOF;
            log_debug(LOG_INFO, "recv on sd %d eof rb  %zu sb %zu", c->sd,
                      c->recv_nbyte, c->send_nbyte);
            return n;
        }

        /* n < 0 */
        if (errno == EINTR) {
            log_debug(LOG_VERB, "recv on sd %d not ready - EINTR", c->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            log_debug(LOG_VERB, "recv on sd %d not ready - EAGAIN", c->sd);
            return CC_EAGAIN;
        } else {
            c->err = errno;
            log_error("recv on sd %d failed: %s", c->sd, strerror(errno));
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
conn_recvv(struct conn *c, struct array *bufv, size_t nbyte)
{
    /* TODO(yao): this is almost identical with conn_recv except for the call
     * to cc_readv. Consolidate the two?
     */
    ssize_t n;

    ASSERT(array_nelem(bufv) > 0);
    ASSERT(nbyte != 0);

    log_debug(LOG_VERB, "recvv on sd %d, total %zu bytes", c->sd, nbyte);

    for (;;) {
        n = cc_readv(c->sd, bufv->data, bufv->nelem);

        log_debug(LOG_VERB, "recvv on sd %d %zd of %zu in %"PRIu32" buffers",
                  c->sd, n, nbyte, bufv->nelem);

        if (n > 0) {
            c->recv_nbyte += (size_t)n;

            return n;
        }

        if (n == 0) {
            log_warn("recvv on sd %d returned zero", c->sd);

            return 0;
        }

        if (errno == EINTR) {
            log_debug(LOG_VERB, "recvv on sd %d not ready - eintr", c->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {

            log_debug(LOG_VERB, "recvv on sd %d not ready - eagain", c->sd);
            return CC_EAGAIN;
        } else {

            c->err = errno;
            log_error("recvv on sd %d failed: %s", c->sd, strerror(errno));
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
conn_send(struct conn *c, void *buf, size_t nbyte)
{
    ssize_t n;

    ASSERT(buf != NULL);
    ASSERT(nbyte > 0);

    log_debug(LOG_VERB, "send on sd %d, total %zu bytes", c->sd, nbyte);

    for (;;) {
        n = cc_write(c->sd, buf, nbyte);

        log_debug(LOG_VERB, "write on sd %d %zd of %zu", c->sd, n, nbyte);

        if (n > 0) {
            c->send_nbyte += (size_t)n;
            return n;
        }

        if (n == 0) {
            log_warn("sendv on sd %d returned zero", c->sd);
            return 0;
        }

        /* n < 0 */
        if (errno == EINTR) {
            log_debug(LOG_VERB, "send on sd %d not ready - EINTR", c->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            log_debug(LOG_VERB, "send on sd %d not ready - EAGAIN", c->sd);
            return CC_EAGAIN;
        } else {
            c->err = errno;
            log_error("sendv on sd %d failed: %s", c->sd, strerror(errno));
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
conn_sendv(struct conn *c, struct array *bufv, size_t nbyte)
{
    /* TODO(yao): this is almost identical with conn_send except for the call
     * to cc_writev. Consolidate the two? Revisit these functions when we build
     * more concrete backend systems.
     */
    ssize_t n;

    ASSERT(array_nelem(bufv) > 0);
    ASSERT(nbyte != 0);

    log_debug(LOG_VERB, "sendv on sd %d, total %zu bytes", c->sd, nbyte);

    for (;;) {
        n = cc_writev(c->sd, bufv->data, bufv->nelem);

        log_debug(LOG_VERB, "sendv on sd %d %zd of %zu in %"PRIu32" buffers",
                  c->sd, n, nbyte, bufv->nelem);

        if (n > 0) {
            c->send_nbyte += (size_t)n;
            return n;
        }

        if (n == 0) {
            log_warn("sendv on sd %d returned zero", c->sd);
            return 0;
        }

        if (errno == EINTR) {
            log_debug(LOG_VERB, "sendv on sd %d not ready - eintr", c->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            log_debug(LOG_VERB, "sendv on sd %d not ready - eagain", c->sd);
            return CC_EAGAIN;
        } else {
            c->err = errno;
            log_error("sendv on sd %d failed: %s", c->sd, strerror(errno));
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

    FREEPOOL_CREATE(&cp, max);
}

void
conn_pool_destroy(void)
{
    struct conn *c, *tc;

    log_debug(LOG_INFO, "destroying conn pool: free %"PRIu32, cp.nfree);

    FREEPOOL_DESTROY(c, tc, &cp, next, conn_destroy);
}

struct conn *
conn_borrow(void)
{
    struct conn *c;

    FREEPOOL_BORROW(c, &cp, next, conn_create);

    if (c == NULL) {
        log_debug(LOG_DEBUG, "borrow conn failed: OOM");
        return NULL;
    }

    conn_reset(c);

    log_debug(LOG_VERB, "borrow conn %p", c);

    return c;
}

void
conn_return(struct conn *c)
{
    log_debug(LOG_VERB, "return conn %p", c);

    FREEPOOL_RETURN(&cp, c, next);
}
