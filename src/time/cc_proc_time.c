#include <time/cc_proc_time.h>

#include <time/cc_timer.h>

#include <cc_debug.h>

time_t time_start;
rel_time_t now_sec;
rel_time_fine_t now_ms;
rel_time_fine_t now_us;
rel_time_fine_t now_ns;

static struct duration start;
static struct duration now;

void
time_update(void)
{
    duration_snapshot(&now, &start);

    __atomic_store_n(&now_sec, (rel_time_t)duration_sec(&now), __ATOMIC_RELAXED);
    __atomic_store_n(&now_ms, (rel_time_fine_t)duration_ms(&now), __ATOMIC_RELAXED);
    __atomic_store_n(&now_us, (rel_time_fine_t)duration_us(&now), __ATOMIC_RELAXED);
    __atomic_store_n(&now_ns, (rel_time_fine_t)duration_ns(&now), __ATOMIC_RELAXED);
}

void
time_setup(void)
{
    time_start = time(NULL);
    duration_start(&start);
    time_update();

    log_info("timer started at %"PRIu64, (uint64_t)time_start);
}

void
time_teardown(void)
{
    duration_reset(&start);
    duration_reset(&now);

    log_info("timer ended at %"PRIu64, (uint64_t)time(NULL));
}
