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

#include <channel/cc_tcp.h>

#include <cc_debug.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <cc_pool.h>
#include <cc_util.h>
#include <cc_event.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>

#define TCP_MODULE_NAME "ccommon::tcp"

FREEPOOL(conn_pool, cq, conn);
static struct conn_pool cp;

static bool tcp_init = false;
static bool cp_init = false;
static int max_backlog = TCP_BACKLOG;

void
tcp_setup(int backlog)
{
    log_info("set up the %s module", TCP_MODULE_NAME);
    log_debug("conn size %zu", sizeof(struct conn));

    max_backlog = backlog;
    if (tcp_init) {
        log_warn("%s has already been setup, overwrite", TCP_MODULE_NAME);
    }
    tcp_init = true;
}

void
tcp_teardown(void)
{
    log_info("tear down the %s module", TCP_MODULE_NAME);

    if (!tcp_init) {
        log_warn("%s has never been setup", TCP_MODULE_NAME);
    }
    tcp_init = false;
}

void
conn_reset(struct conn *c)
{
    STAILQ_NEXT(c, next) = NULL;
    c->free = false;

    c->type = CHANNEL_TCP;
    c->level = CHANNEL_INVALID;
    c->sd = 0;

    c->recv_nbyte = 0;
    c->send_nbyte = 0;

    c->state = TCP_UNKNOWN;
    c->flags = 0;

    c->err = 0;
}

struct conn *
conn_create(void)
{
    struct conn *c = (struct conn *)cc_alloc(sizeof(struct conn));

    if (c == NULL) {
        log_info("connection creation failed due to OOM");
    } else {
        log_verb("created conn %p", c);
    }

    conn_reset(c);

    return c;
}

void
conn_destroy(struct conn **conn)
{
    struct conn *c = *conn;

    if (c == NULL) {
        return;
    }

    log_verb("destroy conn %p", c);

    cc_free(c);

    *conn = NULL;
}

void
conn_pool_create(uint32_t max)
{
    if (!cp_init) {
        log_info("creating conn pool: max %"PRIu32, max);

        FREEPOOL_CREATE(&cp, max);
        cp_init = true;
    } else {
        log_warn("conn pool has already been created, ignore");
    }
}

void
conn_pool_destroy(void)
{
    struct conn *c, *tc;

    if (cp_init) {
        log_info("destroying conn pool: free %"PRIu32, cp.nfree);

        FREEPOOL_DESTROY(c, tc, &cp, next, conn_destroy);
        cp_init = false;
    } else {
        log_warn("conn pool was never created, ignore");
    }

}

struct conn *
conn_borrow(void)
{
    struct conn *c;

    FREEPOOL_BORROW(c, &cp, next, conn_create);

    if (c == NULL) {
        log_debug("borrow conn failed: OOM or over limit");
        return NULL;
    }

    conn_reset(c);

    log_verb("borrow conn %p", c);

    return c;
}

void
conn_return(struct conn **conn)
{
    struct conn *c = *conn;

    if (c == NULL || c->free) {
        return;
    }

    log_verb("return conn %p", c);

    c->free = true;
    FREEPOOL_RETURN(&cp, c, next);

    *conn = NULL;
}

bool
tcp_connect(struct addrinfo *ai, struct conn *c)
{
    int ret;

    ASSERT(c != NULL);

    c->sd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (c->sd < 0) {
	log_error("socket create for conn %p failed: %s", c, strerror(errno));

        goto error;
    }

    ret = tcp_set_tcpnodelay(c->sd);
    if (ret < 0) {
        log_error("set tcpnodelay on c %p sd %d failed: %s", c, c->sd,
                strerror(errno));

        goto error;
    }

    ret = connect(c->sd, ai->ai_addr, ai->ai_addrlen);
    if (ret < 0) {
        if (errno != EINPROGRESS) {
            log_error("connect on c %p sd %d failed: %s", c, c->sd,
                strerror(errno));

            goto error;
        }

        c->state = TCP_CONNECT;
        log_info("connecting on c %p sd %d", c, c->sd);
    } else {
        c->state = TCP_CONNECTED;
        log_info("connected on c %p sd %d", c, c->sd);
    }


    ret = tcp_set_nonblocking(c->sd);
    if (ret < 0) {
        log_error("set nonblock on c %p sd %d failed: %s", c, c->sd,
                strerror(errno));

        goto error;
    }

    return true;

error:
    c->err = errno;
    if (c->sd > 0) {
        close(c->sd);
    }

    return false;
}

