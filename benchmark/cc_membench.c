#include <mem/cc_mem_interface.h>
#include <mem/cc_settings.h>
#include <mem/cc_slabs.h>
#include <mem/cc_items.h>
#include <mem/cc_zipmap.h>
#include <cc_define.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <cc_string.h>
#include <hash/cc_hash_table.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mach/mach_time.h>

#define KB (1024)
#define MB (1024 * (KB))
#define ORWL_NANO (+1.0E-9)
#define ORWL_GIGA UINT64_C(1000000000)

void init_settings(void);
struct timespec orwl_gettime(void);
void fill_cache(void);
void empty_cache(void);
void flush_cache(void);
void set_benchmark(void);
void get_set_benchmark(uint32_t nval, uint32_t sample_size, bool full_cache, bool chained);
void get_ref_benchmark(uint32_t sample_size);
void remove_benchmark(uint32_t sample_size);
void add_benchmark(uint32_t sample_size);
void replace_benchmark(uint32_t sample_size);
void print_times(uint32_t *times, uint32_t ntimes);
int compare_int(const void *a, const void *b);
void get_time_stats(uint32_t *times, uint32_t ntimes);

int
main()
{
    rstatus_t return_status;

    init_settings();
    time_init();

    if(log_init(LOG_WARN, "out.txt") == -1) {
	log_stderr("fatal: log_init failed!");
	return 1;
    }

    return_status = item_init(20);
    if(return_status != CC_OK) {
	log_stderr("fatal: item_init failed!");
	return 1;
    }

    return_status = slab_init();
    if(return_status != CC_OK) {
	log_stderr("fatal: slab_init failed!");
	return 1;
    }

    set_benchmark();

    return 0;
}

void init_settings(void) {
    settings.prealloc = true;
    settings.evict_lru = true;
    settings.use_freeq = true;
    settings.use_cas = false;
    settings.maxbytes = 2 * 1024 * (MB + SLAB_HDR_SIZE);
    settings.slab_size = MB + SLAB_HDR_SIZE;
    settings.profile[1] = 128;
    settings.profile[2] = 256;
    settings.profile[3] = 512;
    settings.profile[4] = KB;
    settings.profile[5] = 2 * KB;
    settings.profile[6] = 4 * KB;
    settings.profile[7] = 8 * KB;
    settings.profile[8] = 16 * KB;
    settings.profile[9] = 32 * KB;
    settings.profile[10] = 64 * KB;
    settings.profile[11] = 128 * KB;
    settings.profile[12] = 256 * KB;
    settings.profile[13] = 512 * KB;
    settings.profile[14] = MB;
    settings.profile_last_id = 14;
    settings.oldest_live = 6000;
}

static double orwl_timebase = 0.0;
static uint64_t orwl_timestart = 0;

struct timespec
orwl_gettime(void)
{
    // be more careful in a multithreaded environment
    if (!orwl_timestart) {
	mach_timebase_info_data_t tb = { 0 };
	mach_timebase_info(&tb);
	orwl_timebase = tb.numer;
	orwl_timebase /= tb.denom;
	orwl_timestart = mach_absolute_time();
    }
    struct timespec t;
    double diff = (mach_absolute_time() - orwl_timestart) * orwl_timebase;
    t.tv_sec = diff * ORWL_NANO;
    t.tv_nsec = diff - (t.tv_sec * ORWL_GIGA);
    return t;
}

/* Fills the cache with items */
void
fill_cache(void)
{
    uint32_t i;
    uint8_t nkey;
    char key[20];
    char *val;

    val = cc_alloc(KB * 1000);

    if(val == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark");
	exit(1);
    }

    cc_memset(val, 0xff, KB * 1000);

    for(i = 0; i < 2048; ++i) {
	nkey = sprintf(key, "fill%u", i);
	store_key(key, nkey, val, KB * 1000);
    }

    free(val);
}

/* Empties cache filled via fill_cache */
void
empty_cache(void)
{
    uint32_t i;
    uint8_t nkey;
    char key[20];

    for(i = 0; i < 2048; ++i) {
	nkey = sprintf(key, "fill%u", i);
	remove_key(key, nkey);
    }
}

void flush_cache(void)
{
    fill_cache();
    empty_cache();
}

void
set_benchmark(void)
{
    printf("-------------------- Set Benchmarks --------------------\n");
    printf("Benchmark 1: Setting 4 byte values in empty cache\n");
    get_set_benchmark(4, 10000, false, false);
    printf("Benchmark 2: Setting 1 KB values in empty cache\n");
    get_set_benchmark(KB, 5000, false, false);
    printf("Benchmark 3: Setting 1 MB values in empty cache\n");
    get_set_benchmark(MB, 2000, false, false);
}

