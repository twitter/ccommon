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

#include <cc_debug.h>

#include <cc_log.h>

#include <ctype.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

#define BACKTRACE_DEPTH 64

void
debug_assert(const char *cond, const char *file, int line, int panic)
{

    loga("assert '%s' failed @ (%s, %d)", cond, file, line);
    if (panic) {
        debug_stacktrace(1);
        abort();
    }
}

void
debug_stacktrace(int skip_count)
{
#ifdef CC_BACKTRACE
    void *stack[BACKTRACE_DEPTH];
    char **symbols;
    int size, i, j;

    loga("printing stracktrace (depth limit: %d)", BACKTRACE_DEPTH);
    size = backtrace(stack, BACKTRACE_DEPTH);
    symbols = backtrace_symbols(stack, size);
    if (symbols == NULL) {
        return;
    }

    skip_count++; /* skip the current frame also */

    for (i = skip_count, j = 0; i < size; i++, j++) {
        loga("[%d] %s", j, symbols[i]);
    }

    free(symbols);
#endif
}
