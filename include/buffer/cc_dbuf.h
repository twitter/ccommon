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

#ifndef _CC_DBUF_H_
#define _CC_DBUF_H_

#include <cc_define.h>
#include <cc_log.h>
#include <cc_util.h>

#include <stdbool.h>

#define DBUF_OPTION(ACTION) \
    ACTION( buffer_init_size,     OPTION_TYPE_UINT,   str(DBUF_DEFAULT_SIZE),     "initial/minimum buffer size"      ) \
    ACTION( buffer_max_size,      OPTION_TYPE_UINT,   str(DBUF_DEFAULT_MAX_SIZE), "max buffer size"                  ) \
    ACTION( buffer_shrink_factor, OPTION_TYPE_UINT,   str(DBUF_DEFAULT_SHRINK),   "fill factor for shrinking buffer" ) \

struct dbuf {
    uint8_t *begin;
    uint8_t *end;
    uint8_t *rpos;
    uint8_t *wpos;
};

#define DBUF_DEFAULT_SIZE           (16 * KiB)
#define DBUF_DEFAULT_MAX_SIZE       (MiB)
#define DBUF_DEFAULT_SHRINK         4

static inline bool
dbuf_empty(const struct dbuf *buf)
{
    return buf->rpos == buf->wpos;
}

static inline bool
dbuf_full(const struct dbuf *buf)
{
    return buf->wpos == buf->end;
}

void dbuf_setup(uint32_t size, uint32_t max_size);
void dbuf_teardown(void);

struct dbuf *dbuf_create(void);
void dbuf_destroy(struct dbuf *buf);

/* Shift unread bytes to the beginning of buf */
void dbuf_shift(struct dbuf *buf);

rstatus_t dbuf_resize(struct dbuf *buf, uint32_t new_size);
rstatus_t dbuf_fit(struct dbuf *buf, uint32_t count);

uint32_t dbuf_read(uint8_t *dst, uint32_t count, struct dbuf *buf);
rstatus_t dbuf_write(uint8_t *src, uint32_t count, struct dbuf *buf);

#endif /* _CC_DBUF_H_ */
