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

#include <stdarg.h>
#include <stdio.h>

#include <cc_debug.h>
#include <cc_mm.h>

#include <cc_string.h>

/*
 * Byte string (struct bstring) is a sequence of unsigned char
 * The length of the string is pre-computed and explicitly available.
 * This means that we don't have to walk the entire character sequence until
 * the null terminating character every time. We also treat the strings as
 * byte strings, meaning we ignore the terminating '\0' and only use the length
 * information in copy, comparison etc.
 *
 * The only way to create a String is to initialize it using, bstring_init()
 * and duplicate an existing String - bstring_duplicate() or copy an existing
 * raw sequence of character bytes - bstring_copy(). Such String's must be
 * freed using bstring_deinit()
 *
 * We can also create String as reference to raw string - bstring_set_raw()
 * or to string literal - bstring_set_text() or bstring(). Such bstrings don't
 * have to be freed.
 */

void
bstring_init(struct bstring *bstr)
{
    bstr->len = 0;
    bstr->data = NULL;
}

void
bstring_deinit(struct bstring *bstr)
{
    ASSERT((bstr->len == 0 && bstr->data == NULL) ||
           (bstr->len != 0 && bstr->data != NULL));

    if (bstr->data != NULL) {
        cc_free(bstr->data);
        bstring_init(bstr);
    }
}

bool
bstring_empty(const struct bstring *str)
{
    ASSERT((str->len == 0 && str->data == NULL) ||
           (str->len != 0 && str->data != NULL));
    return str->len == 0 ? true : false;
}

rstatus_t
bstring_duplicate(struct bstring *dst, const struct bstring *src)
{
    ASSERT(dst->len == 0 && dst->data == NULL);
    ASSERT(src->len != 0 && src->data != NULL);

    dst->data = cc_alloc(src->len);
    if (dst->data == NULL) {
        return CC_ENOMEM;
    }

    cc_memcpy(dst->data, src->data, src->len);
    dst->len = src->len;

    return CC_OK;
}

rstatus_t
bstring_copy(struct bstring *dst, const uint8_t *src, uint32_t srclen)
{
    ASSERT(dst->len == 0 && dst->data == NULL);
    ASSERT(src != NULL && srclen != 0);

    dst->data = cc_alloc(srclen);
    if (dst->data == NULL) {
        return CC_ENOMEM;
    }

    cc_memcpy(dst->data, src, srclen);
    dst->len = srclen;

    return CC_OK;
}

int
bstring_compare(const struct bstring *s1, const struct bstring *s2)
{
    /*
     * the max value difference between two unsigned chars is 255,
     * so we can use 256 to indicate a length difference in case it's useful
     */
    if (s1->len != s2->len) {
        return s1->len - s2->len > 0 ? 256 : -256;
    }

    return cc_bcmp(s1->data, s2->data, s1->len);
}

int
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

int
_scnprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i = _vscnprintf(buf, size, fmt, args);
    va_end(args);

    return i;
}
