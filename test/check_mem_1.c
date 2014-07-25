#include <check.h>

#include "../src/mem/cc_mem_interface.h"
#include "../src/mem/cc_settings.h"
#include "../src/mem/cc_slabs.h"
#include "../src/mem/cc_items.h"
#include "../src/cc_define.h"
#include "../src/hash/cc_hash_table.h"
#include "../src/mem/cc_zipmap.h"
#include "../src/cc_mm.h"
#include "../src/cc_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

void init_settings(void);

void
init_settings(void) {
    settings.prealloc = true;
    settings.evict_lru = false;
    settings.use_freeq = true;
    settings.use_cas = false;
    settings.maxbytes = 8448;
    settings.slab_size = 1056;
    settings.profile[1] = 128;
    settings.profile[2] = 256;
    settings.profile[3] = 512;
    settings.profile[4] = 1024;
    settings.profile_last_id = 4;
    settings.oldest_live = 6000;
}

START_TEST(check_mem_basic)
{
    rstatus_t return_status;
    char *val;
    struct iovec *vector;
    ssize_t bytes;
    struct item *it;
    uint32_t i;

    init_settings();
    time_init();

    return_status = item_init(0); /* Parameter is hash table power, 0 for default. */
    if(return_status != CC_OK) {
	ck_abort_msg("Item init failed! Error code %d", return_status);
    }

    return_status = slab_init();
    if(return_status != CC_OK) {
	ck_abort_msg("Slab init failed! Error code %d", return_status);
    }

    /* Store keys foo and foobar */
    store_key("foo", 3, "bar", 3);
    store_key("foobar", 6, "foobarfoobar", 12);

    /* Get key foo with larger than necessary buffer */
    val = malloc(25);
    memset(val, '\0', 25);
    ck_assert_msg(get_val("foo", 3, val, 24, 0), "get_val failed for key foo!");
    ck_assert_msg(strcmp(val, "bar") == 0, "Wrong value for key foo! Expected bar, got %s", val);
    free(val);

    /* Get key foobar */
    vector = malloc(sizeof(struct iovec));
    ck_assert_msg(get_val_ref("foobar", 6, vector), "get_val_ref failed for key foobar!");
    bytes = writev(STDOUT_FILENO, vector, 1);
    ck_assert_msg(bytes == 12, "Wrote incorrect number of bytes! nbytes: %d errno: %d", bytes, errno);
    free(vector);

    /* Set numeric value */
    store_key("num", 3, "100", 3);

    /* increment num */
    increment_val("num", 3, 50);

    /* get num */
    val = malloc(4);
    ck_assert_msg(get_val("num", 3, val, 3, 0), "get_val failed for key num!");
    val[3] = '\0';
    ck_assert_msg(strcmp(val, "150") == 0, "Wrong value for key num! Expected 150, got %s", val);
    free(val);

    /* Append to key foobar */
    append_val("foobar", 6, "bazquxbazqux", 12);

    /* get key foobar */
    val = malloc(25);
    memset(val, '\0', 25);
    ck_assert_msg(get_val("foobar", 6, val, 24, 0), "get_val failed for key foo!");
    ck_assert_msg(strcmp(val, "foobarfoobarbazquxbazqux") == 0, "Wrong value for key foobar! got %s", val);
    free(val);

    /* Prepend to key foobar */
    prepend_val("foobar", 6, "bazquxbazqux", 12);

    /* get key foobar */
    val = malloc(37);
    memset(val, '\0', 37);
    ck_assert_msg(get_val("foobar", 6, val, 36, 0), "get_val failed for key foobar!");
    ck_assert_msg(strcmp(val, "bazquxbazquxfoobarfoobarbazquxbazqux") == 0, "Wrong value for key foobar! got %s", val);
    free(val);

    /* Prepend key to foobar that requires reallocating (no chaining) */
    prepend_val("foobar", 6, "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 200);

    /* get key foobar */
    val = malloc(237);
    memset(val, '\0', 237);
    ck_assert_msg(get_val("foobar", 6, val, 236, 0), "get_val failed for key foobar!");
    ck_assert_msg(strcmp(val, "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890bazquxbazquxfoobarfoobarbazquxbazqux") == 0, "Wrong value for key foobar! got %s", val);
    free(val);

#if defined CC_CHAINED && CC_CHAINED == 1
    /* Prepend key to foobar that requires reallocating to a chain */
    prepend_val("foobar", 6, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 900);

    /* Get key foobar */
    val = malloc(1137);
    memset(val, '\0', 1137);
    ck_assert_msg(get_val("foobar", 6, val, 1136, 0), "get_val failed for key foobar!");
    ck_assert_msg(strcmp(val, "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890bazquxbazquxfoobarfoobarbazquxbazqux") == 0, "Wrong value for key foobar! got %s", val);
    free(val);


    val = malloc(1501);
    for(i = 0; i < 1000; ++i) {
	/* store large val */
	store_key("large val", 9, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 1500);

	memset(val, '\0', 1501);
	ck_assert_msg(get_val("large val", 9, val, 1500, 0), "get_val failed for key large val!");
	ck_assert_msg(strcmp(val, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890") == 0, "Wrong value for key large val, got %s", val);

	/* remove key large val */
	remove_key("large val", 9);
    }
    free(val);
#endif
}
END_TEST

START_TEST(check_mem_replace)
{
    rstatus_t return_status;
    char *val;

    init_settings();
    time_init();

    return_status = item_init(0);
    if(return_status != CC_OK) {
	ck_abort_msg("Item init failed! Error code %d", return_status);
    }

    return_status = slab_init();
    if(return_status != CC_OK) {
	ck_abort_msg("Slab init failed! Error code %d", return_status);
    }

    store_key("foo", 3, "bar", 3);
    store_key("foobar", 6, "foobarfoobar", 12);

    val = malloc(4);
    ck_assert_msg(get_val("foo", 3, val, 3, 0), "get_val failed for key foo!");
    val[3] = '\0';
    ck_assert_msg(strcmp(val, "bar") == 0, "Wrong value for key foo! Expected bar, got %s", val);
    free(val);

    val = malloc(7);
    replace_key("foo", 3, "foobar", 6);
    ck_assert_msg(get_val("foo", 3, val, 6, 0), "get_val failed for key foo!");
    val[6] = '\0';
    ck_assert_msg(strcmp(val, "foobar") == 0, "Replace unsuccessful! Expected foobar, got %s", val);
    free(val);
}
END_TEST

START_TEST(check_zipmap_basic)
{
    rstatus_t return_status;
    void *val;
    uint32_t nval, len, i, j;
    int ret;
    int64_t num;
    zmap_key_numeric_vector_t numeric_pairs;

    init_settings();
    time_init();

    return_status = item_init(0);
    if(return_status != CC_OK) {
	ck_abort_msg("Item init failed! Error code %d", return_status);
    }

    return_status = slab_init();
    if(return_status != CC_OK) {
	ck_abort_msg("Slab init failed! Error code %d", return_status);
    }

    zmap_init("map", 3);
    printf("map init'd\n");
    ck_assert_msg(zmap_len("map", 3) == 0, "zipmap initialized with incorrect len!");

    /* Set first val */
    printf("setting first val...\n");
    ck_assert_msg(zmap_set("map", 3, "foo", 3, "bar", 3) == ZMAP_SET_OK, "zipmap set not successful!");
    printf("@@@ set successful!\n");
    ck_assert_msg(zmap_len("map", 3) == 1, "zipmap has incorrect len (should be 1)!");

    /* Get first val */
    printf("getting key foo...\n");
    val = cc_alloc(12);
    ck_assert(val != NULL);
    ret = zmap_get("map", 3, "foo", 3, &val, &nval);
    ck_assert_msg(ret == ZMAP_GET_OK, "get not successful! %d", ret);
    ck_assert_msg(cc_memcmp(val, "bar", 3) == 0, "got wrong value!");

    /* Set second val */
    printf("setting second val...\n");
    ck_assert_msg(zmap_set("map", 3, "baz", 3, "qux", 3) == ZMAP_SET_OK, "zipmap set not successful!");
    ck_assert_msg(zmap_len("map", 3) == 2, "zipmap has incorrect len (should be 2)!");

    /* Get second val */
    printf("getting key baz...\n");
    ret = zmap_get("map", 3, "baz", 3, &val, &nval);
    ck_assert_msg(ret == ZMAP_GET_OK, "get not successful! %d", ret);
    ck_assert_msg(cc_memcmp(val, "qux", 3) == 0, "got wrong value!");

    /* Get first val */
    printf("getting key foo...\n");
    ret = zmap_get("map", 3, "foo", 3, &val, &nval);
    ck_assert_msg(ret == ZMAP_GET_OK, "get not successful! %d", ret);
    ck_assert_msg(cc_memcmp(val, "bar", 3) == 0, "got wrong value!");

    /* Set numeric value */
    printf("setting numeric val...\n");
    ck_assert_msg(zmap_set_numeric("map", 3, "n1", 2, 0xFFFFFF) == ZMAP_SET_OK, "zipmap set not successful!");
    len = zmap_len("map", 3);
    ck_assert_msg(len == 3, "zipmap has incorrect len (should be 3 but is %u)!", len);

    /* Get numeric value */
    printf("getting numeric val...\n");
    ret = zmap_get("map", 3, "n1", 2, &val, &nval);
    ck_assert_msg(ret == ZMAP_GET_OK, "get not successful! %d", ret);
    num = *((uint64_t *)val);
    ck_assert_msg(num == 0xFFFFFF, "got incorrect value %lx", num);

    /* Increment numeric value */
    printf("incrementing numeric val...\n");
    ck_assert_msg(zmap_delta("map", 3, "n1", 2, 0xF000000) == ZMAP_DELTA_OK, "zipmap delta not successful!");

    /* Get numeric value */
    printf("getting numeric val...\n");
    ret = zmap_get("map", 3, "n1", 2, &val, &nval);
    ck_assert_msg(ret == ZMAP_GET_OK, "get not successful! %d", ret);
    num = *((int64_t *)val);
    ck_assert_msg(num == 0xFFFFFFF, "got incorrect value %lx", num);

    /* Replace existing val with val of the same size */
    printf("replacing key foo... (same size)\n");
    ret = zmap_replace("map", 3, "foo", 3, "foo", 3);
    ck_assert_msg(ret == ZMAP_REPLACE_OK, "replace not successful! %d", ret);
    ck_assert_msg(zmap_len("map", 3) == 3, "zipmap has incorrect len (should be 3)!");

    /* Get new val */
    printf("getting key foo...\n");
    ret = zmap_get("map", 3, "foo", 3, &val, &nval);
    ck_assert_msg(ret == ZMAP_GET_OK, "get not successful! %d", ret);
    ck_assert_msg(cc_memcmp(val, "foo", 3) == 0, "got wrong value!");

    /* Replace existing val with val of different size */
    printf("replacing key foo... (different size)\n");
    ret = zmap_replace("map", 3, "foo", 3, "foobarfoobar", 12);
    ck_assert_msg(ret == ZMAP_REPLACE_OK, "replace not successful! %d", ret);
    ck_assert_msg(zmap_len("map", 3) == 3, "zipmap has incorrect len (should be 3)!");
    /* Get new val */
    printf("getting key foo...\n");
    ret = zmap_get("map", 3, "foo", 3, &val, &nval);
    ck_assert_msg(ret == ZMAP_GET_OK, "get not successful! %d", ret);
    ck_assert_msg(cc_memcmp(val, "foobarfoobar", 12) == 0, "got wrong value! (got %s)", val);

    /* Replace non existent value (should not go through) */
    printf("replacing key asdf...\n");
    ret = zmap_replace("map", 3, "asdf", 4, "asdfasdf", 8);
    ck_assert_msg(ret == ZMAP_REPLACE_ENTRY_NOT_FOUND, "replace returned incorrect status %d!", ret);
    ck_assert_msg(zmap_len("map", 3) == 3, "zipmap has incorrect len (should be 3)!");

    /* Add new val */
    printf("adding val...");
    ret = zmap_add("map", 3, "foobar", 6, "foobarbazqux", 12);
    ck_assert_msg(ret == ZMAP_ADD_OK, "add not successful! %d", ret);
    ck_assert_msg(zmap_len("map", 3) == 4, "zipmap has incorrect len (should be 4)!");

    /* Get new val */
    printf("getting key foobar...\n");
    ret = zmap_get("map", 3, "foobar", 6, &val, &nval);
    ck_assert_msg(ret == ZMAP_GET_OK, "get not successful! %d", ret);
    ck_assert_msg(cc_memcmp(val, "foobarbazqux", 12) == 0, "got wrong value! (got %s)", val);

    /* Add existing val (should not go through) */
    printf("adding key foo...\n");
    ret = zmap_add("map", 3, "foo", 3, "bar", 3);
    ck_assert_msg(ret == ZMAP_ADD_EXISTS, "add returned incorrect status %d!", ret);
    ck_assert_msg(zmap_len("map", 3) == 4, "zipmap has incorrect len (should be 4)!");

    /* Replace key foo with numeric value */
    printf("replacing key foo...\n");
    ret = zmap_replace_numeric("map", 3, "foo", 3, 0xDEADBEEF);
    ck_assert_msg(ret == ZMAP_REPLACE_OK, "replace returned incorrect status %d!", ret);
    ck_assert_msg(zmap_len("map", 3) == 4, "zipmap has incorrect len (should be 4)!");

    /* Get new val */
    printf("getting numeric val...\n");
    ret = zmap_get("map", 3, "foo", 3, &val, &nval);
    ck_assert_msg(ret == ZMAP_GET_OK, "get not successful! &d", ret);
    num = *((int64_t *)val);
    ck_assert_msg(num == 0xDEADBEEF, "got incorrect value %lx", num);

#if defined CC_CHAINED && CC_CHAINED == 1/* Add many new values, so that chaining will be required to hold them all */
    printf("setting 200 numeric vals...\n");
    numeric_pairs.key_numeric_pairs = malloc(200 * sizeof(struct key_numeric_pair));
    ck_assert(numeric_pairs.key_numeric_pairs != NULL);
    numeric_pairs.len = 200;

    for(i = 0; i < 200; ++i) {
	numeric_pairs.key_numeric_pairs[i].key = malloc(7);
	numeric_pairs.key_numeric_pairs[i].nkey = sprintf(numeric_pairs.key_numeric_pairs[i].key, "num%u", i);
	numeric_pairs.key_numeric_pairs[i].val = i;
    }

    for(i = 0; i < 100; ++i) {
	ret = zmap_set_multiple_numeric("map", 3, &numeric_pairs);
	ck_assert_msg(ret == ZMAP_SET_OK, "set not successful! %d", ret);
	ck_assert_msg(zmap_len("map", 3) == 204, "zipmap has incorrect len (should be 204)!");

	for(j = 0; j < 200; ++j) {
	    ret = zmap_delete("map", 3, numeric_pairs.key_numeric_pairs[j].key, strlen(numeric_pairs.key_numeric_pairs[j].key));
	    ck_assert_msg(ret == ZMAP_DELETE_OK, "delete not successful! error code %d on key %s keylen %hhu", ret, j);
	}
	ck_assert_msg(zmap_len("map", 3) == 4, "zipmap has incorrect len (should be 4)!");
    }
    for(i = 0; i < 200; ++i) {
	free(numeric_pairs.key_numeric_pairs[i].key);
    }
    free(numeric_pairs.key_numeric_pairs);

    /* Add many new values, so that chaining will be required to hold them all */
    printf("setting 200 numeric vals...\n");
    numeric_pairs.key_numeric_pairs = malloc(200 * sizeof(struct key_numeric_pair));
    ck_assert(numeric_pairs.key_numeric_pairs != NULL);
    numeric_pairs.len = 200;
    for(i = 0; i < 200; ++i) {
    	numeric_pairs.key_numeric_pairs[i].key = malloc(7);
    	numeric_pairs.key_numeric_pairs[i].nkey = sprintf(numeric_pairs.key_numeric_pairs[i].key, "num%u", i);
    	numeric_pairs.key_numeric_pairs[i].val = i;
    }
    ret = zmap_set_multiple_numeric("map", 3, &numeric_pairs);
    ck_assert_msg(ret == ZMAP_SET_OK, "set not successful! %d", ret);
    ck_assert_msg(zmap_len("map", 3) == 204, "zipmap has incorrect len (should be 204)!");
    for(i = 0; i < 200; ++i) {
    	free(numeric_pairs.key_numeric_pairs[i].key);
    }
    free(numeric_pairs.key_numeric_pairs);

    /* replace node in beginning with larger one */
    printf("setting key baz...\n");
    ck_assert_msg(zmap_set("map", 3, "baz", 3, "asdfasdfasdfasdfasdfasdfasdfasdfasdfasdf", 3) == ZMAP_SET_OK, "zipmap set not successful!");
    ck_assert_msg(zmap_len("map", 3) == 204, "zipmap has incorrect len (should be 204)!");

    /* delete node */
    printf("deleting key num100\n");
    ret = zmap_delete("map", 3, "num100", 6);
    ck_assert_msg(ret == ZMAP_DELETE_OK, "zipmap delete not successful! %d", ret);
    ck_assert_msg(zmap_len("map", 3) == 203, "zipmap has incorrect len (should be 203)!");
#endif
}
END_TEST

START_TEST(check_mem_evict)
{
    rstatus_t return_status;
    uint32_t i, n;
    char *val, *key;

    init_settings();
    time_init();

    return_status = item_init(0);
    if(return_status != CC_OK) {
	ck_abort_msg("Item init failed! Error code %d", return_status);
    }

    return_status = slab_init();
    if(return_status != CC_OK) {
	ck_abort_msg("Slab init failed! Error code %d", return_status);
    }

    val = malloc(7);
    for(i = 0; i < 1000; ++i) {
	n = sprintf(val, "val%u", i);
	store_key(val, n, val, n);
    }
    free(val);

    key = malloc(14);
    for(i = 0; i < 1000; ++i) {
	/* store large val */
	n = sprintf(key, "large val %u", i);
	printf("@@@ storing key %u\n", i);
	store_key(key, n, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 1500);
    }
    free(key);
}
END_TEST

Suite *
mem_suite(void)
{
    Suite *s = suite_create("mem");

    TCase *tc_core = tcase_create("Core");
    //tcase_add_test(tc_core, check_mem_basic);
    //tcase_add_test(tc_core, check_mem_replace);
    //tcase_add_test(tc_core, check_zipmap_basic);
    tcase_add_test(tc_core, check_mem_evict);
    tcase_set_timeout(tc_core, 60);
    suite_add_tcase(s, tc_core);

    return s;
}

int
main(void)
{
    int number_failed;
    Suite *s = mem_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return(number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
