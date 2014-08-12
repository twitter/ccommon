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

#include <mem/cc_mem_interface.h>

#include <cc_debug.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <cc_string.h>
#include <cc_time.h>
#include <mem/cc_item.h>
#include <mem/cc_settings.h>
#include <mem/cc_slab.h>

#include <limits.h>

static void check_annex_status(item_annex_result_t ret);
static void check_delta_status(item_delta_result_t ret);

void
store_key(void *key, uint8_t nkey, void *val, uint32_t nval)
{
    struct item *new_item = create_item(key, nkey, val, nval);

    item_set(new_item);
    item_remove(new_item);
}

void
add_key(void *key, uint8_t nkey, void *val, uint32_t nval)
{
    struct item *new_item = create_item(key, nkey, val, nval);

    if(item_add(new_item) == ADD_EXISTS) {
	log_debug(LOG_NOTICE, "Server already holds data for key %s, value not stored.", key);
    }

    item_remove(new_item);
}

void
replace_key(void *key, uint8_t nkey, void *val, uint32_t nval)
{
    struct item *new_item = create_item(key, nkey, val, nval);

    if(item_replace(new_item) == REPLACE_NOT_FOUND) {
	log_debug(LOG_NOTICE, "Server does not hold data for key %s, value not stored.", key);
    }

    item_remove(new_item);
}

void
append_val(void *key, uint8_t nkey, void *val, uint32_t nval)
{
    struct item *it = create_item(key, nkey, val, nval);

    check_annex_status(item_append(it));

    item_remove(it);
}

void
prepend_val(void *key, uint8_t nkey, void *val, uint32_t nval)
{
    struct item *it = create_item(key, nkey, val, nval);

    check_annex_status(item_prepend(it));

    item_remove(it);
}

void
increment_val(void *key, uint8_t nkey, uint64_t delta)
{
    check_delta_status(item_delta(key, nkey, true, delta));
}

void
decrement_val(void *key, uint8_t nkey, uint64_t delta)
{
    check_delta_status(item_delta(key, nkey, false, delta));
}

uint64_t
get_val_size(void *key, uint8_t nkey)
{
    uint64_t ret;
    struct item *it = item_get(key, nkey);

    if(it == NULL) {
	log_debug(LOG_NOTICE, "No item with key %s!", key);
	return 0;
    }

    ret = item_total_nbyte(it);
    item_remove(it);
    return ret;
}

#if defined CC_CHAINED && CC_CHAINED == 1
size_t
get_num_nodes(void *key, uint8_t nkey)
{
    size_t ret;
    struct item *it = item_get(key, nkey);

    if(it == NULL) {
	log_debug(LOG_NOTICE, "No item with key %s!", key);
	return 0;
    }

    ret = item_num_nodes(it);
    item_remove(it);
    return ret;
}
#endif

bool
get_val_ref(void *key, uint8_t nkey, struct iovec *vector)
{
    uint32_t i = 0;
    struct item *it;
#if defined CC_CHAINED && CC_CHAINED == 1
    struct item *iter;
#endif

    ASSERT(vector != NULL);

    it = item_get(key, nkey);

    if(it == NULL) {
	log_debug(LOG_NOTICE, "No item with key %s!", key);
	return false;
    }

#if defined CC_CHAINED && CC_CHAINED == 1
    /* Store the item payload location and number of bytes for each node into
       vector */
    for(iter = it; iter != NULL; iter = iter->next_node, ++i) {
	vector[i].iov_base = item_data(iter);
	vector[i].iov_len = iter->nbyte;
    }
#else
    vector[i].iov_base = item_data(it);
    vector[i].iov_len = it->nbyte;
#endif

    item_remove(it);

    return true;
}

