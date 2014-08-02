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
#include <stdlib.h>
#include <string.h>

#include <cc_define.h>

struct bstring {
    uint32_t len;   /* string length */
    uint8_t  *data; /* string data */
};

#define bstring(_str)   { sizeof(_str) - 1, (uint8_t *)(_str) }
#define null_string    { 0, NULL }

#define bstring_set_text(_str, _text) do {       \
    (_str)->len = (uint32_t)(sizeof(_text) - 1);\
    (_str)->data = (uint8_t *)(_text);          \
} while (0);

#define bstring_set_raw(_str, _raw) do {         \
    (_str)->len = (uint32_t)(cc_strlen(_raw));  \
    (_str)->data = (uint8_t *)(_raw);           \
} while (0);

void bstring_init(struct bstring *str);
void bstring_deinit(struct bstring *str);
bool bstring_empty(const struct bstring *str);
rstatus_t bstring_duplicate(struct bstring *dst, const struct bstring *src);
rstatus_t bstring_copy(struct bstring *dst, const uint8_t *src, uint32_t srclen);
int bstring_compare(const struct bstring *s1, const struct bstring *s2);

/* efficient implementation of string comparion of short strings */
#define str3cmp(m, c0, c1, c2)                                                              \
    (m[0] == c0 && m[1] == c1 && m[2] == c2)

#ifdef CC_LITTLE_ENDIAN

#define str4cmp(m, c0, c1, c2, c3)                                                          \
    (*(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0))

#define str5cmp(m, c0, c1, c2, c3, c4)                                                      \
    (str4cmp(m, c0, c1, c2, c3) && (m[4] == c4))

#define str6cmp(m, c0, c1, c2, c3, c4, c5)                                                  \
    (str4cmp(m, c0, c1, c2, c3) &&                                                          \
        (((uint32_t *) m)[1] & 0xffff) == ((c5 << 8) | c4))

#define str7cmp(m, c0, c1, c2, c3, c4, c5, c6)                                              \
    (str6cmp(m, c0, c1, c2, c3, c4, c5) && (m[6] == c6))

#define str8cmp(m, c0, c1, c2, c3, c4, c5, c6, c7)                                          \
    (str4cmp(m, c0, c1, c2, c3) &&                                                          \
        (((uint32_t *) m)[1] == ((c7 << 24) | (c6 << 16) | (c5 << 8) | c4)))

#define str9cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8)                                      \
    (str8cmp(m, c0, c1, c2, c3, c4, c5, c6, c7) && m[8] == c8)

#define str10cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9)                                 \
    (str8cmp(m, c0, c1, c2, c3, c4, c5, c6, c7) &&                                          \
        (((uint32_t *) m)[2] & 0xffff) == ((c9 << 8) | c8))

#define str11cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10)                            \
    (str10cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9) && (m[10] == c10))

#define str12cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11)                       \
    (str8cmp(m, c0, c1, c2, c3, c4, c5, c6, c7) &&                                          \
        (((uint32_t *) m)[2] == ((c11 << 24) | (c10 << 16) | (c9 << 8) | c8)))

#else // BIG ENDIAN

#define str4cmp(m, c0, c1, c2, c3)                                                          \
    (str3cmp(m, c0, c1, c2) && (m3 == c3))

#define str5cmp(m, c0, c1, c2, c3, c4)                                                      \
    (str4cmp(m, c0, c1, c2, c3) && (m[4] == c4))

#define str6cmp(m, c0, c1, c2, c3, c4, c5)                                                  \
    (str5cmp(m, c0, c1, c2, c3, c4) && m[5] == c5)

#define str7cmp(m, c0, c1, c2, c3, c4, c5, c6)                                              \
    (str6cmp(m, c0, c1, c2, c3, c4, c5) && m[6] == c6)

#define str8cmp(m, c0, c1, c2, c3, c4, c5, c6, c7)                                          \
    (str7cmp(m, c0, c1, c2, c3, c4, c5, c6) && m[7] == c7)

#define str9cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8)                                      \
    (str8cmp(m, c0, c1, c2, c3, c4, c5, c6, c7) && m[8] == c8)

#define str10cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9)                                 \
    (str9cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8) && m[9] == c9)

#define str11cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10)                            \
    (str10cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9) && m[10] == c10)

#define str12cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11)                       \
    (str11cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10) && m[11] == c11)

#endif // CC_LITTLE_ENDIAN


/*
 * Wrapper around common routines for manipulating C character strings
 *
 * cc_memcpy
 * cc_memmove
 * cc_memchr
 *
 * cc_strlen
 * cc_strncmp
 * cc_strndup
 *
 * cc_strchr
 * cc_strrchr
 */

#define cc_memcpy(_d, _c, _n)                                   \
    memcpy(_d, _c, (size_t)(_n))

#define cc_memmove(_d, _c, _n)                                  \
    memmove(_d, _c, (size_t)(_n))

#define cc_memchr(_d, _c, _n)                                   \
    memchr(_d, _c, (size_t)(_n))

#define cc_strlen(_s)                                           \
    strlen((char *)(_s))

#define cc_bcmp(_s1, _s2, _n)                                   \
    bcmp((char *)(_s1), (char *)(_s2), (size_t)(_n))

#define cc_strncmp(_s1, _s2, _n)                                \
    strncmp((char *)(_s1), (char *)(_s2), (size_t)(_n))

#define cc_strndup(_s, _n)                                      \
    (uint8_t *)strndup((char *)(_s), (size_t)(_n));

#define cc_strchr(_p, _l, _c)                                   \
    _cc_strchr((uint8_t *)(_p), (uint8_t *)(_l), (uint8_t)(_c))

#define cc_strrchr(_p, _s, _c)                                  \
    _cc_strrchr((uint8_t *)(_p),(uint8_t *)(_s), (uint8_t)(_c))


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

#endif
