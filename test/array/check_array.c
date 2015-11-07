#include <cc_array.h>

#include <check.h>

#include <stdlib.h>
#include <stdio.h>

#define SUITE_NAME "array"
#define DEBUG_LOG  SUITE_NAME ".log"

/*
 * utilities
 */
static void
test_setup(void)
{
}

static void
test_reset(void)
{
}

static void
test_teardown(void)
{
}

/*
 * test suite
 */
static Suite *
array_suite(void)
{
    Suite *s = suite_create(SUITE_NAME);

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

    Suite *suite = array_suite();
    SRunner *srunner = srunner_create(suite);
    srunner_set_log(srunner, DEBUG_LOG);
    srunner_run_all(srunner, CK_ENV); /* set CK_VEBOSITY in ENV to customize */
    nfail = srunner_ntests_failed(srunner);
    srunner_free(srunner);

    /* teardown */
    test_teardown();

    return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
