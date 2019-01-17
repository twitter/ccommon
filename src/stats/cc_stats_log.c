#include <cc_stats_log.h>

#include <cc_debug.h>
#include <cc_log.h>
#include <cc_metric.h>

#define STATS_LOG_MODULE_NAME "util::stats_log"
#define CSV_FMT "%s, "
#define PRINT_BUF_LEN 64

static struct logger *slog = NULL;
static bool stats_log_init = false;

static char buf[PRINT_BUF_LEN];


void
stats_log_setup(stats_log_options_st *options)
{
    size_t log_nbuf = STATS_LOG_NBUF;
    char *filename = STATS_LOG_FILE;

    log_info("set up the %s module", STATS_LOG_MODULE_NAME);

    if (stats_log_init) {
        log_warn("%s has already been setup, overwrite", STATS_LOG_MODULE_NAME);
        if (slog != NULL) {
            log_destroy(&slog);
        }
    }

    if (options != NULL) {
        filename = option_str(&options->stats_log_file);
        log_nbuf = option_uint(&options->stats_log_nbuf);
    }

    if (filename != NULL) {
        slog = log_create(filename, log_nbuf);
        if (slog == NULL) {
            log_warn("Could not create logger");
        }
    }

    stats_log_init = true;
}

void
stats_log_teardown(void)
{
    log_info("tear down the %s module", STATS_LOG_MODULE_NAME);

    if (!stats_log_init) {
        log_warn("%s has never been setup", STATS_LOG_MODULE_NAME);
    }

    if (slog != NULL) {
        log_destroy(&slog);
    }

    stats_log_init = false;
}

void
stats_log_name(struct metric metrics[], unsigned int nmetric)
{
    unsigned int i;

    for (i = 0; i < nmetric; i++, metrics++) {
        int len = 0;

        len = metric_print_name(buf, PRINT_BUF_LEN, CSV_FMT, metrics);
        log_write(slog, buf, len);
    }
}

void
stats_log_value(struct metric metrics[], unsigned int nmetric)
{
    unsigned int i;

    for (i = 0; i < nmetric; i++, metrics++) {
        int len = 0;

        len = metric_print_value(buf, PRINT_BUF_LEN, CSV_FMT, metrics);
        log_write(slog, buf, len);
    }
}


void
stats_log_flush(void)
{
    log_flush(slog);
}

