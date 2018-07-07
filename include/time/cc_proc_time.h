/*
 * ccommon - a cache common library.
 * Copyright (C) 2018 Twitter, Inc.
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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <time.h>

/*
 * For less granular time, rel_time_t will suffice and gives space savings.
 * For more granular time, rel_time_fine_t gives additional precision.
 */
typedef uint32_t rel_time_t;
typedef uint64_t rel_time_fine_t;

/*
 * Time when the process was started expressed as absolute unix timestamp
 */
extern time_t time_start;

/*
 * Current time relative to process start. These are updated with each call
 * to time_update().
 */
extern rel_time_t now_sec;
extern rel_time_fine_t now_ms;
extern rel_time_fine_t now_us;
extern rel_time_fine_t now_ns;

/* Get the time the process was started */
static inline time_t
time_started(void)
{
    return __atomic_load_n(&time_start, __ATOMIC_RELAXED);
}

/* Get the current time in seconds since the process started */
static inline rel_time_t
time_now(void)
{
    return __atomic_load_n(&now_sec, __ATOMIC_RELAXED);
}

/* Get the current time in milliseconds since the process started */
static inline rel_time_fine_t
time_now_ms(void)
{
    return __atomic_load_n(&now_ms, __ATOMIC_RELAXED);
}

/* Get the current time in microseconds since the process started */
static inline rel_time_fine_t
time_now_us(void)
{
    return __atomic_load_n(&now_us, __ATOMIC_RELAXED);
}

/* Get the current time in nanoseconds since the process started */
static inline rel_time_fine_t
time_now_ns(void)
{
    return __atomic_load_n(&now_ns, __ATOMIC_RELAXED);
}

/* Get the current absolute time (not since process began) */
static inline time_t
time_now_abs(void)
{
    return time_started() + time_now();
}

/* Because time objects are shared, only one thread should call time_update */
void time_update(void);

void time_setup(void);
void time_teardown(void);

#ifdef __cplusplus
}
#endif
