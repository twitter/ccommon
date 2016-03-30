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
#include <cc_util.h>

#include <stdbool.h>


/*          name            type              default           description */
#define DBUF_OPTION(ACTION)                                                                   \
    ACTION( dbuf_max_power, OPTION_TYPE_UINT, DBUF_DEFAULT_MAX, "max # times buf is doubled" )

typedef struct {
    DBUF_OPTION(OPTION_DECLARE)
} dbuf_options_st;

#define DBUF_DEFAULT_MAX 8  /* 16KiB default size gives us 4 MiB max */

/* Setup/teardown doubling buffer module */
void dbuf_setup(dbuf_options_st *options);
void dbuf_teardown(void);

/* Buffer resizing functions */
rstatus_i dbuf_double(struct buf **buf); /* 2x size, slightly >2x capacity */
rstatus_i dbuf_shrink(struct buf **buf); /* reset to initial size */
rstatus_i dbuf_fit(struct buf **buf, uint32_t cap); /* resize to fit cap */

#ifdef __cplusplus
}
#endif
