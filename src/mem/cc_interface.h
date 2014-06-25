#ifndef _CC_INTERFACE_H_
#define _CC_INTERFACE_H_

#include <stdint.h>

/*
 * This interface module provides a way to interact with the cache to store
 * and retrieve key/value pairs. 
 */

/*
 * Store the key value pair in the cache. Overwrites if the key already exists.
 */
void store_key_val(void *key, uint8_t nkey, void *val, uint32_t nval);

/*
 * Store the key value pair, but only if the server does not already hold data 
 * for that key.
 */
void add_key_val(void *key, uint8_t nkey, void *val, uint32_t nval);

/*
 * Store the key value pair, but only if the server already holds data for that
 * key.
 */
void replace_key_val(void *key, uint8_t nkey, void *val, uint32_t nval);

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
 * Retrieves the value corresponding with the provided key. User is responsible
 * for freeing the value that is retrieved.
 */
void *get_val(void *key, uint8_t nkey);

/*
 * Removes a key/value pair from the cache.
 */
void delete_key_val(void *key, uint8_t nkey);

#endif
