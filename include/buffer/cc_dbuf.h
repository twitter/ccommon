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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <buffer/cc_buf.h>
#include <cc_define.h>
#include <cc_log.h>
#include <cc_util.h>

#include <stdbool.h>

#define DBUF_OPTION(ACTION) \
    ACTION( dbuf_max_size,      OPTION_TYPE_UINT,   str(DBUF_DEFAULT_MAX_SIZE), "max buffer size"                  ) \
    ACTION( dbuf_shrink_factor, OPTION_TYPE_UINT,   str(DBUF_DEFAULT_SHRINK),   "fill factor for shrinking buffer" )

#define DBUF_DEFAULT_MAX_SIZE       MiB
#define DBUF_DEFAULT_SHRINK         4

/* Setup/teardown doubling buffer module */
void dbuf_setup(uint32_t max_size, uint32_t shrink_factor);
void dbuf_teardown(void);

/* Return buf to pool. You MUST use this version (not buf_return) if you used
   any of the dbuf functions with this buffer! */
void dbuf_return(struct buf **buf);

/* Buffer resizing functions */
rstatus_t dbuf_resize(struct buf *buf, uint32_t new_size);
rstatus_t dbuf_fit(struct buf *buf, uint32_t count);
rstatus_t dbuf_double(struct buf *buf);

/* Read from doubling buffer. Returns # bytes read */
uint32_t dbuf_read(uint8_t *dst, uint32_t count, struct buf *buf);

/* Write to doubling buffer. Returns # bytes written */
uint32_t dbuf_write(uint8_t *src, uint32_t count, struct buf *buf);

/* Read/write with bstring */
uint32_t dbuf_read_bstring(struct buf *buf, struct bstring *bstr);
uint32_t dbuf_write_bstring(struct buf *buf, const struct bstring *bstr);

#ifdef __cplusplus
}
#endif
