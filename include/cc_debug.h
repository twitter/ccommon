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

#include <cc_define.h>

/*
 * Wrappers for defining custom assert based on whether macro
 * CC_ASSERT_PANIC or CC_ASSERT_LOG was defined at the moment
 * ASSERT was called.
 */
#if defined CC_ASSERT_PANIC && CC_ASSERT_PANIC == 1 /* log and panic */

#define ASSERT(_x) do {                             \
    if (!(_x)) {                                    \
        debug_assert(#_x, __FILE__, __LINE__, 1);  \
    }                                               \
} while (0)

#define NOT_REACHED() ASSERT(0)

#elif defined CC_ASSERT_LOG && CC_ASSERT_LOG == 1 /* just log */

#define ASSERT(_x) do {                             \
    if (!(_x)) {                                    \
        debug_assert(#_x, __FILE__, __LINE__, 0);  \
    }                                               \
} while (0)

#define NOT_REACHED() ASSERT(0)

#else /* ignore all asserts */

#define ASSERT(_x)

#define NOT_REACHED()

#endif

void debug_assert(const char *cond, const char *file, int line, int panic);
void debug_stacktrace(int skip_count);

#ifdef __cplusplus
}
#endif
