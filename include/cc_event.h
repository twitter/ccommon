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

#ifndef _CC_EVENT_H_
#define _CC_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <cc_define.h>

#include <inttypes.h>

#define EVENT_SIZE  1024

#define EVENT_READ  0x0000ff
#define EVENT_WRITE 0x00ff00
#define EVENT_ERR   0xff0000

typedef void (*event_cb_t)(void *, uint32_t);  /* event callback */

#ifdef CC_HAVE_EPOLL

struct event_base {
    int                ep;      /* epoll descriptor */

    struct epoll_event *event;  /* event[] - events that were triggered */
    int                nevent;  /* # events */

    event_cb_t         cb;      /* event callback */
};

#elif CC_HAVE_KQUEUE

struct event_base {
    int           kq;           /* kernel event queue descriptor */

    struct kevent *change;      /* change[] - events we want to monitor */
    int           nchange;      /* # change */

    struct kevent *event;       /* event[] - events that were triggered */
    int           nevent;       /* # events */
    int           nreturned;    /* # events placed in event[] */
    int           nprocessed;   /* # events processed from event[] */

    event_cb_t    cb;           /* event callback */
};

#else
# error missing scalable I/O event notification mechanism
#endif

/* event base */
struct event_base *event_base_create(int size, event_cb_t cb);
void event_base_destroy(struct event_base **evb);

/* event control */
int event_add_read(struct event_base *evb, int fd, void *data);
int event_add_write(struct event_base *evb, int fd, void *data);
int event_register(struct event_base *evb, int fd, void *data);
int event_deregister(struct event_base *evb, int fd);

/* event wait */
int event_wait(struct event_base *evb, int timeout);

#ifdef __cplusplus
}
#endif

#endif /* _CC_EVENT_H */
