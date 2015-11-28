#include <channel/cc_tcp.h>

#include <check.h>

#include <stdlib.h>
#include <stdio.h>

#define SUITE_NAME "tcp"
#define DEBUG_LOG  SUITE_NAME ".log"

/*
 * utilities
 */
static void
test_setup(void)
{
    tcp_setup(0, NULL);
}

static void
test_teardown(void)
{
    tcp_teardown();
}

static void
test_reset(void)
{
    test_teardown();
    test_setup();
}

static void
find_port_listen(struct tcp_conn **_conn_listen, struct addrinfo **_ai, uint16_t *_port)
{
    uint16_t port = 9001;
    char servname[CC_UINTMAX_MAXLEN + 1];
    struct tcp_conn *conn_listen, *conn_client;
    struct addrinfo hints, *ai;

    test_reset();

    conn_listen = tcp_conn_create();
    ck_assert_ptr_ne(conn_listen, NULL);

    conn_client = tcp_conn_create();
    ck_assert_ptr_ne(conn_client, NULL);

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    for (;;) {
        sprintf(servname, "%"PRIu32, port);
        ck_assert_int_eq(getaddrinfo("localhost", servname, &hints, &ai), 0);
        if (tcp_connect(ai, conn_client)) {
            // port is in use by other process
            tcp_close(conn_client);
            port++;
            continue;
        }

        ck_assert_int_eq(getaddrinfo("localhost", servname, &hints, &ai), 0);
        if (tcp_listen(ai, conn_listen)) {
            break;
        }
    }
    /* for some reason this line is needed, I would appreciate some insight */
    ck_assert_int_eq(tcp_connect(ai, conn_client), true);

    if (_conn_listen) {
        *_conn_listen = conn_listen;
    }
    if (_ai) {
        *_ai = ai;
    }
    if (_port) {
        *_port = port;
    }
    tcp_close(conn_client);
    tcp_conn_destroy(&conn_client);
}

START_TEST(test_listen_connect)
{
    struct tcp_conn *conn_listen, *conn_client;
    struct addrinfo *ai;

    find_port_listen(&conn_listen, &ai, NULL);

    conn_client = tcp_conn_create();
    ck_assert_ptr_ne(conn_client, NULL);

    ck_assert_int_eq(tcp_connect(ai, conn_client), true);

    tcp_close(conn_listen);
    tcp_close(conn_client);

    tcp_conn_destroy(&conn_listen);
    tcp_conn_destroy(&conn_client);
}
END_TEST

START_TEST(test_listen_listen)
{
    struct tcp_conn *conn_listen1, *conn_listen2;
    struct addrinfo *ai;

    test_reset();

    find_port_listen(&conn_listen1, &ai, NULL);

    conn_listen2 = tcp_conn_create();
    ck_assert_ptr_ne(conn_listen2, NULL);

    ck_assert_int_eq(tcp_listen(ai, conn_listen2), false);

    tcp_close(conn_listen1);

    tcp_conn_destroy(&conn_listen1);
    tcp_conn_destroy(&conn_listen2);
}
END_TEST

/*
 * test suite
 */
static Suite *
log_suite(void)
{
    Suite *s = suite_create(SUITE_NAME);
    TCase *tc_log = tcase_create("tcp test");
    suite_add_tcase(s, tc_log);

    tcase_add_test(tc_log, test_listen_connect);
    tcase_add_test(tc_log, test_listen_listen);

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
