#include <cc_log.h>

#include <check.h>

#include <stdlib.h>
#include <stdio.h>

#define SUITE_NAME "log"
#define DEBUG_LOG  SUITE_NAME ".log"

static log_metrics_st metrics;
/*
 * utilities
 */
static void
test_setup(void)
{
    log_setup(&metrics);
}

static void
test_teardown(void)
{
    log_teardown();
}

static void
test_reset(void)
{
    test_teardown();
    test_setup();
}

static char *
tmpname_create()
{
#define PATH "/tmp/temp.XXXXXX"
    char *path = malloc(sizeof(PATH) + 3);
    strcpy(path, PATH);
    mkdtemp(path);
    size_t len = strlen(path);
    path[len++] = '/';
    path[len++] = '1';
    path[len++] = 0;
    return path;
#undef PATH
}

static void
tmpname_destroy(char *path)
{
    unlink(path);
    path[strlen(path) - 2] = 0;
    rmdir(path);
    free(path);
}

static void
assert_file_contents(const char *tmpname, const char *str, size_t len)
{
    char *filedata = malloc(sizeof(char) * len);
    FILE *fp = fopen(tmpname, "r");
    ck_assert_ptr_ne(fp, NULL);

    ck_assert_uint_eq(len, fread(filedata, sizeof(char), len + 1, fp));
    ck_assert_int_eq(memcmp(filedata, str, len * sizeof(char)), 0);

    fclose(fp);
    free(filedata);
}

START_TEST(test_create_write_destroy)
{
#define LOGSTR "foo bar baz"
    struct logger *logger = NULL;
    char *tmpname = tmpname_create();

    test_reset();

    logger = log_create(tmpname, 0);
    ck_assert_ptr_ne(logger, NULL);

    ck_assert_int_eq(_log_write(logger, LOGSTR, sizeof(LOGSTR) - 1), 1);

    log_destroy(&logger);
    ck_assert_ptr_eq(logger, NULL);

    assert_file_contents(tmpname, LOGSTR, sizeof(LOGSTR) - 1);

    tmpname_destroy(tmpname);
#undef LOGSTR
}
END_TEST

/*
 * test suite
 */
static Suite *
log_suite(void)
{
    Suite *s = suite_create(SUITE_NAME);
    TCase *tc_log = tcase_create("log test");
    suite_add_tcase(s, tc_log);

    tcase_add_test(tc_log, test_create_write_destroy);

    return s;
}
/**************
 * test cases *
 **************/

int
main(void)
{
    int nfail;

    /* setup */
    test_setup();

    Suite *suite = log_suite();
    SRunner *srunner = srunner_create(suite);
    srunner_set_log(srunner, DEBUG_LOG);
    srunner_run_all(srunner, CK_ENV); /* set CK_VEBOSITY in ENV to customize */
    nfail = srunner_ntests_failed(srunner);
    srunner_free(srunner);

    /* teardown */
    test_teardown();

    return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
