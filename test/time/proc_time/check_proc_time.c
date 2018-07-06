#include <time/cc_proc_time.h>

#include <check.h>

#include <math.h>
#include <stdlib.h>
#include <time.h>

#define SUITE_NAME "proc_time"
#define DEBUG_LOG  SUITE_NAME ".log"

#define NSEC_PER_USEC  1000L
#define NSEC_PER_MSEC  1000000L
#define NSEC_PER_SEC   1000000000L

/*
 * utilities
 */
static void
test_setup(void)
{
    time_setup();
}

static void
test_teardown(void)
{
    time_teardown();
}

static void
test_reset(void)
{
    test_teardown();
    test_setup();
}

/*
 * tests
 */
START_TEST(test_short_duration)
{
#define DURATION_NS 100000

    rel_time_t s_before, s_after;
    rel_time_fine_t ms_before, ms_after, us_before, us_after, ns_before, ns_after;
    struct timespec ts = (struct timespec){0, DURATION_NS};

    test_reset();

    time_update();
    s_before = time_now();
    ms_before = time_now_ms();
    us_before = time_now_us();
    ns_before = time_now_ns();

    nanosleep(&ts, NULL);

    time_update();
    s_after = time_now();
    ms_after = time_now_ms();
    us_after = time_now_us();
    ns_after = time_now_ns();

    /* duration is as expected */
    ck_assert_uint_ge((unsigned int)(ns_after - ns_before), DURATION_NS);
    ck_assert_uint_ge((unsigned int)(us_after - us_before),
            DURATION_NS / NSEC_PER_USEC);
    ck_assert_uint_ge((unsigned int)(ms_after - ms_before),
            DURATION_NS / NSEC_PER_MSEC);
    ck_assert_uint_ge((unsigned int)(s_after - s_before),
            DURATION_NS / NSEC_PER_SEC);

#undef DURATION_NSEC
}
END_TEST

START_TEST(test_long_duration)
{
#define DURATION_S 2

    rel_time_t s_before, s_after;
    rel_time_fine_t ms_before, ms_after, us_before, us_after, ns_before, ns_after;
    struct timespec ts = (struct timespec){DURATION_S, 0};

    test_reset();

    time_update();
    s_before = time_now();
    ms_before = time_now_ms();
    us_before = time_now_us();
    ns_before = time_now_ns();

    nanosleep(&ts, NULL);

    time_update();
    s_after = time_now();
    ms_after = time_now_ms();
    us_after = time_now_us();
    ns_after = time_now_ns();

    /* duration is as expected */
    ck_assert_uint_ge((unsigned int)(ns_after - ns_before), DURATION_NS);
    ck_assert_uint_ge((unsigned int)(us_after - us_before),
            DURATION_NS / NSEC_PER_USEC);
    ck_assert_uint_ge((unsigned int)(ms_after - ms_before),
            DURATION_NS / NSEC_PER_MSEC);
    ck_assert_uint_ge((unsigned int)(s_after - s_before),
            DURATION_NS / NSEC_PER_SEC);

#undef DURATION_S
}
END_TEST

START_TEST(test_start_time)
{

    test_reset();

    time_update();

    /* check if time_started and time_now are correct on start */
    ck_assert_int_le(labs(time_started() - time(NULL)), 1);
    ck_assert_int_le(labs(time_now_abs() - time(NULL)), 1);
    ck_assert_int_le(time_now(), 1);

}
END_TEST

/*
 * test suite
 */
static Suite *
proc_time_suite(void)
{
    Suite *s = suite_create(SUITE_NAME);

    TCase *tc_duration = tcase_create("proc_time duration test");
    suite_add_tcase(s, tc_duration);
    tcase_add_test(tc_duration, test_short_duration);
    tcase_add_test(tc_duration, test_long_duration);

    TCase *tc_start = tcase_create("proc_time start time test");
    suite_add_tcase(s, tc_start);
    tcase_add_test(tc_start, test_start_time);

    return s;
}

int
main(void)
{
    int nfail;

    /* setup */
    test_setup();

    Suite *suite = proc_time_suite();
    SRunner *srunner = srunner_create(suite);
    srunner_set_log(srunner, DEBUG_LOG);
    srunner_run_all(srunner, CK_ENV); /* set CK_VEBOSITY in ENV to customize */
    nfail = srunner_ntests_failed(srunner);
    srunner_free(srunner);

    /* teardown */
    test_teardown();

    return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
