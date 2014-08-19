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

#ifndef _CC_MEM_INTERFACE_H_
#define _CC_MEM_INTERFACE_H_

#include <cc_define.h>

#include <stdbool.h>
#include <stdint.h>
#include <sys/uio.h>

/*
 * This interface module provides a way to interact with the cache to store
 * and retrieve key/value pairs.
 */

struct item;

/*
 * Store the key value pair in the cache. Overwrites if the key already exists.
 */
void store_key(void *key, uint8_t nkey, void *val, uint32_t nval);

/*
 * Store the key value pair, but only if the server does not already hold data
 * for that key.
 */
void add_key(void *key, uint8_t nkey, void *val, uint32_t nval);

/*
 * Store the key value pair, but only if the server already holds data for that
 * key.
 */
void replace_key(void *key, uint8_t nkey, void *val, uint32_t nval);

/*
 * Appends val to the end of the item with the corresponding key
 */
void append_val(void *key, uint8_t nkey, void *val, uint32_t nval);

/*
 * Prepends val to the end of the item with the corresponding key
 */
void prepend_val(void *key, uint8_t nkey, void *val, uint32_t nval);

/*
 * Increments the value with the corresponding key by delta
 */
void increment_val(void *key, uint8_t nkey, uint64_t delta);

/*
 * Decrements the value with the corresponding key by delta
 */
void decrement_val(void *key, uint8_t nkey, uint64_t delta);

/*
 * Get the size of the value (in bytes) associated with the given key.
 */
uint64_t get_val_size(void *key, uint8_t nkey);

#if defined CC_HAVE_CHAINED && CC_HAVE_CHAINED == 1
/*
 * Get the number of nodes for the item with the given key
 */
size_t get_num_nodes(void *key, uint8_t nkey);
#endif

/*
 * Grants access to the value associated to the given key by making it accessible
 * via the vector parameter. User is responsible for allocating a large enough
 * struct iovec array (must be able to contain at least as many struct iovec as
 * the number of nodes in the item associated with key). Returns true on success
 * and false on failure.
 */
bool get_val_ref(void *key, uint8_t nkey, struct iovec *vector);

/*
 * Retrieves the value corresponding with the provided key, and copies it over
 * to the provided buffer. Copying starts at offset, and is performed until all
 * of the data is copied, or until buf_size bytes are copied. Returns true on
 * success and false on failure.
 */
bool get_val(void *key, uint8_t nkey, void *buf, uint64_t buf_size, uint64_t offset);

/*
 * Removes a key/value pair from the cache.
 */
void remove_key(void *key, uint8_t nkey);

/*
 * Create an item with the given key and value. Does not link item.
 */
struct item *create_item(void *key, uint8_t nkey, void *val, uint32_t nval);

#endif /* _CC_MEM_INTERFACE_ */
