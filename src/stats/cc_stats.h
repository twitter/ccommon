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

#ifndef _CC_STATS_H_
#define _CC_STATS_H_

#include <cc_define.h>

#include <inttypes.h>

#if defined CC_STATS && CC_STATS == 1

#define stats_incr(_c)        __atomic_add_fetch(&(_c), 1, __ATOMIC_RELAXED)
#define stats_incr(_c)        __atomic_add_fetch(&(_c), 1, __ATOMIC_RELAXED)
#define stats_incr_n(_c, _d)  __atomic_add_fetch(&(_c), (_d), __ATOMIC_RELAXED)
#define stats_decr(_c)        __atomic_sub_fetch(&(_c), 1, __ATOMIC_RELAXED)
#define stats_decr_n(_c, _d)  __atomic_sub_fetch(&(_c), (_d), __ATOMIC_RELAXED)

#define STATS_DECLARE(_name, _type, _description)   \
    struct stats _name;

#define STATS_INIT(_name, _type, _description)      \
    ._name = {.name = #_name, .type = _type},

#define STATS_NAME(_name, _type, _description)      \
    #_name,

#else

#define stats_incr(_c)
#define stats_incr_n(_c, _d)
#define stats_decr(_c)
#define stats_decr_n(_c, _d)

#define STATS_DECLARE(_name, _type, _description)
#define STATS_INIT(_name, _type, _description)
#define STATS_NAME(_name, _type, _description)

#endif

#define STATS_CARDINALITY(_o) sizeof(_o) / sizeof(struct stats)

typedef enum stats_type {
    METRIC_COUNTER,
    METRIC_GAUGE
} stats_type_t;

typedef uint64_t counter_t;
typedef int64_t gauge_t;

/* Note: anonymous union does not work with older (<gcc4.7) compilers */
/* TODO(yao): determine if we should dynamically allocate the value field
 * during init. The benefit is we don't have to allocate the same amount of
 * memory for different types of values, potentially wasting space. */
struct stats {
    const char *name;
    const stats_type_t type;
    union {
        counter_t counter;
        gauge_t gauge;
    };
};

void stats_reset(struct stats sarr[], unsigned int nstats);
void stats_setup(void);
void stats_teardown(void);

#endif /* _CC_STATS_H_ */
