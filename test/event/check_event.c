#include <cc_event.h>

#include <check.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define SUITE_NAME "event"
#define DEBUG_LOG  SUITE_NAME ".log"

struct event {
    void *arg;
    uint32_t events;
};

static struct event event_log[1024];
static uint32_t event_log_count;

/*
 * utilities
 */
static void
test_setup(void)
{
    event_log_count = 0;
    event_setup(NULL);
}

static void
test_teardown(void)
{
    event_teardown();
}

static void
test_reset(void)
{
    test_teardown();
    test_setup();
}

static char *
tmpname_create(void)
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
log_event(void *arg, uint32_t events)
{
    event_log[event_log_count].arg = arg;
    event_log[event_log_count++].events = events;
}

START_TEST(test_read)
{
    FILE *fp;
    struct event_base *event_base;
    char *tmpname = tmpname_create();
    char *data = "foo bar baz";
    int random_pointer[1] = {1};

    test_reset();

    event_base = event_base_create(1024, log_event);

    fp = fopen(tmpname, "w");
    fwrite(data, 1, sizeof(data), fp);
    fclose(fp);

    fp = fopen(tmpname, "r");
    event_add_read(event_base, fileno(fp), random_pointer);

    ck_assert_int_eq(event_log_count, 0);

    event_wait(event_base, -1);

    ck_assert_int_eq(event_log_count, 1);
    ck_assert_ptr_eq(event_log[0].arg, random_pointer);
    ck_assert_int_eq(event_log[0].events, EVENT_READ);

    fclose(fp);
    event_base_destroy(&event_base);
    tmpname_destroy(tmpname);
}
END_TEST

START_TEST(test_cannot_read)
{
    FILE *fp;
    struct event_base *event_base;
    char *tmpname = tmpname_create();
    int random_pointer[1] = {1};

    test_reset();

    event_base = event_base_create(1024, log_event);

    fp = fopen(tmpname, "w");
    fclose(fp);

    fp = fopen(tmpname, "r");
    event_add_read(event_base, fileno(fp), random_pointer);

    ck_assert_int_eq(event_log_count, 0);

    event_wait(event_base, 1000);

    ck_assert_int_eq(event_log_count, 0);

    event_base_destroy(&event_base);
    tmpname_destroy(tmpname);
}
END_TEST

/*
 * test suite
 */
static Suite *
event_suite(void)
{
    Suite *s = suite_create(SUITE_NAME);

    /* basic requests */
    TCase *tc_event = tcase_create("cc_event test");
    suite_add_tcase(s, tc_event);

    tcase_add_test(tc_event, test_read);
    tcase_add_test(tc_event, test_cannot_read);

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

    Suite *suite = event_suite();
    SRunner *srunner = srunner_create(suite);
    srunner_set_log(srunner, DEBUG_LOG);
    srunner_run_all(srunner, CK_ENV); /* set CK_VEBOSITY in ENV to customize */
    nfail = srunner_ntests_failed(srunner);
    srunner_free(srunner);

    /* teardown */
    test_teardown();

    return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
