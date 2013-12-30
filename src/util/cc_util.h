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

#ifndef _CC_UTIL_H_
#define _CC_UTIL_H_

/* syntax */
#define LF              (uint8_t) 10
#define CR              (uint8_t) 13
#define CRLF            "\r\n"
#define CRLF_LEN        (uint32_t) (sizeof(CRLF) - 1)

/* unit */
#define KiB             (1024)
#define MiB             (1024 * KiB)
#define GiB             (1024 * MiB)

/* math */
#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#define MAX(a, b)           ((a) > (b) ? (a) : (b))

#define SQUARE(d)           ((d) * (d))
#define VAR(s, s2, n)       (((n) < 2) ? 0.0 : ((s2) - SQUARE(s)/(n)) / ((n) - 1))
#define STDDEV(s, s2, n)    (((n) < 2) ? 0.0 : sqrt(VAR((s), (s2), (n))))

/* network */
#define CC_INET4_ADDRSTRLEN (sizeof("255.255.255.255") - 1)
#define CC_INET6_ADDRSTRLEN \
    (sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") - 1)
#define CC_INET_ADDRSTRLEN  MAX(CC_INET4_ADDRSTRLEN, CC_INET6_ADDRSTRLEN)
#define CC_UNIX_ADDRSTRLEN  \
    (sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path))
#define CC_MAXHOSTNAMELEN   256