bool
get_val(void *key, uint8_t nkey, void *buf, uint64_t buf_size, uint64_t offset)
{
    struct item *it;
#if defined CC_CHAINED && CC_CHAINED == 1
    uint64_t amt_copied;
    size_t amt_to_copy;
    struct item *iter;
#endif

    ASSERT(buf != NULL);

    it = item_get(key, nkey);

    if(it == NULL) {
	log_debug(LOG_NOTICE, "No item with key %s!", key);
	return false;
    }

#if defined CC_CHAINED && CC_CHAINED == 1
    /* Get to the correct node for offset */
    for(iter = it; iter != NULL && offset > iter->nbyte;
	iter = iter->next_node, offset -= iter->nbyte);

    if(iter == NULL) {
	log_debug(LOG_NOTICE, "Offset too large!");
	return false;
    }

    /* Copy over data from first node */
    amt_to_copy = (iter->nbyte - offset < buf_size) ?
	iter->nbyte - offset : buf_size;
    cc_memcpy(buf, item_data(iter) + offset, amt_to_copy);
    amt_copied = amt_to_copy;

    /* Copy over the rest of the data */
    for(iter = iter->next_node; iter != NULL && amt_copied < buf_size;
	iter = iter->next_node) {
	amt_to_copy = (buf_size - amt_copied < iter->nbyte) ?
	    buf_size - amt_copied : iter->nbyte;
	cc_memcpy(buf + amt_copied, item_data(iter), amt_to_copy);
	amt_copied += amt_to_copy;
    }
#else
if(offset >= it->nbyte) {
	log_debug(LOG_NOTICE, "Offset too large!");
	return false;
    }

    cc_memcpy(buf, item_data(it) + offset,
	      (it->nbyte - offset < buf_size) ? it->nbyte - offset : buf_size);
#endif

    item_remove(it);

    return true;
}

void
remove_key(void *key, uint8_t nkey)
{
    if(item_delete(key, nkey) == DELETE_NOT_FOUND) {
	log_debug(LOG_NOTICE, "key %s does not exist", key);
    } else {
	log_debug(LOG_VERB, "Item %s deleted", key);
    }
}

#if defined CC_CHAINED && CC_CHAINED == 1
struct item *
create_item(void *key, uint8_t nkey, void *val, uint32_t nval)
{
    struct item *ret, *iter;
    uint32_t amt_copied = 0;

    /* Currently exptime is arbitrarily set; not sure what to do about this yet */
    ret = item_alloc(nkey, time_now() + 6000, nval);

    if(ret == NULL) {
	log_debug(LOG_WARN, "Not enough memory to allocate item");
	return NULL;
    }

    /* Copy over key */
    cc_memcpy(item_key(ret), key, nkey);

    /* Copy over data in val */
    for(iter = ret; iter != NULL; iter = iter->next_node) {
	cc_memcpy(item_data(iter), (uint8_t *)val + amt_copied, iter->nbyte);
	amt_copied += iter->nbyte;
    }

    ASSERT(amt_copied == nval);
    return ret;
}
#else
struct item *
create_item(void *key, uint8_t nkey, void *val, uint32_t nval)
{
    struct item *ret;

    if(item_slabid(nkey, nval) == SLABCLASS_CHAIN_ID) {
	log_debug(LOG_NOTICE, "No slabclass large enough to contain item of that"
		  " size! (try turning chaining on)");
	return NULL;
    }

    /* Currently exptime is arbitrarily set; not sure what to do about this yet */
    ret = item_alloc(nkey, time_now() + 6000, nval);

    if(ret == NULL) {
	log_debug(LOG_WARN, "Not enough memory to allocate item");
	return NULL;
    }

    /* Copy over key and val */
    cc_memcpy(item_key(ret), key, nkey);
    cc_memcpy(item_data(ret), val, nval);

    return ret;
}
#endif

/*
 * Handle append/prepend return value
 */
static void
check_annex_status(item_annex_result_t ret)
{
    switch(ret) {
    case ANNEX_OVERSIZED:
	log_debug(LOG_NOTICE, "Cannot annex: annex operation too large");
	break;
    case ANNEX_NOT_FOUND:
	log_debug(LOG_NOTICE, "Cannot annex: no item with that key found");
	break;
    case ANNEX_EOM:
	log_debug(LOG_WARN, "Cannot annex: not enough memory");
	break;
    default:
	break;
    }
}

/*
 * Handle increment/decrement return value
 */
static void
check_delta_status(item_delta_result_t ret)
{
    switch(ret) {
    case DELTA_NOT_FOUND:
	log_debug(LOG_NOTICE, "Cannot perform delta operation: no item with that key found.");
	break;
    case DELTA_NON_NUMERIC:
	log_debug(LOG_NOTICE, "Cannot perform delta operation: value is not numeric.");
	break;
    case DELTA_EOM:
	log_debug(LOG_WARN, "Cannot perform delta operation: not enough memory.");
	break;
    case DELTA_CHAINED:
	log_debug(LOG_NOTICE, "Cannot perform delta operation: target is chained.");
	break;
    default:
	break;
    }
}
