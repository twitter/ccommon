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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <cc_debug.h>
#include <cc_log.h>
#include <cc_util.h>

#include <cc_nio.h>

void
conn_init(void)
{
    log_debug(LOG_VERB, "initialize connection");
    log_debug(LOG_DEBUG, "conn size %zu", sizeof(struct conn));
}

void
conn_deinit(void)
{
    log_debug(LOG_DEBUG, "conn size %zu", sizeof(struct conn));
}

/**
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

    for (;;) {
        n = cc_read(conn->sd, buf, nbyte);

        log_debug(LOG_VERB, "recv on sd %d %zd of %zu", conn->sd, n, nbyte);

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
            log_debug(LOG_INFO, "recv on sd %d eof rb %zu sb %zu", conn->sd,
                      conn->recv_bytes, conn->send_bytes);
            return n;
        }

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
