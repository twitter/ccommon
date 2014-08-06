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

#ifndef _CC_TIME_H_
#define _CC_TIME_H_

#include <sys/time.h>

/*
 * Time relative to server start time in seconds.
 *
 * On systems where size(time_t) > sizeof(unsigned int), this gives
 * us space savings over tracking absolute unix time of type time_t
 */
typedef unsigned int rel_time_t;

/* Update the current time */
void time_update(void);

/* Get the current time */
rel_time_t time_now(void);

/* Get the current absolute time (not time since process began) */
time_t time_now_abs(void);

/* Get the time the process started */
time_t time_started(void);

/* Get time relative to process start given absolute time */
rel_time_t time_reltime(time_t exptime);

/* Initialize time; record process start time */
void time_init(void);

#endif
