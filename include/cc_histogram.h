#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum histo_rstatus {
    HISTO_OK         = 0,
    HISTO_EOVERFLOW  = -1,
    HISTO_EUNDERFLOW = -2,
    HISTO_EEMPTY     = -3,
    HISTO_EORDER     = -4,
} histo_rstatus_e;

struct percentile_profile
{
    uint8_t cap;       /* number of percentiles that can be lookedup at once */
    uint8_t count;     /* number of percentiles to look up */
    double *percentile;/* sorted percentiles to be queried, allocated at init */
    uint64_t *result;  /* results of lookup, allocated at init */
    uint64_t min;      /* min value */
    uint64_t max;      /* max value */
};

struct histo_u32
{
    /* the following variables are configurable */
    uint32_t m;
    uint32_t r;
    uint32_t n;
    bool     over; /* reported values should overstate if true, understate if false */

    /* the following variables are computed from those above */
    uint64_t M; /* Minimum Resolution: 2^m */
    uint64_t R; /* Minimum Resolution Range: 2^r - 1 */
    uint64_t N; /* Maximum Value: 2^n - 1 */
    uint64_t G; /* Grouping Factor: 2^(r-m-1) */
    uint64_t nbucket;  /* total number of buckets: (n-r+2)*G */

    /* we are treating integer operations as atomic (under relaxed constraint),
     * this is true on x86 which is all we care about for now
     */
    uint64_t nrecord;  /* total number of records */
    uint32_t *buckets; /* buckets where counts are kept as uint32_t */

    /* these are only used for producer/consumer style access */
    // pthread_spinlock_t lock;
};

/* APIs */
struct histo_u32 *histo_u32_create(uint32_t m, uint32_t r, uint32_t n, bool roundup);
void histo_u32_destroy(struct histo_u32 **h);

/* the most important assumption here is that we assume reset/report will always
 * be called by the same thread, while record is called from a different thread.
 * This fits the worker / admin model where the former records value and the
 * latter reports/resets histograms.
 */
void histo_u32_reset(struct histo_u32 *h);
histo_rstatus_e histo_u32_record(struct histo_u32 *h, uint64_t value, uint32_t count);
/* the following functions return the highest/lowest value the corresponding
 * buckets represent depending on how histogram is configured in terms of over-/
 * under-stating.
 * If the histogram is too sparse for the percentile specified, the next
 * higher (if overstating) / lower (if understating) non-empty bucket would be
 * returned, with all values bounded by minimum / maximum occupied buckets.
 *
 * For example, for m=2 and records (2, 5). From an overstating histogram,
 * min/p0 returns 2, p25 returns 2, p50 returns 2, p75 returns 6, p90 returns 6,
 * and max/p100 returns 6. From an understating histogram, min/p0 returns 1,
 * p25/p50/p75/p90 all return 1, and max/p100 returns 5.
 */
histo_rstatus_e histo_u32_report(uint64_t *value, const struct histo_u32 *h, double p);
histo_rstatus_e histo_u32_report_multi(struct percentile_profile *pp, const struct histo_u32 *h);

void histo_u32_reset_consumer(struct histo_u32*h);
histo_rstatus_e histo_u32_record_producer(struct histo_u32 *h, uint64_t value, uint32_t count);

struct percentile_profile *percentile_profile_create(uint8_t cap);
void percentile_profile_destroy(struct percentile_profile **pp);
histo_rstatus_e percentile_profile_set(struct percentile_profile *pp, const double *percentile, uint8_t count);

#ifdef __cplusplus
}
#endif
