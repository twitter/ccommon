#include <mem/cc_mem_interface.h>
#include <mem/cc_settings.h>
#include <mem/cc_slab.h>
#include <mem/cc_item.h>
#include <data_structure/cc_zipmap.h>
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
#define FILL_DEFAULT_NVAL (1000 * (KB))
#define FILL_DEFAULT_NUM 2048

void init_benchmark(void);
void init_settings(void);
struct timespec orwl_gettime(void);
void fill_cache_default(void);
void fill_cache(uint32_t nval, uint32_t num);
void fill_cache_numeric(void);
void empty_cache(void);
void flush_cache(void);
void fill_zipmap(void *pkey, uint8_t npkey, uint32_t nval, uint32_t num);
void empty_zipmap(void *pkey, uint8_t npkey);

void cc_alloc_benchmark(void);
void set_benchmark(void);
void add_benchmark(void);
void replace_benchmark(void);
void append_benchmark(void);
void prepend_benchmark(void);
void delta_benchmark(void);
void get_benchmark(void);
void remove_benchmark(void);
void zmap_get_benchmark(void);

void zmap_benchmark(void);
void zmap_set_benchmark(void);
void zmap_add_benchmark(void);
void zmap_replace_benchmark(void);
void zmap_delete_benchmark(void);

void get_cc_alloc_benchmark(uint32_t nbyte, uint32_t sample_size);
void get_type1_benchmark(void (*fn)(void *, uint8_t, void *, uint32_t), uint32_t nval, uint32_t sample_size, bool full_cache, bool existing_key);
void get_type2_benchmark(int (*fn)(void *, uint8_t, void *, uint8_t, void*, uint32_t), void *pkey, uint8_t npkey, uint32_t nval, uint32_t num, uint32_t sample_size, bool existing_key);
void get_delta_benchmark(void (*fn)(void *, uint8_t, uint64_t), uint64_t delta, uint32_t sample_size);
void get_getref_benchmark(uint32_t nval, uint32_t sample_size);
void get_getval_benchmark(uint32_t nval, uint32_t sample_size);
void get_remove_benchmark(uint32_t nval, uint32_t sample_size);
void get_zipmap_replace_benchmark(void *pkey, uint8_t npkey, uint32_t nval, uint32_t num, uint32_t new_nval, uint32_t sample_size);
void get_zipmap_delete_benchmark(void *pkey, uint8_t npkey, uint32_t nval, uint32_t num, uint32_t sample_size);
void get_zipmap_get_benchmark(void *pkey, uint8_t npkey, uint32_t nval, uint32_t num, uint32_t sample_size, uint32_t index);

void print_times(uint32_t *times, uint32_t ntimes);
int compare_int(const void *a, const void *b);
void get_time_stats(uint32_t *times, uint32_t ntimes);

uint16_t nbenchmark = 0;

int
main()
{
    init_benchmark();

    cc_alloc_benchmark();

    /* set_benchmark(); */
    /* add_benchmark(); */
    /* replace_benchmark(); */
    /* append_benchmark(); */
    /* prepend_benchmark(); */
    /* delta_benchmark(); */
    /* get_benchmark(); */
    /* remove_benchmark(); */
    zmap_benchmark();

    return 0;
}

