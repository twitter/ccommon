#include <cc_metric.h>

#include <check.h>

#include <stdlib.h>
#include <stdio.h>

#define SUITE_NAME "metric"
#define DEBUG_LOG  SUITE_NAME ".log"

#define TEST_METRIC(ACTION)                                \
    ACTION( c,       METRIC_COUNTER, "# counter"    )\
    ACTION( g,       METRIC_GAUGE,   "# gauge"      )\
    ACTION( v,       METRIC_FPN,     "value"        )

typedef struct {
        TEST_METRIC(METRIC_DECLARE)
} test_metrics_st;

test_metrics_st *test_metrics = NULL;

#define TEST_METRIC_INIT(_metrics) do {                            \
    *(_metrics) = (test_metrics_st) { TEST_METRIC(METRIC_INIT) }; \
} while(0)

/*
 * utilities
 */
static void
test_setup(void)
{
    metric_setup();
    test_metrics = malloc(sizeof(test_metrics_st));
    TEST_METRIC_INIT(test_metrics);
}

static void
test_teardown(void)
{
    free(test_metrics);
    metric_teardown();
}

static void
test_reset(void)
{
    test_teardown();
    test_setup();
}

START_TEST(test_counter)
{
    test_reset();
    ck_assert_int_eq(test_metrics->c.counter, 0);
    INCR(test_metrics, c);
    ck_assert_int_eq(test_metrics->c.counter, 1);
    INCR_N(test_metrics, c, 2);
    ck_assert_int_eq(test_metrics->c.counter, 3);
    UPDATE_VAL(test_metrics, c, 2);
    ck_assert_int_eq(test_metrics->c.counter, 2);
    DECR(test_metrics, c);
    ck_assert_int_eq(test_metrics->c.counter, 2);
}
END_TEST

START_TEST(test_gauge)
{
    test_reset();
    ck_assert_int_eq(test_metrics->g.counter, 0);
    INCR(test_metrics, g);
    ck_assert_int_eq(test_metrics->g.counter, 1);
    INCR_N(test_metrics, g, 2);
    ck_assert_int_eq(test_metrics->g.counter, 3);
    UPDATE_VAL(test_metrics, g, 2);
    ck_assert_int_eq(test_metrics->g.counter, 2);
    DECR(test_metrics, g);
    ck_assert_int_eq(test_metrics->g.counter, 1);
    DECR_N(test_metrics, g, 5);
    ck_assert_int_eq(test_metrics->g.counter, -4);
}
END_TEST

/*
 * test suite
 */
static Suite *
metric_suite(void)
{
    Suite *s = suite_create(SUITE_NAME);

    /* basic requests */
    TCase *tc_metric = tcase_create("cc_metric test");
    suite_add_tcase(s, tc_metric);

    tcase_add_test(tc_metric, test_counter);
    tcase_add_test(tc_metric, test_gauge);

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

    Suite *suite = metric_suite();
    SRunner *srunner = srunner_create(suite);
    srunner_set_log(srunner, DEBUG_LOG);
    srunner_run_all(srunner, CK_ENV); /* set CK_VEBOSITY in ENV to customize */
    nfail = srunner_ntests_failed(srunner);
    srunner_free(srunner);

    /* teardown */
    test_teardown();

    return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
