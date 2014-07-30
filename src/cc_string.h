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

#ifndef _CC_STRING_H_
#define _CC_STRING_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cc_define.h>

struct string {
    uint32_t len;   /* string length */
    uint8_t  *data; /* string data */
};

#define string(_str)   { sizeof(_str) - 1, (uint8_t *)(_str) }
#define null_string    { 0, NULL }

#define string_set_text(_str, _text) do {       \
    (_str)->len = (uint32_t)(sizeof(_text) - 1);\
    (_str)->data = (uint8_t *)(_text);          \
} while (0);

#define string_set_raw(_str, _raw) do {         \
    (_str)->len = (uint32_t)(cc_strlen(_raw));  \
    (_str)->data = (uint8_t *)(_raw);           \
} while (0);

void string_init(struct string *str);
void string_deinit(struct string *str);
bool string_empty(const struct string *str);
rstatus_t string_duplicate(struct string *dst, const struct string *src);
rstatus_t string_copy(struct string *dst, const uint8_t *src, uint32_t srclen);
int string_compare(const struct string *s1, const struct string *s2);

/*
 * Wrapper around common routines for manipulating C character strings
 *
 * cc_memcmp
 * cc_memcpy
 * cc_memmove
 * cc_memchr
 * cc_memset
 *
 * cc_strlen
 * cc_strncmp
 * cc_strndup
 *
 * cc_strchr
 * cc_strrchr
 *
 * cc_snprintf
 * cc_scnprintf
 * cc_vscnprintf
 */
#define cc_memcmp(_p1, _p2, _n)                                 \
    memcmp(_p1, _p2, (size_t)(_n))

#define cc_memcpy(_d, _c, _n)                                   \
    memcpy(_d, _c, (size_t)(_n))

#define cc_memmove(_d, _c, _n)                                  \
    memmove(_d, _c, (size_t)(_n))

#define cc_memchr(_d, _c, _n)                                   \
    memchr(_d, _c, (size_t)(_n))

#define cc_memset(_p, _v, _n)                                   \
    memset(_p, _v, (size_t)(_n))

#define cc_strlen(_s)                                           \
    strlen((char *)(_s))

#define cc_strncmp(_s1, _s2, _n)                                \
    strncmp((char *)(_s1), (char *)(_s2), (size_t)(_n))

#define cc_strndup(_s, _n)                                      \
    (uint8_t *)strndup((char *)(_s), (size_t)(_n));

#define cc_strchr(_p, _l, _c)                                   \
    _cc_strchr((uint8_t *)(_p), (uint8_t *)(_l), (uint8_t)(_c))

#define cc_strrchr(_p, _s, _c)                                  \
    _cc_strrchr((uint8_t *)(_p),(uint8_t *)(_s), (uint8_t)(_c))

#define cc_snprintf(_s, _n, ...)                                \
    snprintf((char *)(_s), (size_t)(_n), __VA_ARGS__)

#define cc_scnprintf(_s, _n, ...)                               \
    _scnprintf((char *)(_s), (size_t)(_n), __VA_ARGS__)

#define cc_vscnprintf(_s, _n, _f, _a)                           \
    _vscnprintf((char *)(_s), (size_t)(_n), _f, _a)

static inline uint8_t *
_cc_strchr(uint8_t *p, uint8_t *last, uint8_t c)
{
    while (p < last) {
        if (*p == c) {
            return p;
        }
        p++;
    }

    return NULL;
}

static inline uint8_t *
_cc_strrchr(uint8_t *p, uint8_t *start, uint8_t c)
{
    while (p >= start) {
        if (*p == c) {
            return p;
        }
        p--;
    }

    return NULL;
}

int _scnprintf(char *buf, size_t size, const char *fmt, ...);
int _vscnprintf(char *buf, size_t size, const char *fmt, va_list args);

#endif
