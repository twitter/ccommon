/*
 * ccommon - a cache common library.
 * Copyright (C) 2015 Twitter, Inc.
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

#ifndef __CC_PIPE_H_
#define __CC_PIPE_H_

#include <cc_debug.h>
#include <channel/cc_channel.h>

#include <stdbool.h>
#include <unistd.h>

/**
 * This implements the channel interface for pipes
 */

#define PIPE_POOLSIZE 1         /* Currently our applications only use 1 pipe conn */

#define PIPE_OPTION(ACTION) \
    ACTION( pipe_poolsize, OPTION_TYPE_UINT, str(PIPE_POOLSIZE), "pipe conn pool size" )

/* pipe_conn states */
#define PIPE_CLOSED  0
#define PIPE_OPEN    1
#define PIPE_EOF     2

struct pipe_conn {
    STAILQ_ENTRY(pipe_conn) next;       /* for pool */
    bool                    free;       /* in use? */

    int                     fd[2];      /* file descriptors */

    size_t                  recv_nbyte; /* # bytes read */
    size_t                  send_nbyte; /* # bytes written */

    unsigned                state:4;    /* defined as above */
    unsigned                flags;      /* annotation fields */

    err_t                   err;        /* errno */
};

/* functions for managing pipe connection structs */

/* creation/destruction */
struct pipe_conn *pipe_conn_create(void);
void pipe_conn_destroy(struct pipe_conn **c);

/* initialize a pipe_conn struct for use */
void pipe_conn_reset(struct pipe_conn *c);

/* pool functions */
void pipe_conn_pool_create(uint32_t max);
void pipe_conn_pool_destroy(void);
struct pipe_conn *pipe_conn_borrow(void);
void pipe_conn_return(struct pipe_conn **c);

/* functions for using pipe connections */

/* open/close pipe */
bool pipe_open(void *addr, struct pipe_conn *c);
void pipe_close(struct pipe_conn *c);

/* send/recv on pipe */
ssize_t pipe_recv(struct pipe_conn *c, void *buf, size_t nbyte);
ssize_t pipe_send(struct pipe_conn *c, void *buf, size_t nbyte);

/* file descriptor access */
static inline ch_id_t pipe_conn_id(struct pipe_conn *c)
{
    return c->fd;
}

static inline int pipe_read_fd(struct pipe_conn *c)
{
    ASSERT(c != NULL);
    return c->fd[0];
}

static inline int pipe_write_fd(struct pipe_conn *c)
{
    ASSERT(c != NULL);
    return c->fd[1];
}

/* functions for getting/setting pipe read flags */
int pipe_rset_blocking(struct pipe_conn *c);
int pipe_rset_nonblocking(struct pipe_conn *c);

/* functions for getting/setting pipe write flags */
int pipe_wset_blocking(struct pipe_conn *c);
int pipe_wset_nonblocking(struct pipe_conn *c);

#endif /* __CC_PIPE_H_ */
