#include <cc_rbuf.h>

#include <check.h>

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#define SUITE_NAME "rbuf"
#define DEBUG_LOG  SUITE_NAME ".log"

#define ARRAY_MAX_NELEM_DELTA 8

/*
 * utilities
 */
static void
test_setup(void)
{
    rbuf_setup(NULL);
}

static void
test_teardown(void)
{
    rbuf_teardown();
}

static void
test_reset(void)
{
    test_teardown();
    test_setup();
}

START_TEST(test_create_write_read_destroy)
{
#define W1_LEN 8
#define W2_LEN 12
#define CAP (W1_LEN + W2_LEN)
    size_t i, written, read;
    char write_data[CAP], read_data[CAP];
    struct rbuf *buffer;

    test_reset();

    for (i = 0; i < CAP; i++) {
        write_data[i] = i % CHAR_MAX;
    }

    buffer = rbuf_create(CAP);
    ck_assert_ptr_ne(buffer, NULL);

    written = rbuf_write(buffer, write_data, W1_LEN);
    ck_assert_int_eq(written, W1_LEN);

    ck_assert_int_eq(rbuf_rcap(buffer), W1_LEN);
    ck_assert_int_eq(rbuf_wcap(buffer), W2_LEN);


    written = rbuf_write(buffer, &write_data[W1_LEN], W2_LEN);
    ck_assert_int_eq(written, W2_LEN);

    ck_assert_int_eq(rbuf_rcap(buffer), CAP);
    ck_assert_int_eq(rbuf_wcap(buffer), 0);

    read = rbuf_read(read_data, buffer, W1_LEN);
    ck_assert_int_eq(read, W1_LEN);

    read = rbuf_read(&read_data[W1_LEN], buffer, W2_LEN);
    ck_assert_int_eq(read, W2_LEN);

    ck_assert_int_eq(memcmp(read_data, write_data, CAP), 0);

    rbuf_destroy(buffer);
#undef CAP
#undef W2_LEN
#undef W1_LEN
}
END_TEST

/*
 * test suite
 */
static Suite *
rbuf_suite(void)
{
    Suite *s = suite_create(SUITE_NAME);

    /* basic requests */
    TCase *tc_rbuf = tcase_create("cc_rbuf test");
    suite_add_tcase(s, tc_rbuf);

    tcase_add_test(tc_rbuf, test_create_write_read_destroy);

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

    Suite *suite = rbuf_suite();
    SRunner *srunner = srunner_create(suite);
    srunner_set_log(srunner, DEBUG_LOG);
    srunner_run_all(srunner, CK_ENV); /* set CK_VEBOSITY in ENV to customize */
    nfail = srunner_ntests_failed(srunner);
    srunner_free(srunner);

    /* teardown */
    test_teardown();

    return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
