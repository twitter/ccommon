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
    settings.evict_lru = true;
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

    store_key("foo", 3, "bar", 3);
    store_key("foobar", 6, "foobarfoobar", 12);

    val = malloc(4);
    ck_assert_msg(get_val("foo", 3, val, 3, 0), "get_val failed for key foo!");
    val[3] = '\0';
    ck_assert_msg(strcmp(val, "bar") == 0, "Wrong value for key foo! Expected bar, got %s", val);
    free(val);

    vector = malloc(sizeof(struct iovec));
    ck_assert_msg(get_val_ref("foobar", 6, vector), "get_val_ref failed for key foobar!");
    bytes = writev(STDOUT_FILENO, vector, 1);
    ck_assert_msg(bytes == 12, "Wrote incorrect number of bytes! nbytes: %d errno: %d", bytes, errno);
    free(vector);
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
    uint32_t nval, len;
    int ret;
    int64_t num;

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
    ck_assert_msg(zmap_len("map", 3) == 1, "zipmap has incorrect len (should be 1)!");

    /* Get first val */
    printf("getting key foo...\n");
    val = cc_alloc(3);
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
    num = *((uint64_t *)val);
    ck_assert_msg(num == 0xFFFFFFF, "got incorrect value %lx", num);
}
END_TEST

Suite *
mem_suite(void)
{
    Suite *s = suite_create("mem");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, check_mem_basic);
    tcase_add_test(tc_core, check_mem_replace);
    tcase_add_test(tc_core, check_zipmap_basic);
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