void
init_benchmark(void)
{
    settings_load("benchmark.config");
    time_init();

    if(log_init(LOG_WARN, "out.txt") == -1) {
	log_stderr("fatal: log_init failed!");
	exit(1);
    }

    if(item_init() != CC_OK) {
	log_stderr("fatal: item_init failed!");
	exit(1);
    }

    if(item_hash_init(20) != CC_OK) {
	log_stderr("fatal: item_hash_init failed!");
	exit(1);
    }

    if(slab_init() != CC_OK) {
	log_stderr("fatal: slab_init failed!");
	exit(1);
    }

    flush_cache();
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
fill_cache_default(void)
{
    fill_cache(FILL_DEFAULT_NVAL, FILL_DEFAULT_NUM);
}

/* Fills cache with items with specified parameters */
void
fill_cache(uint32_t nval, uint32_t num)
{
    uint32_t i;
    uint8_t nkey;
    char key[20];
    char *val;

    val = cc_alloc(nval);

    if(val == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark");
	exit(1);
    }

    cc_memset(val, 0xff, nval);

    for(i = 0; i < num; ++i) {
	nkey = sprintf(key, "%u", i);
	store_key(key, nkey, val, nval);
    }

    cc_free(val);
}

/* Fills cache (not completely full) with numeric items */
void
fill_cache_numeric(void)
{
    uint32_t i;
    uint8_t nkey;
    char key[20];

    for(i = 0; i < 1000000; ++i) {
	nkey = sprintf(key, "%u", i);
	store_key(key, nkey, key, nkey);
    }
}

/* Empties cache filled via fill_cache */
void
empty_cache(void)
{
    uint32_t i;
    uint8_t nkey;
    char key[20];

    for(i = 0; i < 2048; ++i) {
	nkey = sprintf(key, "%u", i);
	remove_key(key, nkey);
    }
}

void flush_cache(void)
{
    fill_cache_default();
    empty_cache();
}

/* Fills zipmap with num entries with nval entry value size */
void
fill_zipmap(void *pkey, uint8_t npkey, uint32_t nval, uint32_t num)
{
    uint32_t i;
    uint8_t nskey;
    char skey[20];
    char *val;

    val = cc_alloc(nval);

    if(val == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark!");
	exit(1);
    }

    cc_memset(val, 0xff, nval);

    for(i = 0; i < num; ++i) {
	nskey = sprintf(skey, "%u", i);
	zmap_set(pkey, npkey, skey, nskey, val, nval);
    }

    cc_free(val);
}

/* empties the zipmap with the given key. */
void
empty_zipmap(void *pkey, uint8_t npkey)
{
    void *pkey_cpy;

    pkey_cpy = cc_alloc(npkey);

    if(pkey_cpy == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark!");
	exit(1);
    }

    cc_memcpy(pkey_cpy, pkey, npkey);

    remove_key(pkey, npkey);
    zmap_init(pkey_cpy, npkey);

    cc_free(pkey_cpy);
}

void
cc_alloc_benchmark(void)
{
    printf("-------------------- cc_alloc Benchmarks (for comparison) --------------------\n");
    printf("Benchmark %hu: alloc 4 byte values\n", ++nbenchmark);
    get_cc_alloc_benchmark(4, 10000);
    printf("Benchmark %hu: alloc 1 KB values\n", ++nbenchmark);
    get_cc_alloc_benchmark(KB, 5000);
    printf("Benchmark %hu: alloc 1000 KB values\n", ++nbenchmark);
    get_cc_alloc_benchmark(1000 * KB, 2000);
    printf("Benchmark %hu: alloc 2 MB values\n", ++nbenchmark);
    get_cc_alloc_benchmark(2 * MB, 1000);
}

void
set_benchmark(void)
{
    printf("-------------------- Set Benchmarks --------------------\n");
    printf("Benchmark %hu: Setting 4 byte values in empty cache\n", ++nbenchmark);
    get_type1_benchmark(&store_key, 4, 10000, false, false);
    printf("Benchmark %hu: Setting 1 KB values in empty cache\n", ++nbenchmark);
    get_type1_benchmark(&store_key, KB, 5000, false, false);
    printf("Benchmark %hu: Setting 1000 KB values in empty cache\n", ++nbenchmark);
    get_type1_benchmark(&store_key, 1000 * KB, 2000, false, false);
    printf("Benchmark %hu: Setting 2 MB values (chained) in empty cache\n",
	   ++nbenchmark);
    get_type1_benchmark(&store_key, 2 * MB, 1000, false, false);
    printf("Benchmark %hu: Setting 4 byte values in full cache\n", ++nbenchmark);
    get_type1_benchmark(&store_key, 4, 10000, true, false);
    printf("Benchmark %hu: Setting 1 KB values in full cache\n", ++nbenchmark);
    get_type1_benchmark(&store_key, KB, 5000, true, false);
    printf("Benchmark %hu: Setting 1000 KB values in full cache\n", ++nbenchmark);
    get_type1_benchmark(&store_key, 1000 * KB, 2000, true, false);
    printf("Benchmark %hu: Setting 2 MB values (chained) in empty cache\n",
	   ++nbenchmark);
    get_type1_benchmark(&store_key, 2 * MB, 1000, true, false);
}

void
add_benchmark(void)
{
    printf("-------------------- Add Benchmarks --------------------\n");
    printf("Benchmark %hu: Setting 4 byte values in empty cache\n", ++nbenchmark);
    get_type1_benchmark(&add_key, 4, 10000, false, false);
    printf("Benchmark %hu: Setting 1 KB values in empty cache\n", ++nbenchmark);
    get_type1_benchmark(&add_key, KB, 5000, false, false);
    printf("Benchmark %hu: Setting 1000 KB values in empty cache\n", ++nbenchmark);
    get_type1_benchmark(&add_key, 1000 * KB, 2000, false, false);
    printf("Benchmark %hu: Setting 2 MB values (chained) in empty cache\n",
	   ++nbenchmark);
    get_type1_benchmark(&add_key, 2 * MB, 1000, false, false);
    printf("Benchmark %hu: Setting 4 byte values in full cache\n", ++nbenchmark);
    get_type1_benchmark(&add_key, 4, 10000, true, false);
    printf("Benchmark %hu: Setting 1 KB values in full cache\n", ++nbenchmark);
    get_type1_benchmark(&add_key, 1000 * KB, 5000, true, false);
    printf("Benchmark %hu: Setting 1000 KB values in full cache\n", ++nbenchmark);
    get_type1_benchmark(&add_key, MB, 2000, true, false);
    printf("Benchmark %hu: Setting 2 MB values (chained) in full cache\n",
	   ++nbenchmark);
    get_type1_benchmark(&add_key, 2 * MB, 1000, true, false);
}

void
replace_benchmark(void)
{
    printf("-------------------- Replace Benchmarks --------------------\n");
    printf("Benchmark %hu: Replacing to 4 byte values in full cache\n", ++nbenchmark);
    get_type1_benchmark(&replace_key, 4, 10000, true, true);
    printf("Benchmark %hu: Replacing to 1 KB values in full cache\n", ++nbenchmark);
    get_type1_benchmark(&replace_key, KB, 5000, true, true);
    printf("Benchmark %hu: Replacing to 1000 KB values in full cache\n",
	   ++nbenchmark);
    get_type1_benchmark(&replace_key, 1000 * KB, 2000, true, true);
    printf("Benchmark %hu: Replacing to 2 MB values (chained) in full cache\n",
	   ++nbenchmark);
    get_type1_benchmark(&replace_key, 2 * MB, 1000, true, true);
}

void
append_benchmark(void)
{
    printf("-------------------- Append Benchmarks --------------------\n");
    printf("Benchmark %hu: Appending 4 bytes in full cache\n", ++nbenchmark);
    get_type1_benchmark(&append_val, 4, 10000, true, true);
    printf("Benchmark %hu: Appending 1 KB in full cache\n", ++nbenchmark);
    get_type1_benchmark(&append_val, KB, 5000, true, true);
    printf("Benchmark %hu: Appending 1000 KB in full cache\n", ++nbenchmark);
    get_type1_benchmark(&append_val, 1000 * KB, 2000, true, true);
}

void
prepend_benchmark(void)
{
    printf("-------------------- Prepend Benchmarks --------------------\n");
    printf("Benchmark %hu: Prepending 4 bytes in full cache\n", ++nbenchmark);
    get_type1_benchmark(&prepend_val, 4, 10000, true, true);
    printf("Benchmark %hu: Prepending 1 KB in full cache\n", ++nbenchmark);
    get_type1_benchmark(&prepend_val, KB, 5000, true, true);
    printf("Benchmark %hu: Prepending 1000 KB in full cache\n", ++nbenchmark);
    get_type1_benchmark(&prepend_val, 1000 * KB, 2000, true, true);
}

void
delta_benchmark(void)
{
    printf("-------------------- Delta Benchmarks --------------------\n");
    printf("Benchmark %hu: Incrementing\n", ++nbenchmark);
    get_delta_benchmark(&increment_val, 1, 10000);
    printf("Benchmark %hu: Decrementing\n", ++nbenchmark);
    get_delta_benchmark(&decrement_val, 1, 10000);
}

void
get_benchmark(void)
{
    printf("-------------------- Get Benchmarks --------------------\n");
    printf("Benchmark %hu: Get by reference - 4 byte values\n", ++nbenchmark);
    get_getref_benchmark(4, 10000);
    printf("Benchmark %hu: Get by reference - 1 KB values\n", ++nbenchmark);
    get_getref_benchmark(KB, 5000);
    printf("Benchmark %hu: Get by reference - 1000 KB values\n", ++nbenchmark);
    get_getref_benchmark(1000 * KB, 2000);
    printf("Benchmark %hu: Get by reference - 2 MB values (chained)\n",
	   ++nbenchmark);
    get_getref_benchmark(2 * MB, 1000);
    printf("Benchmark %hu: Get by value - 4 byte values\n", ++nbenchmark);
    get_getval_benchmark(4, 10000);
    printf("Benchmark %hu: Get by value - 1 KB values\n", ++nbenchmark);
    get_getval_benchmark(KB, 5000);
    printf("Benchmark %hu: Get by value - 1000 KB values\n", ++nbenchmark);
    get_getval_benchmark(1000 * KB, 2000);
    printf("Benchmark %hu: get by value - 2 MB values (chained)\n", ++nbenchmark);
    get_getval_benchmark(2 * MB, 1000);
}

void
remove_benchmark(void)
{
    printf("-------------------- Remove Benchmarks --------------------\n");
    printf("Benchmark %hu: Removing 4 byte values\n", ++nbenchmark);
    get_remove_benchmark(4, 10000);
    printf("Benchmark %hu: Removing 1 KB values\n", ++nbenchmark);
    get_remove_benchmark(KB, 5000);
    printf("Benchmark %hu: Removing 1000 KB values\n", ++nbenchmark);
    get_remove_benchmark(1000 * KB, 2000);
    printf("Benchmark %hu: Removing 2 MB values\n", ++nbenchmark);
    get_remove_benchmark(2 * MB, 1000);
}

void
zmap_benchmark(void)
{
    zmap_init("zmap", 4);
    /* zmap_set_benchmark(); */
    /* zmap_add_benchmark(); */
    /* zmap_replace_benchmark(); */
    /* zmap_delete_benchmark(); */
    zmap_get_benchmark();
}

void
zmap_set_benchmark(void)
{
    printf("-------------------- Zimap Set Benchmarks --------------------\n");
    printf("Benchmark %hu: Setting 4 byte value to empty zipmap\n", ++nbenchmark);
    get_type2_benchmark(&zmap_set, "zmap", 4, 4, 0, 10000, false);
    printf("Benchmark %hu: Setting 1 KB value to empty zipmap\n", ++nbenchmark);
    get_type2_benchmark(&zmap_set, "zmap", 4, KB, 0, 5000, false);
    printf("Benchmark %hu: Setting 100 KB value to empty zipmap\n", ++nbenchmark);
    get_type2_benchmark(&zmap_set, "zmap", 4, 100 * KB, 0, 3000, false);
    printf("Benchmark %hu: Setting 4 byte value to zipmap with 100 items\n",
    	   ++nbenchmark);
    get_type2_benchmark(&zmap_set, "zmap", 4, 4, 100, 5000, false);
    printf("Benchmark %hu: Setting 1 KB value to zipmap with 100 items\n",
    	   ++nbenchmark);
    get_type2_benchmark(&zmap_set, "zmap", 4, KB, 100, 2500, false);
    printf("Benchmark %hu: Setting 100 KB value to zipmap with 100 items\n",
    	   ++nbenchmark);
    get_type2_benchmark(&zmap_set, "zmap", 4, 100 * KB, 100, 1000, false);
    printf("Benchmark %hu: Setting 4 byte value to zipmap with 1000 items\n",
    	   ++nbenchmark);
    get_type2_benchmark(&zmap_set, "zmap", 4, 4, 1000, 5000, false);
    printf("Benchmark %hu: Setting 1 KB value to zipmap with 1000 items\n",
    	   ++nbenchmark);
    get_type2_benchmark(&zmap_set, "zmap", 4, KB, 1000, 2500, false);
    printf("Benchmark %hu: Setting 100 KB value to zipmap with 1000 items\n",
    	   ++nbenchmark);
    get_type2_benchmark(&zmap_set, "zmap", 4, 100 * KB, 1000, 1000, false);
}

void
zmap_add_benchmark(void)
{
    printf("-------------------- Zimap Add Benchmarks --------------------\n");
    printf("Benchmark %hu: Adding 4 byte value to empty zipmap\n", ++nbenchmark);
    get_type2_benchmark(&zmap_add, "zmap", 4, 4, 0, 10000, false);
    printf("Benchmark %hu: Adding 1 KB value to empty zipmap\n", ++nbenchmark);
    get_type2_benchmark(&zmap_add, "zmap", 4, KB, 0, 5000, false);
    printf("Benchmark %hu: Adding 100 KB value to empty zipmap\n", ++nbenchmark);
    get_type2_benchmark(&zmap_add, "zmap", 4, 100 * KB, 0, 3000, false);
    printf("Benchmark %hu: Adding 4 byte value to zipmap with 100 items\n",
    	   ++nbenchmark);
    get_type2_benchmark(&zmap_add, "zmap", 4, 4, 100, 5000, false);
    printf("Benchmark %hu: Adding 1 KB value to zipmap with 100 items\n",
    	   ++nbenchmark);
    get_type2_benchmark(&zmap_add, "zmap", 4, KB, 100, 2500, false);
    printf("Benchmark %hu: Adding 100 KB value to zipmap with 100 items\n",
    	   ++nbenchmark);
    get_type2_benchmark(&zmap_add, "zmap", 4, 100 * KB, 100, 1000, false);
    printf("Benchmark %hu: Adding 4 byte value to zipmap with 1000 items\n",
    	   ++nbenchmark);
    get_type2_benchmark(&zmap_add, "zmap", 4, 4, 1000, 5000, false);
    printf("Benchmark %hu: Adding 1 KB value to zipmap with 1000 items\n",
    	   ++nbenchmark);
    get_type2_benchmark(&zmap_add, "zmap", 4, KB, 1000, 2500, false);
    printf("Benchmark %hu: Adding 100 KB value to zipmap with 1000 items\n",
    	   ++nbenchmark);
    get_type2_benchmark(&zmap_add, "zmap", 4, 100 * KB, 1000, 1000, false);
}

void
zmap_replace_benchmark(void)
{
    printf("-------------------- Zimap Replace Benchmarks --------------------\n");
    printf("Benchmark %hu: Replacing 4 byte value with equal size to zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_replace_benchmark("zmap", 4, 4, 100, 4, 10000);
    printf("Benchmark %hu: Replacing 1 KB value with equal size to zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_replace_benchmark("zmap", 4, KB, 100, KB, 5000);
    printf("Benchmark %hu: Replacing 100 KB value with equal size to zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_replace_benchmark("zmap", 4, 100 * KB, 100, 100 * KB, 3000);
    printf("Benchmark %hu: Replacing 4 byte value with equal size to zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_replace_benchmark("zmap", 4, 4, 1000, 4, 5000);
    printf("Benchmark %hu: Replacing 1 KB value with equal size to zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_replace_benchmark("zmap", 4, KB, 1000, KB, 2500);
    printf("Benchmark %hu: Replacing 100 KB value with equal size to zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_replace_benchmark("zmap", 4, 100 * KB, 1000, 100 * KB, 1000);
    printf("Benchmark %hu: Replacing 4 byte value with 8 byte value to zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_replace_benchmark("zmap", 4, 4, 100, 8, 5000);
    printf("Benchmark %hu: Replacing 1 KB value with 2 KB value to zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_replace_benchmark("zmap", 4, KB, 100, 2 * KB, 2500);
    printf("Benchmark %hu: Replacing 100 KB value with 200 KB value to zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_replace_benchmark("zmap", 4, 100 * KB, 100, 200 * KB, 1000);
    printf("Benchmark %hu: Replacing 4 byte value with 8 byte value to zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_replace_benchmark("zmap", 4, 4, 1000, 8, 5000);
    printf("Benchmark %hu: Replacing 1 KB value with 2 KB value to zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_replace_benchmark("zmap", 4, KB, 1000, 2 * KB, 2500);
    printf("Benchmark %hu: Replacing 100 KB value with 200 KB value to zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_replace_benchmark("zmap", 4, 100 * KB, 1000, 200 * KB, 1000);
}

void
zmap_delete_benchmark(void)
{
    printf("-------------------- Zipmap Delete Benchmarks --------------------\n");
    printf("Benchmark %hu: Deleting 4 byte value from zipmap with 100 items\n",
	   ++nbenchmark);
    get_zipmap_delete_benchmark("zmap", 4, 4, 100, 10000);
    printf("Benchmark %hu: Deleting 1 KB value from zipmap with 100 items\n",
	   ++nbenchmark);
    get_zipmap_delete_benchmark("zmap", 4, KB, 100, 5000);
    printf("Benchmark %hu: Deleting 100 KB value from zipmap with 100 items\n",
	   ++nbenchmark);
    get_zipmap_delete_benchmark("zmap", 4, 100 * KB, 100, 3000);
    printf("Benchmark %hu: Deleting 4 byte value from zipmap with 1000 items\n",
	   ++nbenchmark);
    get_zipmap_delete_benchmark("zmap", 4, 4, 1000, 5000);
    printf("Benchmark %hu: Deleting 1 KB value from zipmap with 1000 items\n",
	   ++nbenchmark);
    get_zipmap_delete_benchmark("zmap", 4, KB, 1000, 2500);
    printf("Benchmark %hu: Deleting 100 KB value from zipmap with 1000 items\n",
	   ++nbenchmark);
    get_zipmap_delete_benchmark("zmap", 4, 100 * KB, 1000, 1000);
}

void
zmap_get_benchmark(void)
{
    printf("-------------------- Zipmap Get Benchmarks --------------------\n");
    printf("Benchmark %hu: Getting 4 byte value from beginning of zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 100, 10000, 0);
    printf("Benchmark %hu: Getting 1 KB value from beginning of zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, KB, 100, 5000, 0);
    printf("Benchmark %hu: Getting 100 KB value from beginning of zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 100 * KB, 100, 3000, 0);



    printf("Benchmark %hu: Getting 4 byte value from beginning of zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 1000, 5000, 0);
    printf("Benchmark %hu: Getting 1 KB value from beginning of zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, KB, 1000, 2500, 0);
    printf("Benchmark %hu: Getting 100 KB value from beginning of zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 100 * KB, 1000, 1000, 0);



    printf("Benchmark %hu: Getting 4 byte value from middle of zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 100, 10000, 49);
    printf("Benchmark %hu: Getting 1 KB value from middle of zipmap with 100 items\n",
	   ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, KB, 100, 5000, 49);
    printf("Benchmark %hu: Getting 100 KB value from middle of zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 100 * KB, 100, 3000, 49);



    printf("Benchmark %hu: Getting 4 byte value from middle of zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 1000, 5000, 499);
    printf("Benchmark %hu: Getting 1 KB value from middle of zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, KB, 1000, 2500, 499);
    printf("Benchmark %hu: Getting 100 KB value from middle of zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 100 * KB, 1000, 1000, 499);



    printf("Benchmark %hu: Getting 4 byte value from end of zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 100, 10000, 99);
    printf("Benchmark %hu: Getting 1 KB value from end of zipmap with 100 items\n",
	   ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, KB, 100, 5000, 99);
    printf("Benchmark %hu: Getting 100 KB value from end of zipmap with 100 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 100 * KB, 100, 3000, 99);



    printf("Benchmark %hu: Getting 4 byte value from end of zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 1000, 5000, 999);
    printf("Benchmark %hu: Getting 1 KB value from end of zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, KB, 1000, 2500, 999);
    printf("Benchmark %hu: Getting 100 KB value from end of zipmap with 1000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 100 * KB, 1000, 1000, 999);

    printf("-------------------- Zipmap Get Benchmarks: num items --------------------\n");
    printf("Benchmark %hu: Getting 4 byte value from beginning of zipmap with 20 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 20, 10000, 0);
    printf("Benchmark %hu: Getting 4 byte value from middle of zipmap with 20 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 20, 10000, 9);
    printf("Benchmark %hu: Getting 4 byte value from end of zipmap with 20 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 20, 10000, 19);
    printf("Benchmark %hu: Getting 4 byte value from beginning of zipmap with 200 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 200, 5000, 0);
    printf("Benchmark %hu: Getting 4 byte value from middle of zipmap with 200 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 200, 5000, 99);
    printf("Benchmark %hu: Getting 4 byte value from end of zipmap with 200 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 200, 5000, 199);
    printf("Benchmark %hu: Getting 4 byte value from beginning of zipmap with 500 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 500, 5000, 0);
    printf("Benchmark %hu: Getting 4 byte value from middle of zipmap with 500 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 500, 5000, 249);
    printf("Benchmark %hu: Getting 4 byte value from end of zipmap with 500 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 500, 5000, 499);
    printf("Benchmark %hu: Getting 4 byte value from beginning of zipmap with 2000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 2000, 3000, 0);
    printf("Benchmark %hu: Getting 4 byte value from middle of zipmap with 2000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 2000, 3000, 999);
    printf("Benchmark %hu: Getting 4 byte value from end of zipmap with 2000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 2000, 3000, 1999);
    printf("Benchmark %hu: Getting 4 byte value from beginning of zipmap with 5000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 5000, 3000, 0);
    printf("Benchmark %hu: Getting 4 byte value from middle of zipmap with 5000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 5000, 3000, 2499);
    printf("Benchmark %hu: Getting 4 byte value from end of zipmap with 5000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 5000, 3000, 4999);
    printf("Benchmark %hu: Getting 4 byte value from beginning of zipmap with 20000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 20000, 2000, 0);
    printf("Benchmark %hu: Getting 4 byte value from middle of zipmap with 20000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 20000, 2000, 9999);
    printf("Benchmark %hu: Getting 4 byte value from end of zipmap with 20000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 20000, 2000, 19999);
    printf("Benchmark %hu: Getting 4 byte value from beginning of zipmap with 50000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 50000, 2000, 0);
    printf("Benchmark %hu: Getting 4 byte value from middle of zipmap with 50000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 50000, 2000, 24999);
    printf("Benchmark %hu: Getting 4 byte value from end of zipmap with 50000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 50000, 2000, 49999);
    printf("Benchmark %hu: Getting 4 byte value from beginning of zipmap with 200000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 200000, 1000, 0);
    printf("Benchmark %hu: Getting 4 byte value from middle of zipmap with 200000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 200000, 1000, 99999);
    printf("Benchmark %hu: Getting 4 byte value from end of zipmap with 200000 items\n", ++nbenchmark);
    get_zipmap_get_benchmark("zmap", 4, 4, 200000, 1000, 199999);
}

void
get_cc_alloc_benchmark(uint32_t nbyte, uint32_t sample_size)
{
    uint32_t i;
    uint32_t *times;
    void **vals;
    void *val_cpy;

    times = cc_alloc(sample_size * sizeof(uint32_t));
    vals = cc_alloc(sample_size * sizeof(void *));
    val_cpy = cc_alloc(nbyte);

    if(times == NULL || vals == NULL || val_cpy == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark");
	exit(1);
    }

    cc_memset(val_cpy, 0xff, nbyte);

    for(i = 0; i < sample_size; ++i) {
	struct timespec before, after;

	before = orwl_gettime();
	vals[i] = cc_alloc(nbyte);
	cc_memcpy(vals[i], val_cpy, nbyte);
	after = orwl_gettime();

	times[i] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
    }

    get_time_stats(times, sample_size);

    for(i = 0; i < sample_size; ++i) {
	cc_free(vals[i]);
    }

    cc_free(val_cpy);
    cc_free(vals);
    cc_free(times);
}

void
get_type1_benchmark(void (*fn)(void *, uint8_t, void *, uint32_t), uint32_t nval, uint32_t sample_size, bool full_cache, bool existing_key)
{
    uint32_t i;
    uint32_t *times;
    char *val;

    times = cc_alloc(sample_size * sizeof(uint32_t));
    val = cc_alloc(nval);

    if(times == NULL || val == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark");
	exit(1);
    }

    cc_memset(val, 0xff, nval);

    if(full_cache) {
	fill_cache_default();
    }

    for(i = 0; i < sample_size; ++i) {
	struct timespec before, after;
	char key[20];
	uint8_t nkey;

	if(existing_key) {
	    nkey = sprintf(key, "%u", i);
	} else {
	    nkey = sprintf(key, "k%u", i);
	}

	before = orwl_gettime();
	fn(key, nkey, val, nval);
	after = orwl_gettime();

	times[i] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
    }

    get_time_stats(times, sample_size);

    flush_cache();

    cc_free(val);
    cc_free(times);
}

/* Calls fn sample_size times and retrieves runtime statistics. Calls fn for the
   zipmap with the given pkey with num items of value size nval initially
   in the zmap. Calls fn with values of size nval. Uses keys already existing in
   the zipmap if existing_key is true, and does not otherwise. */
void
get_type2_benchmark(int (*fn)(void *, uint8_t, void *, uint8_t, void*, uint32_t), void *pkey, uint8_t npkey, uint32_t nval, uint32_t num, uint32_t sample_size, bool existing_key)
{
    uint32_t i;
    uint32_t *times;
    char *val;

    times = cc_alloc(sample_size * sizeof(uint32_t));
    val = cc_alloc(nval);

    if(times == NULL || val == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark");
	exit(1);
    }

    cc_memset(val, 0xff, nval);

    for(i = 0; i < sample_size; ++i) {
	struct timespec before, after;
	char skey[20];
	uint8_t nskey;

	if(existing_key) {
	    nskey = sprintf(skey, "%u", i);
	} else {
	    nskey = sprintf(skey, "k%u", i);
	}

	fill_zipmap(pkey, npkey, nval, num);

	before = orwl_gettime();
	fn(pkey, npkey, skey, nskey, val, nval);
	after = orwl_gettime();

	empty_zipmap(pkey, npkey);

	times[i] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
    }

    get_time_stats(times, sample_size);

    cc_free(val);
    cc_free(times);
}

void
get_delta_benchmark(void (*fn)(void *, uint8_t, uint64_t), uint64_t delta, uint32_t sample_size)
{
    uint32_t i;
    uint32_t *times;

    times = cc_alloc(sample_size * sizeof(uint32_t));

    if(times == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark");
	exit(1);
    }

    fill_cache_numeric();

    for(i = 0; i < sample_size; ++i) {
	struct timespec before, after;
	char key[20];
	uint8_t nkey;
	nkey = sprintf(key, "%d", i);

	before = orwl_gettime();
	fn(key, nkey, delta);
	after = orwl_gettime();

	times[i] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
    }

    get_time_stats(times, sample_size);

    flush_cache();

    cc_free(times);
}

void
get_getref_benchmark(uint32_t nval, uint32_t sample_size)
{
    uint32_t i;
    uint32_t *times;

    times = cc_alloc(sample_size * sizeof(uint32_t));

    if(times == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark");
	exit(1);
    }

    fill_cache(nval, sample_size);

    for(i = 0; i < sample_size; ++i) {
	struct timespec before, after;
	struct iovec vector[20];
	char key[20];
	uint8_t nkey;

	nkey = sprintf(key, "%d", i);

	before = orwl_gettime();
	get_val_ref(key, nkey, vector);
	after = orwl_gettime();

	times[i] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
    }

    get_time_stats(times, sample_size);

    flush_cache();

    cc_free(times);
}

void
get_getval_benchmark(uint32_t nval, uint32_t sample_size)
{
    uint32_t i;
    uint32_t *times;
    void *buf;

    times = cc_alloc(sample_size * sizeof(uint32_t));
    buf = cc_alloc(nval);

    if(times == NULL || buf == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark");
	exit(1);
    }

    fill_cache(nval, sample_size);

    for(i = 0; i < sample_size; ++i) {
	struct timespec before, after;
	char key[20];
	uint8_t nkey;

	nkey = sprintf(key, "%d", i);

	before = orwl_gettime();
	get_val(key, nkey, buf, nval, 0);
	after = orwl_gettime();

	times[i] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
    }

    get_time_stats(times, sample_size);

    flush_cache();

    cc_free(buf);
    cc_free(times);
}

void
get_remove_benchmark(uint32_t nval, uint32_t sample_size)
{
    uint32_t i;
    uint32_t *times;

    times = cc_alloc(sample_size * sizeof(uint32_t));

    if(times == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark");
	exit(1);
    }

    fill_cache(nval, sample_size);

    for(i = 0; i < sample_size; ++i) {
	struct timespec before, after;
	char key[20];
	uint8_t nkey;

	nkey = sprintf(key, "%d", i);

	before = orwl_gettime();
	remove_key(key, nkey);
	after = orwl_gettime();

	times[i] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
    }

    get_time_stats(times, sample_size);

    flush_cache();

    cc_free(times);
}

void
get_zipmap_replace_benchmark(void *pkey, uint8_t npkey, uint32_t nval, uint32_t num, uint32_t new_nval, uint32_t sample_size)
{
    uint32_t i;
    uint32_t *times;
    char *val;

    times = cc_alloc(sample_size * sizeof(uint32_t));
    val = cc_alloc(new_nval);

    if(times == NULL || val == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark");
	exit(1);
    }

    cc_memset(val, 0xff, new_nval);

    for(i = 0; i < sample_size; ++i) {
	struct timespec before, after;
	char skey[20];
	uint8_t nskey;

	nskey = sprintf(skey, "%u", i % num);

	fill_zipmap(pkey, npkey, nval, num);

	before = orwl_gettime();
	zmap_replace(pkey, npkey, skey, nskey, val, new_nval);
	after = orwl_gettime();

	empty_zipmap(pkey, npkey);

	times[i] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
    }

    get_time_stats(times, sample_size);

    cc_free(val);
    cc_free(times);
}

void
get_zipmap_delete_benchmark(void *pkey, uint8_t npkey, uint32_t nval, uint32_t num, uint32_t sample_size)
{
    uint32_t i;
    uint32_t *times;

    times = cc_alloc(sample_size * sizeof(uint32_t));

    if(times == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark");
	exit(1);
    }

    for(i = 0; i < sample_size; ++i) {
	struct timespec before, after;
	char skey[20];
	uint8_t nskey;

	nskey = sprintf(skey, "%u", i % num);

	fill_zipmap(pkey, npkey, nval, num);

	before = orwl_gettime();
	zmap_delete(pkey, npkey, skey, nskey);
	after = orwl_gettime();

	empty_zipmap(pkey, npkey);

	times[i] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
    }

    get_time_stats(times, sample_size);

    cc_free(times);
}

void
get_zipmap_get_benchmark(void *pkey, uint8_t npkey, uint32_t nval, uint32_t num, uint32_t sample_size, uint32_t index)
{
    uint32_t i;
    uint32_t *times;
    char skey[20];
    uint8_t nskey;

    times = cc_alloc(sample_size * sizeof(uint32_t));

    if(times == NULL) {
	log_stderr("fatal: not enough memory to perform benchmark");
	exit(1);
    }

    nskey = sprintf(skey, "%u", index);

    fill_zipmap(pkey, npkey, nval, num);

    for(i = 0; i < sample_size; ++i) {
	struct timespec before, after;
	void *val_ptr;
	uint32_t val_len;

	before = orwl_gettime();
	zmap_get(pkey, npkey, skey, nskey, &val_ptr, &val_len);
	after = orwl_gettime();

	times[i] = (after.tv_nsec - before.tv_nsec) + 1000000000 * (after.tv_sec - before.tv_sec);
    }

    empty_zipmap(pkey, npkey);

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
    uint64_t sum = 0;
    uint32_t i;
    qsort(times, ntimes, sizeof(uint32_t), compare_int);

    for(i = 0; i < ntimes; ++i) {
	sum += times[i];
    }

    printf("sample size: %u\nall times in nanoseconds\n"
	   "min: %u\n25th percentile: %u\n"
	   "50th percentile: %u\n75th percentile: %u\n90th percentile: %u\n"
	   "95th percentile: %u\n99th percentile: %u\n99.9th percentile: %u\n"
	   "max: %u\navg: %llu\n\n", ntimes, times[0],
	   times[(int)((double)ntimes * 0.25)],
	   times[(int)((double)ntimes * 0.5)],
	   times[(int)((double)ntimes * 0.75)],
	   times[(int)((double)ntimes * 0.9)],
	   times[(int)((double)ntimes * 0.95)],
	   times[(int)((double)ntimes * 0.99)],
	   times[(int)((double)ntimes * 0.999)],
	   times[ntimes - 1],
	   sum / ntimes);
}
