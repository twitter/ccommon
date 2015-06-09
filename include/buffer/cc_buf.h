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

/*
 * buf: a buffer base for contiguous buffers that can be pooled together
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <cc_bstring.h>
#include <cc_define.h>
#include <cc_log.h>
#include <cc_queue.h>
#include <cc_util.h>

#include <stdbool.h>

#define BUF_OPTION(ACTION) \
    ACTION( buf_size,     OPTION_TYPE_UINT,  str(BUF_SIZE),     "buf default size (how large it is when initialized)" ) \
    ACTION( buf_poolsize, OPTION_TYPE_UINT,  str(BUF_POOLSIZE), "buf pool size"                                       )

struct buf {
    STAILQ_ENTRY(buf) next;     /* next buf in pool */
    uint8_t           *rpos;    /* read marker */
    uint8_t           *wpos;    /* write marker */
    uint8_t           *end;     /* end of buffer */
    uint8_t           free;     /* is this buf free? */
    uint8_t           begin[1]; /* beginning of buffer */
};

STAILQ_HEAD(buf_q, buf);

extern uint32_t buf_size;

#define BUF_SIZE      (16 * KiB)
#define BUF_POOLSIZE  0 /* unlimited */

#define BUF_EMTPY(BUF) \
    ((BUF)->rpos == (BUF)->wpos)

#define BUF_FULL(BUF) \
    ((BUF)->wpos == (BUF)->end)

/* Setup/teardown buf module */
void buf_setup(uint32_t size);
void buf_teardown(void);

/* Create/destroy buffer pool */
void buf_pool_create(uint32_t max);
void buf_pool_destroy(void);

/* Obtain/return a buffer from the pool */
struct buf *buf_borrow(void);
void buf_return(struct buf **buf);

/* Discard any read/write data */
void buf_reset(struct buf *buf);

/* Create/destroy a buffer (allocate/deallocate) */
struct buf *buf_create(void);
void buf_destroy(struct buf **buf);

/* Size of data that has yet to be read */
uint32_t buf_rsize(struct buf *buf);

/* Amount of room left in buffer for writing new data */
uint32_t buf_wsize(struct buf *buf);

/* Total capacity of given fbuf */
uint32_t buf_capacity(struct buf *buf);

/* Shift rpos, wpos, and content to the beginning/end */
void buf_lshift(struct buf *buf);
void buf_rshift(struct buf *buf);

/* Read from buffer. Returns number of bytes read. Count indicates the size of
   the destination memory area (max number of bytes to read) */
uint32_t buf_read(uint8_t *dst, uint32_t count, struct buf *buf);

/* Write to the buffer. */
uint32_t buf_write(uint8_t *src, uint32_t count, struct buf *buf);

/* Read/write for bstrings */
uint32_t buf_read_bstring(struct buf *buf, struct bstring *bstr);
uint32_t buf_write_bstring(struct buf *buf, const struct bstring *bstr);

#ifdef __cplusplus
}
#endif
