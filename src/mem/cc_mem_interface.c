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

#include <mem/cc_items.h>
#include <mem/cc_settings.h>
#include <mem/cc_slabs.h>
#include <mem/cc_time.h>
#include <cc_debug.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <cc_string.h>

#include <limits.h>

static struct item *create_item(void *key, uint8_t nkey, void *val, uint32_t nval);
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
	log_stderr("Server already holds data for key %s, value not stored.\n", key);
    }

    item_remove(new_item);
}

void
replace_key(void *key, uint8_t nkey, void *val, uint32_t nval)
{
    struct item *new_item = create_item(key, nkey, val, nval);

    if(item_replace(new_item) == REPLACE_NOT_FOUND) {
	log_stderr("Server does not hold data for key %s, value not stored\n", key);
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
	log_stderr("No item with key %s!\n", key);
	return 0;
    }

    ret = item_total_nbyte(it);
    item_remove(it);
    return ret;
}

size_t
get_num_nodes(void *key, uint8_t nkey)
{
    size_t ret;
    struct item *it = item_get(key, nkey);

    if(it == NULL) {
	log_stderr("No item with key %s!\n", key);
	return 0;
    }

    ret = item_num_nodes(it);
    item_remove(it);
    return ret;
}

bool
get_val_ref(void *key, uint8_t nkey, struct iovec *vector)
{
    uint32_t i;
    struct item *it, *iter;

    ASSERT(vector != NULL);

    it = item_get(key, nkey);

    if(it == NULL) {
	log_stderr("No item with key %s!\n", key);
	return false;
    }

    /* Store the item payload location and number of bytes for each node into
       vector */
    for(i = 0, iter = it; iter != NULL; iter = iter->next_node, ++i) {
	vector[i].iov_base = item_data(iter);
	vector[i].iov_len = iter->nbyte;
    }

    item_remove(it);

    return true;
}

bool
get_val(void *key, uint8_t nkey, void *buf, uint64_t buf_size, uint64_t offset)
{
    struct item *it, *iter;
    uint64_t amt_copied;
    size_t amt_to_copy;

    ASSERT(buf != NULL);

    it = item_get(key, nkey);

    if(it == NULL) {
	log_stderr("No item with key %s!\n", key);
	return false;
    }

    /* Get to the correct node for offset */
    for(iter = it; iter != NULL && offset > iter->nbyte;
	iter = iter->next_node, offset -= iter->nbyte);

    if(iter == NULL) {
	log_stderr("Offset too large!\n");
	return false;
    }

    /* Copy over data from first node */
    amt_to_copy = (iter->nbyte - offset < buf_size) ? iter->nbyte - offset : buf_size;
    cc_memcpy(buf, item_data(iter) + offset, amt_to_copy);
    log_stderr("@@@ item_data: %p", item_data(iter) + offset);
    log_stderr("@@@ buf: %s", buf);
    amt_copied = amt_to_copy;

    /* Copy over the rest of the data */
    for(iter = iter->next_node; iter != NULL && amt_copied < buf_size;
	iter = iter->next_node) {
	amt_to_copy = (buf_size - amt_copied < iter->nbyte) ?
	    buf_size - amt_copied : iter->nbyte;
	cc_memcpy(buf + amt_copied, item_data(iter), amt_to_copy);
	log_stderr("@@@ item_data: %p", item_data(iter));
	log_stderr("@@@ buf: %s", buf);
	amt_copied += amt_to_copy;
    }

    item_remove(it);

    return true;
}

void
remove_key(void *key, uint8_t nkey)
{
    if(item_delete(key, nkey) == DELETE_NOT_FOUND) {
	log_stderr("key %s does not exist\n", key);
    } else {
	log_stderr("Item %s deleted\n", key);
    }
}

/*
 * Creates an item and initializes its data with val.
 */
static struct item *
create_item(void *key, uint8_t nkey, void *val, uint32_t nval)
{
    struct item *ret, *iter;
    uint32_t amt_copied = 0;

    /* Currently exptime is arbitrarily set; not sure what to do about this
       yet. */
    ret = item_alloc(key, nkey, 0, time_now() + 6000, nval);
    if(ret == NULL) {
	log_stderr("Not enough memory to allocate item\n");
	return NULL;
    }

    /* Copy over data in val */
    for(iter = ret; iter != NULL; iter = iter->next_node) {
	cc_memcpy(item_data(iter), (char*)val + amt_copied, iter->nbyte);
	amt_copied += iter->nbyte;
    }

    ASSERT(amt_copied == nval);
    return ret;
}

/*
 * Handle append/prepend return value
 */
static void
check_annex_status(item_annex_result_t ret)
{
    if(ret == ANNEX_OVERSIZED) {
	log_stderr("Annex operation too large; data to be annexed must be able "
		   "to fit in one unchained item.\n");
    } else if(ret == ANNEX_NOT_FOUND) {
	log_stderr("Cannot annex: no item with that key found.\n");
    } else if(ret == ANNEX_EOM) {
	log_stderr("Cannot annex: not enough memory.\n");
    }
}

/*
 * Handle increment/decrement return value
 */
static void
check_delta_status(item_delta_result_t ret)
{
    if(ret == DELTA_NOT_FOUND) {
	log_stderr("Cannot perform delta operation: no item with that key found.\n");
    } else if(ret == DELTA_NON_NUMERIC) {
	log_stderr("Cannot perform delta operation: value is not numeric.\n");
    } else if(ret == DELTA_EOM) {
	log_stderr("Cannot perform delta operation: not enough memory.\n");
    }
}
