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

#include <cc_print.h>

/*
 * Note: the impelmentation of cc_print_uint64_unsafe uses Facebook/folly's
 * implementation as a reference (folly/Conv.h)
 */
static const uint64_t BASE10[CC_UINT64_MAXLEN - 1] = {
    0, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000,
    10000000000, 100000000000, 1000000000000, 10000000000000, 100000000000000,
    1000000000000000, 10000000000000000, 100000000000000000,
    1000000000000000000, 10000000000000000000ul};

static inline size_t
_digits(int n) {
    size_t d = 1;

    while (n >= BASE10[d]) {
        d++;
    }

    return d;
}

static inline void
_print_uint64(char *buf, size_t d, uint64_t n)
{
    char *p;

    p = buf + d;
    do {
        p--;
        *p = '0' + n % 10;
        n = n / 10;
    } while (n > 0);
}

size_t
cc_print_uint64_unsafe(char *buf, uint64_t n)
{
    size_t d;

    d = _digits(n);
    _print_uint64(buf, d, n);

    return d;
}

size_t
cc_print_uint64(char *buf, size_t size, uint64_t n)
{
    size_t d;

    d = _digits(n);
    if (size < d) {
        return 0;
    }

    _print_uint64(buf, d, n);

    return d;
}

size_t
_vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    int i;

    i = vsnprintf(buf, size, fmt, args);

    /*
     * The return value is the number of characters which would be written
     * into buf not including the trailing '\0'. If size is == 0 the
     * function returns 0.
     *
     * On error, the function also returns 0. This is to allow idiom such
     * as len += _vscnprintf(...)
     *
     * See: http://lwn.net/Articles/69419/
     */
    if (i <= 0) {
        return 0;
    }

    if (i < size) {
        return i;
    }

    return size - 1;
}

size_t
_scnprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i = _vscnprintf(buf, size, fmt, args);
    va_end(args);

    return i;
}

