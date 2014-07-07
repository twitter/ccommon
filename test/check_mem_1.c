#include <check.h>

#include "../src/mem/cc_mem_interface.h"
#include "../src/mem/cc_settings.h"
#include "../src/mem/cc_slabs.h"
#include "../src/mem/cc_items.h"
#include "../src/cc_define.h"
#include "../src/hash/cc_hash_table.h"

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
    bytes = writev(STDOUT_FILENO, vector[0].iov_base, 12);
    ck_assert_msg(bytes == 12, "Wrote incorrect number of bytes! nbytes: %d errno: %d", bytes, errno);
    free(vector);

/*
    val = get_val("foobar", 6);
    ck_assert_msg(strcmp(val, "foobarfoobar") == 0, "Wrong value for key foobar! Expected foobarfoobar, got %s", val);
    free(val);
*/
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

Suite *
mem_suite(void)
{
    Suite *s = suite_create("mem");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, check_mem_basic);
    tcase_add_test(tc_core, check_mem_replace);
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