void
get_set_benchmark(uint32_t nval, uint32_t sample_size, bool full_cache, bool chained)
{
    uint32_t i;
    uint32_t *times;
    struct timespec before, after;
    uint8_t nkey;
    char key[20];
    char *val;

    times = cc_alloc(sample_size * sizeof(uint32_t));
    val = cc_alloc(nval);

    if(times == NULL || val == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark");
	exit(1);
    }

    cc_memset(val, 0xff, nval);

    if(full_cache) {
	if(chained) {
	    /* fill cache with chained items */
	} else {
	    fill_cache();
	}
    }

    for(i = 0; i < sample_size; ++i) {
	nkey = sprintf(key, "%d", i);

	before = orwl_gettime();
	store_key(key, nkey, val, nval);
	after = orwl_gettime();

	times[i] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
    }

    get_time_stats(times, sample_size);

    flush_cache();

    cc_free(val);
    cc_free(times);
}

void
get_ref_benchmark(uint32_t sample_size)
{
    uint32_t i;
    uint32_t *times;
    struct timespec before, after;
    uint8_t nkey;
    char key[20];

    printf("getting get_ref benchmarks\n");

    times = cc_alloc(sample_size * sizeof(uint32_t));

    if(times == NULL) {
	log_stderr("cannot benchmark; not enough memory");
	exit(1);
    }

    for(i = 0; i < sample_size + 1000; ++i) {
	struct iovec vector;
	nkey = sprintf(key, "key%d", i);

	before = orwl_gettime();
	get_val_ref(key, nkey, &vector);
	after = orwl_gettime();

	if(i >= 1000) {
	    times[i - 1000] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
	}
    }

    get_time_stats(times, sample_size);

    cc_free(times);
}

void
remove_benchmark(uint32_t sample_size)
{
    uint32_t i;
    uint32_t *times;
    struct timespec before, after;
    uint8_t nkey;
    char key[20];

    printf("getting remove benchmarks\n");

    times = cc_alloc(sample_size * sizeof(uint32_t));

    if(times == NULL) {
	log_stderr("cannot benchmark; not enough memory");
	exit(1);
    }

    for(i = 0; i < sample_size + 1000; ++i) {
	nkey = sprintf(key, "key%d", i);

	before = orwl_gettime();
	remove_key(key, nkey);
	after = orwl_gettime();

	if(i >= 1000) {
	    times[i - 1000] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
	}
    }

    get_time_stats(times, sample_size);

    cc_free(times);
}

void
add_benchmark(uint32_t sample_size)
{
    uint32_t i;
    uint32_t *times;
    struct timespec before, after;
    uint8_t nkey;
    char key[20];

    printf("getting add benchmarks\n");

    times = cc_alloc(sample_size * sizeof(uint32_t));

    if(times == NULL) {
	log_stderr("cannot benchmark; not enough memory");
	exit(1);
    }

    for(i = 0; i < sample_size + 1000; ++i) {
	nkey = sprintf(key, "key%d", i);

	before = orwl_gettime();
	add_key(key, nkey, "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 100);
	after = orwl_gettime();

	if(i >= 1000) {
	    times[i - 1000] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
	}
    }

    get_time_stats(times, sample_size);

    cc_free(times);
}

void
replace_benchmark(uint32_t sample_size)
{
    uint32_t i;
    uint32_t *times;
    struct timespec before, after;
    uint8_t nkey;
    char key[20];

    printf("getting replace benchmarks\n");

    times = cc_alloc(sample_size * sizeof(uint32_t));

    if(times == NULL) {
	log_stderr("cannot benchmark; not enough memory");
	exit(1);
    }

    for(i = 0; i < sample_size + 1000; ++i) {
	nkey = sprintf(key, "key%d", i);

	before = orwl_gettime();
	replace_key(key, nkey, "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 100);
	after = orwl_gettime();

	if(i >= 1000) {
	    times[i - 1000] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
	}
    }

    get_time_stats(times, sample_size);

    cc_free(times);
}

void
print_times(uint32_t *times, uint32_t ntime)
{
    uint32_t i;

    for(i = 0; i < ntime; ++i) {
	loga("%u\n", times[i]);
    }
}

int
compare_int(const void *a, const void *b)
{
    return (*(int *)a - *(int *)b);
}

void
get_time_stats(uint32_t *times, uint32_t ntimes)
{
    qsort(times, ntimes, sizeof(uint32_t), compare_int);
    printf("sample size: %u\nall times in nanoseconds\nmin: %u\n25th percentile: %u\n"
	   "50th percentile: %u\n75th percentile: %u\n90th percentile: %u\n"
	   "95th percentile: %u\n99th percentile: %u\n99.9th percentile: %u\n"
	   "max: %u\n\n", ntimes, times[0], times[(int)((double)ntimes * 0.25)],
	   times[(int)((double)ntimes * 0.5)], times[(int)((double)ntimes * 0.75)],
	   times[(int)((double)ntimes * 0.9)], times[(int)((double)ntimes * 0.95)],
	   times[(int)((double)ntimes * 0.99)], times[(int)((double)ntimes * 0.999)],
	   times[ntimes - 1]);
}
