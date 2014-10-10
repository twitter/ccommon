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

#include <cc_option.h>

#include <cc_debug.h>
#include <cc_log.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static rstatus_t
_option_parse_bool(struct option *opt, char *val_str)
{
    rstatus_t status = CC_OK;

    if (strlen(val_str) == 3 && str3cmp(val_str, 'y', 'e', 's')) {
        opt->set = true;
        opt->val.vbool = true;
    } else if (strlen(val_str) == 2 && str2cmp(val_str, 'n', 'o')) {
        opt->set = true;
        opt->val.vbool = false;
    } else {
        log_error("unrecognized boolean option (valid values: 'yes' or 'no'), "
                "value provided: '%s'", val_str);

        status = CC_ERROR;
    }

    return status;
}

static rstatus_t
_option_parse_uint(struct option *opt, char *val_str)
{
    uintmax_t val;
    char *endptr;

    val = strtoumax(val_str, &endptr, 0); /* 0 auto detects among base 8/10/16 */

    if (val == UINTMAX_MAX) {
        log_error("unsigned integer option value overflows: %s", strerror(errno));

        return CC_ERROR;
    }

    if (endptr - val_str < strlen(val_str)) {
        log_error("unsigned int option value %s cannot be parsed completely: "
                "%s, stopped at position %zu", val_str, strerror(errno),
                endptr - val_str);

        return CC_ERROR;
    }

    opt->set = true;
    opt->val.vuint = val;

    return CC_OK;
}

static void
_option_parse_str(struct option *opt, char *val_str)
{
    if (val_str != NULL) {
        opt->set = true;
    }
    opt->val.vstr = val_str;
}

rstatus_t
option_set(struct option *opt, char *val_str)
{
    rstatus_t status;

    switch (opt->type) {
    case CONFIG_TYPE_BOOL:
        status = _option_parse_bool(opt, val_str);

	return status;

    case CONFIG_TYPE_UINT:
        status = _option_parse_uint(opt, val_str);

	return status;

    case CONFIG_TYPE_STR:
        _option_parse_str(opt, val_str);

	return CC_OK;

    default:
	log_error("option set error: unrecognized option type");
	return CC_ERROR;
    }

    NOT_REACHED();
}

static inline bool
_allowed_in_name(char c)
{
    /* the criteria is C's rules on variable names since we use it as such */
    if ((c >= 'a' && c <= 'z') || c == '_' || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9')) {
        return true;
    } else {
        return false;
    }
}

rstatus_t
option_parse(char *line, char name[OPTNAME_MAXLEN+1], char val[OPTVAL_MAXLEN+1])
{
    char *p = line;
    char *q;
    size_t vlen, llen = strlen(line);

    if (strlen(line) == 0 || isspace(line[0]) || line[0] == '#') {
        log_debug("empty line or comment line");

        return CC_EEMPTY;
    }

    if (llen > OPTLINE_MAXLEN) {
        log_error("option parse error: line length %zu exceeds limit %zu",
                llen, OPTLINE_MAXLEN);

        return CC_ERROR;
    }

    /* parse name */
    while (*p != ':' && (p - line) < llen && (p - line) <= OPTNAME_MAXLEN) {
        if (_allowed_in_name(*p)) {
            *name = *p;
            name++;
        } else {
            log_error("option parse error: invalid char'%c' at pos %d in name",
                    *p, (p - line));

            return CC_ERROR;
        }
        p++;
    }
    if (p - line == llen) {
        log_error("option parse error: incomplete option line");

        return CC_ERROR;
    }
    if (p - line > OPTNAME_MAXLEN) {
        log_error("option parse error: name too long (max %zu)",
                OPTNAME_MAXLEN);

        return CC_ERROR;
    }
    *name = '\0'; /* terminate name string properly */

    /* parse value: l/rtrim WS characters */
    p++; /* move over ':' */
    q = line + llen - 1;
    while (isspace(*p) && p < q) {
        p++;
    }
    while (isspace(*q) && q >= p) {
        q--;
    }
    if (p > q) {
        log_error("option parse error: empty value");

        return CC_ERROR;
    }
    vlen = q - p + 1; /* +1 because value range is [p, q] */
    if (vlen > OPTVAL_MAXLEN) {
        log_error("option parse error: value too long (max %zu)",
                OPTVAL_MAXLEN);

        return CC_ERROR;
    }
    /*
     * Here we don't use strlcpy() below because value is not NULL-terminated.
     * As long as the buffers parsed in are big enough (satisfy ASSERTs above),
     * we should be fine.
     */
    strncpy(val, p, vlen);
    *(val + vlen) = '\0'; /* terminate value string properly */

    return CC_OK;
}
