#include <cc_histogram.h>

#include <check.h>

#include <float.h>
#include <stdlib.h>
#include <stdio.h>

#define SUITE_NAME "histogram"
#define DEBUG_LOG  SUITE_NAME ".log"

#define PARRAY_SIZE 7
const double parray[PARRAY_SIZE] = {0.25, 0.5, 0.75, 0.9, 0.95, 0.99, 0.999};

/*
 * utilities
 */
static void
test_setup(void)
{
}

static void
test_teardown(void)
{
}

/*
 * tests
 */
START_TEST(test_histo_create_destroy)
{
#define m 1
#define r 10
#define n 20
    struct histo_u32 *histo = histo_u32_create(m, r, n);

    ck_assert(histo != NULL);
    ck_assert_int_eq(histo->M, 1 << m);
    ck_assert_int_eq(histo->R, (1 << r) - 1);
    ck_assert_int_eq(histo->N, (1 << n) - 1);
    ck_assert_int_eq(histo->G, (1 << (r -m - 1)));
    ck_assert_int_eq(histo->nbucket, (n - r + 2) * histo->G);
    histo_u32_destroy(&histo);
    ck_assert(histo == NULL);
#undef n
#undef r
#undef m
}
END_TEST

START_TEST(test_percentile_basic)
{
    struct percentile_profile *pp = percentile_profile_create(PARRAY_SIZE * 2);

    ck_assert(pp != NULL);
    ck_assert_int_eq(pp->cap, PARRAY_SIZE * 2);
    ck_assert_int_eq(pp->count, 0);

    percentile_profile_set(pp, parray, PARRAY_SIZE);
    ck_assert_int_eq(pp->count, PARRAY_SIZE);
    for (int count = 0; count < PARRAY_SIZE; count++) {
        ck_assert(fabs(*(pp->percentile + count) - parray[count]) < DBL_EPSILON);
    }

    percentile_profile_destroy(&pp);
    ck_assert(pp == NULL);

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
    TCase *tc_histogram = tcase_create("cc_histogram test");
    suite_add_tcase(s, tc_histogram);

    tcase_add_test(tc_histogram, test_histo_create_destroy);
    tcase_add_test(tc_histogram, test_percentile_basic);

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