bool
tcp_listen(struct addrinfo *ai, struct conn *c)
{
    int ret;
    ch_id_t sd;

    c->sd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (c->sd < 0) {
        log_error("socket failed: %s", strerror(errno));
        goto error;
    }

    sd = c->sd;

    ret = tcp_set_reuseaddr(sd);
    if (ret < 0) {
        log_error("reuse of sd %d failed: %s", sd, strerror(errno));
        goto error;
    }

    ret = bind(sd, ai->ai_addr, ai->ai_addrlen);
    if (ret < 0) {
        log_error("bind on sd %d failed: %s", sd, strerror(errno));
        goto error;
    }

    ret = listen(sd, max_backlog);
    if (ret < 0) {
        log_error("listen on sd %d failed: %s", sd, strerror(errno));
        goto error;
    }

    ret = tcp_set_nonblocking(sd);
    if (ret != CC_OK) {
        log_error("set nonblock on sd %d failed: %s", sd, strerror(errno));
        goto error;
    }

    c->level = CHANNEL_META;
    c->state = TCP_LISTEN;
    log_info("server listen setup on socket descriptor %d", c->sd);

    return true;

error:
    if (c->sd > 0) {
        tcp_close(c);
    }

    return false;
}

void
tcp_close(struct conn *c)
{
    if (c == NULL) {
        return;
    }

    log_info("closing conn %p sd %d", c, c->sd);

    if (c->sd >= 0) {
        close(c->sd);
    }
}

static inline int
_tcp_accept(struct conn *sc)
{
    int sd;

    ASSERT(sc->sd > 0);

    for (;;) { /* we accept at most one conn with the 'break' at the end */
        sd = accept(sc->sd, NULL, NULL);
        if (sd < 0) {
            if (errno == EINTR) {
                log_debug("accept on sd %d not ready: eintr",
                        sc->sd);
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                log_debug("accept on s %d not ready - eagain",
                        sc->sd);
                return -1;
            }

            return -1;
        }

        break;
    }

    return sd;
}

bool
tcp_accept(struct conn *sc, struct conn *c)
{
    int ret;
    int sd;

    sd = _tcp_accept(sc);
    if (sd < 0) {
        log_error("accept on s %d failed: %s", sc->sd, strerror(errno));

        return false;
    }

    c->sd = sd;
    c->level = CHANNEL_BASE;
    c->state = TCP_CONNECTED;

    ret = tcp_set_nonblocking(sd);
    if (ret < 0) {
        log_warn("set nonblock on c %d failed, ignored: %s", sd,
                strerror(errno));
    }

    ret = tcp_set_tcpnodelay(sd);
    if (ret < 0) {
        log_warn("set tcp nodelay on c %d failed, ignored: %s", sd,
                 strerror(errno));
    }

    log_info("accepted c %d on sd %d", c->sd, sc->sd);

    return true;
}

void
tcp_reject(struct conn *sc)
{
    int ret;
    int sd;

    sd = _tcp_accept(sc);
    if (sd < 0) {
        return;
    }

    ret = close(sd);
    if (ret < 0) {
        log_error("close c %d failed, ignored: %s", sd, strerror(errno));
    }
}

int
tcp_set_blocking(int sd)
{
    int flags;

    flags = fcntl(sd, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }

    return fcntl(sd, F_SETFL, flags & ~O_NONBLOCK);
}

int
tcp_set_nonblocking(int sd)
{
    int flags;

    flags = fcntl(sd, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }

    return fcntl(sd, F_SETFL, flags | O_NONBLOCK);
}

int
tcp_set_reuseaddr(int sd)
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
int
tcp_set_tcpnodelay(int sd)
{
    int nodelay;
    socklen_t len;

    nodelay = 1;
    len = sizeof(nodelay);

    return setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &nodelay, len);
}

int
tcp_set_keepalive(int sd)
{
    int keepalive;
    socklen_t len;

    keepalive = 1;
    len = sizeof(keepalive);

    return setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, len);
}

int
tcp_set_linger(int sd, int timeout)
{
    struct linger linger;
    socklen_t len;

    linger.l_onoff = 1;
    linger.l_linger = timeout;

    len = sizeof(linger);

    return setsockopt(sd, SOL_SOCKET, SO_LINGER, &linger, len);
}

int
tcp_unset_linger(int sd)
{
    struct linger linger;
    socklen_t len;

    linger.l_onoff = 0;
    linger.l_linger = 0;

    len = sizeof(linger);

    return setsockopt(sd, SOL_SOCKET, SO_LINGER, &linger, len);
}

