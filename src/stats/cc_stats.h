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

#if defined CC_STATS && CC_STATS == 1

#define stats_incr(_c)        __atomic_add_fetch(&(_c), 1, __ATOMIC_RELAXED)
#define stats_incr_n(_c, _d)  __atomic_add_fetch(&(_c), (_d), __ATOMIC_RELAXED)
#define stats_decr(_c)        __atomic_sub_fetch(&(_c), 1, __ATOMIC_RELAXED)
#define stats_decr_n(_c, _d)  __atomic_sub_fetch(&(_c), (_d), __ATOMIC_RELAXED)

#else

#define stats_incr(_c)
#define stats_incr_n(_c, _d)
#define stats_decr(_c)
#define stats_decr_n(_c, _d)

#endif

void stats_setup(void);
void stats_teardown(void);

#endif /* _CC_STATS_H_ */
