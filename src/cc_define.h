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

#ifndef __CC_DEFINE_H__
#define __CC_DEFINE_H__

#include <config.h>

#ifdef HAVE_LITTLE_ENDIAN
# define CC_LITTLE_ENDIAN 1
#elif HAVE_BIG_ENDIAN
# define CC_BIG_ENDIAN 1
#endif

#ifdef HAVE_DEBUG_LOG
# define CC_DEBUG_LOG 1
#endif

#ifdef HAVE_ASSERT_PANIC
# define CC_ASSERT_PANIC 1
#endif

#ifdef HAVE_ASSERT_LOG
# define CC_ASSERT_LOG 1
#endif

#ifdef HAVE_EPOLL
# define CC_HAVE_EPOLL 1
#elif HAVE_KQUEUE
# define CC_HAVE_KQUEUE 1
#endif

#ifdef HAVE_BACKTRACE
# define CC_HAVE_BACKTRACE 1
#endif

/* TODO: add compile time option to turn chaining on/off */
/*#ifdef HAVE_CHAINED*/
# define CC_HAVE_CHAINED 1
/*#endif*/

#define CC_OK        0
#define CC_ERROR    -1

#define CC_EAGAIN   -2
#define CC_ERETRY   -3

#define CC_ENOMEM   -4
#define CC_EEMPTY   -5 /* no data */

#define CC_UNFIN    1  /* unfinished, more data expected */

typedef int rstatus_t;  /* generic function return value type */
typedef int err_t; /* erroneous values for rstatus_t */

#endif