int
tcp_set_sndbuf(int sd, int size)
{
    socklen_t len;

    len = sizeof(size);

    return setsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, len);
}

int
tcp_set_rcvbuf(int sd, int size)
{
    socklen_t len;

    len = sizeof(size);

    return setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, len);
}

int
tcp_get_sndbuf(int sd)
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

int
tcp_get_rcvbuf(int sd)
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

void
tcp_maximize_sndbuf(int sd)
{
    int status, min, max, avg;

    /* start with the default size */
    min = tcp_get_sndbuf(sd);
    if (min < 0) {
        return;
    }

    /* binary-search for the real maximum */
    max = 256 * MiB;

    while (min <= max) {
        avg = (min + max) / 2;
        status = tcp_set_sndbuf(sd, avg);
        if (status != 0) {
            max = avg - 1;
        } else {
            min = avg + 1;
        }
    }
}

int
tcp_get_soerror(int sd)
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


/*
 * try reading nbyte bytes from conn and place the data in buf
 * EINTR is continued, EAGAIN is explicitly flagged in return, other errors are
 * returned as a generic error/failure.
 */
ssize_t
tcp_recv(struct conn *c, void *buf, size_t nbyte)
{
    ssize_t n;

    ASSERT(buf != NULL);
    ASSERT(nbyte > 0);

    log_verb("recv on sd %d, total %zu bytes", c->sd, nbyte);

    for (;;) {
        n = read(c->sd, buf, nbyte);

        log_verb("read on sd %d %zd of %zu", c->sd, n, nbyte);

        if (n > 0) {
            c->recv_nbyte += (size_t)n;
            return n;
        }

        if (n == 0) {
            c->state = TCP_EOF;
            log_info("recv on sd %d eof rb  %zu sb %zu", c->sd,
                      c->recv_nbyte, c->send_nbyte);
            return n;
        }

        /* n < 0 */
        if (errno == EINTR) {
            log_verb("recv on sd %d not ready - EINTR", c->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            log_verb("recv on sd %d not ready - EAGAIN", c->sd);
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
 * vector version of tcp_recv, using readv to read into a mbuf array
 */
ssize_t
tcp_recvv(struct conn *c, struct array *bufv, size_t nbyte)
{
    /* TODO(yao): this is almost identical with tcp_recv except for the call
     * to readv. Consolidate the two?
     */
    ssize_t n;

    ASSERT(array_nelem(bufv) > 0);
    ASSERT(nbyte != 0);

    log_verb("recvv on sd %d, total %zu bytes", c->sd, nbyte);

    for (;;) {
        n = readv(c->sd, (const struct iovec *)bufv->data, bufv->nelem);

        log_verb("recvv on sd %d %zd of %zu in %"PRIu32" buffers",
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
            log_verb("recvv on sd %d not ready - eintr", c->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {

            log_verb("recvv on sd %d not ready - eagain", c->sd);
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
tcp_send(struct conn *c, void *buf, size_t nbyte)
{
    ssize_t n;

    ASSERT(buf != NULL);
    ASSERT(nbyte > 0);

    log_verb("send on sd %d, total %zu bytes", c->sd, nbyte);

    for (;;) {
        n = write(c->sd, buf, nbyte);

        log_verb("write on sd %d %zd of %zu", c->sd, n, nbyte);

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
            log_verb("send on sd %d not ready - EINTR", c->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            log_verb("send on sd %d not ready - EAGAIN", c->sd);
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
 * vector version of tcp_send, using writev to send an array of bufs
 */
ssize_t
tcp_sendv(struct conn *c, struct array *bufv, size_t nbyte)
{
    /* TODO(yao): this is almost identical with tcp_send except for the call
     * to writev. Consolidate the two? Revisit these functions when we build
     * more concrete backend systems.
     */
    ssize_t n;

    ASSERT(array_nelem(bufv) > 0);
    ASSERT(nbyte != 0);

    log_verb("sendv on sd %d, total %zu bytes", c->sd, nbyte);

    for (;;) {
        n = writev(c->sd, (const struct iovec *)bufv->data, bufv->nelem);

        log_verb("sendv on sd %d %zd of %zu in %"PRIu32" buffers",
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
            log_verb("sendv on sd %d not ready - eintr", c->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            log_verb("sendv on sd %d not ready - eagain", c->sd);
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
