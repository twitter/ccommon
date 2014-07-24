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

#ifndef _CC_MEM_GLOBALS_H_
#define _CC_MEM_GLOBALS_H_

#include <pthread.h>
#include <sys/types.h>

extern pthread_mutex_t cache_lock; /* lock protecting the cache */
extern pthread_mutex_t slab_lock;  /* lock protecting slabclass and heapinfo */

#endif /* _CC_MEM_GLOBALS_H_ */
