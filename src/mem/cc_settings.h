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

#ifndef _CC_SETTINGS_H_
#define _CC_SETTINGS_H_

#include <cc_time.h>
#include <mem/cc_slab.h>

#include <stdbool.h>
#include <stdint.h>

struct settings {
    bool            prealloc;  /* memory  : whether we preallocate for slabs */
    bool            evict_lru; /* memory  : if true, use lru eviction; else, random */
    bool            use_freeq; /* memory  : whether use items in freeq or not */
    bool            use_cas;   /* protocol: whether cas is supported */

    size_t          maxbytes; /* memory   : maximum bytes allowed for slabs */
    size_t          slab_size; /* memory  : slab size */
    uint32_t        hash_power; /* hash   : hash power (default is 16) */

    size_t          profile[SLABCLASS_MAX_IDS]; /* memory  : slab profile */
    uint8_t         profile_last_id; /* memory  : last id in slab profile */

    rel_time_t      oldest_live; /* data  : ignore existing items older than this */
};

extern struct settings settings;

#endif
