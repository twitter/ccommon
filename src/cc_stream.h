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

#ifndef __CC_STREAM_H_
#define __CC_STREAM_H_

/* Note(yao): a stream supports serialized read/write of data, potentially over
 * a number of media: network sockets, file, shared memory, etc. While we have
 * not implemented all the underlying I/O mechanisms for each medium, we can
 * build an abstraction to provide a unified interface on top of them.
 *
 * From a service's perspective it needs a few essential parts: First, there
 * has to be channels over which requests/data arrive in order, such channels
 * can be TCP connections, Unix Domain Socket file desriptors, a consecutive
 * area in memory, among others. Second, there needs to be a destination and an
 * accompanying format at which the arrived data can be read into. This could
 * be as simple as a memory area, or something like the msg data structures
 * implemented in the ccommon library. Finally, given the protocols we will
 * need to support, there should be at least two ways to specify how much data
 * should/can be read from the channels: by length or by delimiter. A protocol
 * that relies on fixed-length fields or fields whose length are already known,
 * such as Redis, can use the former; protocols such as Memcached ASCII relies
 * on delimiters (and sometimes, size), so both are required to support them.
 *
 * To make the channel IO work with the rest of the service, we use callbacks.
 * Upon receiving a message or some data, a pre-defined routine is called; same
 * goes for sending messages/data.
 *
 * The idea described here is not dissimilar to the use of channels/streams
 * in Plan 9.
 */

#include <cc_string.h>

typedef enum channel_type {
    CHANNEL_UNKNOWN,
    CHANNEL_TCP
} channel_type_t;

typedef void * channel_t;

struct stream;

typedef void (*msg_handler_t)(struct stream *stream, size_t nbyte);

typedef struct stream_handler {
    msg_handler_t pre_recv;  /* callback before msg received */
    msg_handler_t post_recv; /* callback after msg received */
    msg_handler_t pre_send;  /* callback before msg sent */
    msg_handler_t post_send; /* callback after msg sent */
} stream_handler_t;

/* Note(yao): should we use function pointers for the underlying actions and
 * initialized them with the proper channel-specific version when we create the
 * stream? (FYI: This style is used by nanomsg.)
 *
 * we can also support using vector read and write, especially write, but that
 * doesn't necessarily require a vector-ed write buffer (only the iov needs to
 * know where each block of memory starts and ends). For write, the assmebly
 * of that arry should happen outside of the stream module.
 */
struct stream {
    void               *owner;     /* owner of the stream */

    channel_type_t     type;       /* type of the communication channels */
    channel_t          channel;    /* underlying bi-directional channels */

    struct mbuf        *rbuf;      /* read buffer */
    struct mbuf        *wbuf;      /* write buffer */
    stream_handler_t   *handler;   /* stream handlers */
};

/* channel/medium agnostic data IO */
rstatus_t msg_recv(struct stream *stream, size_t nbyte);
rstatus_t msg_send(struct stream *stream, size_t nbyte);

/* NOTE(yao): a yield mechanism for the caller to postpone I/O to the future,
 * especially recv. This can be used to avoid starvation and improve fairness.
 */

/* NOTE(yao): we choose not to implement receiving and sending from raw bufs
 * for now, so everything has to be copied into stream's message buffer first,
 * the saving from doing zero-copy can be significant for large payloads, so
 * it will needs to be supported before we use unified backend for those cases.
 * Another set of useful APIs are those that read past a certain delimiter
 * before returning, but that can wait, too.
 * We may want to add the scatter-gather style write, i.e. sendmsg() alike,
 * very soon*/

#endif
