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

#include <sys/uio.h>
#include <limits.h>

#include <cc_debug.h>
#include <cc_log.h>
#include <cc_mbuf.h>
#include <cc_nio.h>
#include <cc_util.h>

#include <cc_stream.h>

#if (IOV_MAX > 128)
#define CC_IOV_MAX 128
#else
#define CC_IOV_MAX IOV_MAX
#endif

/* recv nbyte at most and store it in rbuf associated with the stream.
 * Uses at most the next CC_IOV_MAX segments/mbufs. Stream buffer must provide
 * enough capacity, otherwise CC_NOBUF is returned.
 * callback pre_recv, if not NULL, is called before receiving data
 * callback pos_recv, if not NULL, is called after receiving data
 */
rstatus_t
msg_recv(struct stream *stream, size_t nbyte)
{
    stream_handler_t *handler;
    rstatus_t status;
    struct array recvv;
    struct iovec *ciov, iov[CC_IOV_MAX];
    struct mbuf *mbuf, *nbuf;
    struct mq *rbuf;
    size_t capacity;
    ssize_t n = 0; /* bytes actually received */

    ASSERT(stream != NULL);
    ASSERT(stream->iobuf != NULL);
    ASSERT(stream->iobuf->rbuf != NULL);
    ASSERT(nbyte != 0 && nbyte <= SSIZE_MAX);

    rbuf = stream->iobuf->rbuf;
    handler = stream->handler;

    /* call pre_recv (if not NULL), abort if return is abnormal */
    if (handler->pre_recv != NULL) {
        status = handler->pre_recv(stream, nbyte);
        if (status != CC_OK) {
            log_debug(LOG_VERB, "pre_recv callback returned error, aborting");
            goto done;
        }
    }

    /* build the iov array from rbuf, include enough mbufs to fit nbyte  */
    array_data_assign(&recvv, CC_IOV_MAX, sizeof(iov[0]), iov);
    for (mbuf = STAILQ_FIRST(rbuf), capacity = 0;
         mbuf != NULL && capacity < nbyte && array_nelem(&recvv) < CC_IOV_MAX;
         mbuf = nbuf) {
        nbuf = STAILQ_NEXT(mbuf, next);

        if (mbuf_full(mbuf)) {
            continue;
        }

        ciov = array_push(&recvv);
        ciov->iov_base = mbuf->wpos;
        ciov->iov_len = MIN(mbuf_wsize(mbuf), nbyte - capacity);

        capacity += ciov->iov_len;
    }

    if (capacity != nbyte) {
        log_debug(LOG_VERB, "not enough capacity in rbuf at %p: nbyte %zu, "
                "capacity %zu, array nelem %"PRIu32, rbuf, nbyte, capacity,
                array_nelem(&recvv));

        status = CC_ENOBUF;
        goto done;
    }

    switch (stream->type) {
    case CHANNEL_TCP:   /* TCP socket */
        n = conn_recvv(stream->channel, &recvv, nbyte);
        if (n < 0) {
            if (n == CC_EAGAIN) {
                status = CC_OK;
            } else {
                status = CC_ERROR;
            }
            goto done;
        }
        break;
    case CHANNEL_UNKNOWN:
        log_error("stream channel type unknown");
        status = CC_ERROR;
        break;

    default:
        NOT_REACHED();
        status = CC_ERROR;
    }

    log_debug(LOG_VERB, "recv %zd bytes on stream stream type %d", n, stream->type);

    status = handler->pos_recv(stream, (size_t)n);

done:
    /* NOTE(yao): do we need to create another read event in case of error? */
    return status;
}


/* send nbyte at most from data stored in wbuf associated with the stream.
 * Uses at most the next CC_IOV_MAX segments/mbufs.
 * callback pre_send, if not NULL, is called before sending data
 * callback pos_send, if not NULL, is called after sending data
 */
rstatus_t msg_send(struct stream *stream, size_t nbyte)
{
    stream_handler_t *handler;
    rstatus_t status;
    struct array sendv;
    struct iovec *ciov, iov[CC_IOV_MAX];
    struct mbuf *mbuf, *nbuf;
    struct mq *wbuf;
    size_t content;
    ssize_t n = 0; /* bytes actually received */

    ASSERT(stream != NULL);
    ASSERT(stream->iobuf != NULL);
    ASSERT(stream->iobuf->wbuf != NULL);
    ASSERT(nbyte != 0 && nbyte <= SSIZE_MAX);

    wbuf = stream->iobuf->wbuf;
    handler = stream->handler;

    /* call pre_recv (if not NULL), abort if return is abnormal */
    if (handler->pre_send != NULL) {
        status = handler->pre_send(stream, nbyte);
        if (status != CC_OK) {
            log_debug(LOG_VERB, "pre_recv callback returned error, aborting");
            goto done;
        }
    }

    /* build the iov array from wbuf, include data available up to nbyte */
    array_data_assign(&sendv, CC_IOV_MAX, sizeof(iov[0]), iov);
    for (mbuf = STAILQ_FIRST(wbuf), content = 0;
         mbuf != NULL && content < nbyte && array_nelem(&sendv) < CC_IOV_MAX;
         mbuf = nbuf) {
        nbuf = STAILQ_NEXT(mbuf, next);

        if (mbuf_empty(mbuf)) {
            continue;
        }

        ciov = array_push(&sendv);
        ciov->iov_base = mbuf->rpos;
        ciov->iov_len = MIN(mbuf_rsize(mbuf), nbyte - content);

        content += ciov->iov_len;
    }

    switch (stream->type) {
    case CHANNEL_TCP:   /* TCP socket */
        n = conn_sendv(stream->channel, &sendv, nbyte);
        if (n < 0) {
            if (n == CC_EAGAIN) {
                status = CC_OK;
            } else {
                status = CC_ERROR;
            }
            goto done;
        }
        break;
    case CHANNEL_UNKNOWN:
        log_error("stream channel type unknown");
        status = CC_ERROR;
        break;

    default:
        NOT_REACHED();
        status = CC_ERROR;
    }

    log_debug(LOG_VERB, "recv %zd bytes on stream stream type %d", n, stream->type);

    status = handler->pos_send(stream, (size_t)n);

done:
    /* NOTE(yao): do we need to create another write event in case of error? */
    return status;
}
