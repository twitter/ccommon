#include "cc_interface.h"

#include "cc_assoc.h"
#include "cc_items.h"
#include "cc_slabs.h"
#include "cc_settings.h"
#include "cc_time.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static struct item *create_item(void *key, uint8_t nkey, void *val, uint32_t nval);
static void check_annex_status(item_annex_result_t ret);
static void check_delta_status(item_delta_result_t ret);

void 
store_key_val(void *key, uint8_t nkey, void *val, uint32_t nval)
{
    struct item *new_item = create_item(key, nkey, val, nval);

    item_set(new_item);
    item_remove(new_item);
}

void
add_key_val(void *key, uint8_t nkey, void *val, uint32_t nval)
{
    struct item *new_item = create_item(key, nkey, val, nval);

    if(item_add(new_item) == ADD_EXISTS) {
	printf("server already holds data for that key, value not stored.\n");
    }

    item_remove(new_item);
}

void
replace_key_val(void *key, uint8_t nkey, void *val, uint32_t nval)
{
    struct item *new_item = create_item(key, nkey, val, nval);

    if(item_replace(new_item) == REPLACE_NOT_FOUND) {
	printf("server does not hold data for that key, value not stored.\n");
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

/*
 * Currently, the implementation null terminates whatever value it retrieves for
 * the key; this is for ease of testing. This is subject to change later.
 */
void *
get_val(void *key, uint8_t nkey)
{
    struct item *it, *iter;
    char *val;
    uint32_t amt_copied = 0;

    it = item_get(key, nkey);

    if(it == NULL) {
	printf("No item with key %s!\n", key);
	return NULL;
    }

    val = malloc(item_total_nbyte(it) + 1);
    val[item_total_nbyte(it)] = '\0';
    if(val == NULL) {
	printf("Not enough memory for that!\n");
	return NULL;
    }

    for(iter = it; iter != NULL; iter = iter->next_node) {
	memcpy(val + amt_copied, item_data(iter), iter->nbyte);
	amt_copied += iter->nbyte;
    }

    item_remove(it);
    return val;
}

void
delete_key_val(void *key, uint8_t nkey)
{
    if(item_delete(key, nkey) == DELETE_NOT_FOUND) {
	printf("No such key!\n");
    } else {
	printf("Item %s deleted\n", key);
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
	printf("Not enough memory for that!\n");
	return NULL;
    }

    for(iter = ret; iter != NULL; iter = iter->next_node) {
	memcpy(item_data(iter), (char*)val + amt_copied, iter->nbyte);
	amt_copied += iter->nbyte;
    }

    assert(amt_copied == nval);
    return ret;
}

static void
check_annex_status(item_annex_result_t ret)
{
    if(ret == ANNEX_OVERSIZED) {
	printf("Annex too large! Data to be annexed must be able to fit in one"
	       " item.\n");
    } else if(ret == ANNEX_NOT_FOUND) {
	printf("No item with that key!\n");
    } else if(ret == ANNEX_EOM) {
	printf("Not enough memory for that!\n");
    }
}

static void
check_delta_status(item_delta_result_t ret)
{
    if(ret == DELTA_NOT_FOUND) {
	printf("No item with that key!\n");
    } else if(ret == DELTA_NON_NUMERIC) {
	printf("Value is non numeric, cannot perform delta operation!\n");
    } else if(ret == DELTA_EOM) {
	printf("Not enough memory for that!\n");
    }
	
}
