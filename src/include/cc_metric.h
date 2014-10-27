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

#ifndef _CC_METRIC_H_
#define _CC_METRIC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <cc_define.h>

#include <inttypes.h>

#if defined CC_STATS && CC_STATS == 1

#define metric_incr_n(_c, _d) do {                                  \
    if ((_c)->type == METRIC_COUNTER) {                             \
         __atomic_add_fetch(&(_c)->counter, (_d), __ATOMIC_RELAXED);\
    } else if ((_c)->type == METRIC_GAUGE) {                        \
         __atomic_add_fetch(&(_c)->gauge, (_d), __ATOMIC_RELAXED);  \
    } else { /* error  */                                           \
    }                                                               \
} while(0)
#define metric_incr(_c) metric_incr_n(_c, 1)


#define metric_decr_n(_c, _d) do {                                  \
    if ((_c)->type == METRIC_GAUGE) {                               \
         __atomic_sub_fetch(&(_c)->gauge, (_d), __ATOMIC_RELAXED);  \
    } else { /* error  */                                           \
    }                                                               \
} while(0)
#define metric_decr(_c) metric_decr_n(_c, 1)

#define METRIC_DECLARE(_name, _type, _description)   \
    struct metric _name;

#define METRIC_INIT(_name, _type, _description)      \
    ._name = {.name = #_name, .type = _type},

#define METRIC_NAME(_name, _type, _description)      \
    #_name,

#else

#define metric_incr(_c)
#define metric_incr_n(_c, _d)
#define metric_decr(_c)
#define metric_decr_n(_c, _d)

#define METRIC_DECLARE(_name, _type, _description)
#define METRIC_INIT(_name, _type, _description)
#define METRIC_NAME(_name, _type, _description)

#endif

#define METRIC_CARDINALITY(_o) sizeof(_o) / sizeof(struct metric)

typedef enum metric_type {
    METRIC_COUNTER,
    METRIC_GAUGE,
    /* directly set values */
    METRIC_DDOUBLE,
    METRIC_DINTMAX
} metric_type_t;

typedef uint64_t counter_t;
typedef int64_t gauge_t;

/* Note: anonymous union does not work with older (<gcc4.7) compilers */
/* TODO(yao): determine if we should dynamically allocate the value field
 * during init. The benefit is we don't have to allocate the same amount of
 * memory for different types of values, potentially wasting space. */
struct metric {
    const char *name;
    const metric_type_t type;
    union {
        counter_t counter;
        gauge_t gauge;
        double vdouble;
        intmax_t vintmax;
    };
};

void metric_reset(struct metric sarr[], unsigned int nmetric);
void metric_setup(void);
void metric_teardown(void);

#ifdef __cplusplus
}
#endif

#endif /* _CC_METRIC_H_ */
