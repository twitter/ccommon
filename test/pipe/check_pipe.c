#include <channel/cc_pipe.h>

#include <check.h>

#include <stdlib.h>
#include <stdio.h>

#define SUITE_NAME "pipe"
#define DEBUG_LOG  SUITE_NAME ".log"

/*
 * utilities
 */
static void
test_setup(void)
{
    pipe_setup(NULL);
}

static void
test_teardown(void)
{
    pipe_teardown();
}

static void
test_reset(void)
{
    test_teardown();
    test_setup();
}

START_TEST(test_send_recv)
{
    struct pipe_conn *pipe;
    const char *write_message = "foo bar baz";
#define READ_MESSAGE_LENGTH 12
    char read_message[READ_MESSAGE_LENGTH];
    test_reset();

    pipe = pipe_conn_create();
    ck_assert_ptr_ne(pipe, NULL);

    ck_assert_int_eq(pipe_open(NULL, pipe), true);
    ck_assert_int_eq(pipe_send(pipe, (void *)write_message, READ_MESSAGE_LENGTH), READ_MESSAGE_LENGTH);

    ck_assert_int_eq(pipe_recv(pipe, read_message, READ_MESSAGE_LENGTH), READ_MESSAGE_LENGTH);

    ck_assert_str_eq(write_message, read_message);

    pipe_close(pipe);
    pipe_conn_destroy(&pipe);
#undef READ_MESSAGE_LENGTH
}
END_TEST

/*
 * test suite
 */
static Suite *
pipe_suite(void)
{
    Suite *s = suite_create(SUITE_NAME);

    TCase *tc_pipe = tcase_create("pipe test");
    tcase_add_test(tc_pipe, test_send_recv);
    suite_add_tcase(s, tc_pipe);

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

    Suite *suite = pipe_suite();
    SRunner *srunner = srunner_create(suite);
    srunner_set_log(srunner, DEBUG_LOG);
    srunner_run_all(srunner, CK_ENV); /* set CK_VEBOSITY in ENV to customize */
    nfail = srunner_ntests_failed(srunner);
    srunner_free(srunner);

    /* teardown */
    test_teardown();

    return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
