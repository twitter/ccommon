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

#ifndef _CC_HASH_TABLE_H_
#define _CC_HASH_TABLE_H_

#include <cc_define.h>
#include <stdlib.h>

#define HASH_MAX_POWER  32

/* Initialize hash table */
rstatus_t hash_table_init(void);

/* Destroy hash table */
void hash_table_deinit(void);


/* Find the item associated with the given key */
struct item *hash_table_find(const char *key, size_t nkey);

/* Insert the item into the hash table */
void hash_table_insert(struct item *it);

/* Delete the item with the given key from the hash table */
void hash_table_delete(const char *key, size_t nkey);

#endif /* _CC_HASH_TABLE_H_ */
