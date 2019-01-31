#include <cc_histogram.h>

#include <cc_debug.h>
#include <cc_mm.h>

#include <float.h>
#include <x86intrin.h>
#include <math.h>

struct histo_u32 *
histo_u32_create(uint32_t m, uint32_t r, uint32_t n, bool over)
{
    struct histo_u32 *histo;

    if (r <= m || r > n || n > 64) { /* validate constraints on input */
        log_error("Invalid input value among m=%"PRIu32", r=%"PRIu32", n=%"
            PRIu32, m, r, n);

        return NULL;
    }

    histo = cc_alloc(sizeof(struct histo_u32));
    if (histo == NULL) {
        log_error("Failed to allocate struct histo_u32");

        return NULL;
    }

    histo->m = m;
    histo->r = r;
    histo->n = n;
    histo->over = over;

    histo->M = 1 << m;
    histo->R = (1 << r) - 1;
    histo->N = (1 << n) - 1;
    histo->G = 1 << (r - m - 1);
    histo->nbucket = (n - r + 2) * histo->G;

    histo_u32_reset(histo);
    log_verb("Created histogram %p with parametersm=%"PRIu32", r=%"PRIu32", n=%"
            PRIu32"; nbucket=%"PRIu64, histo, m, r, n, histo->nbucket);

    return histo;
}

void
histo_u32_destroy(struct histo_u32 **h)
{
    ASSERT(h != NULL);

    struct histo_u32 *histo = *h;

    if (histo == NULL) {
        return;
    }

    cc_free(histo->buckets);
    cc_free(histo);
    *h = NULL;

    log_verb("Destroyed histogram at %p", histo);
}

void
histo_u32_reset(struct histo_u32 *h)
{
    ASSERT(h != NULL);

    h->nrecord = 0;
    memset(h->buckets, 0, sizeof(uint32_t) * h->nbucket);
}

static inline uint64_t
_bucket_offset(uint64_t value, uint32_t m, uint32_t r, uint64_t G)
{
    uint64_t v = (value == 0) + value; /* lzcnt is undefined for 0 */
#ifdef __LZCNT__
    uint32_t h = 63 - __lzcnt64(v);
#else
    uint32_t h = 63 - __builtin_clzll(v);
#endif

    if (h < r) {
        return value >> m;
    } else {
        uint32_t d = h - r + 1;
        return (d + 1) * G + ((value - (1 << h)) >> (m + d));
    }
}

histo_rstatus_e
histo_u32_record(struct histo_u32 *h, uint64_t value, uint32_t count)
{
    uint64_t offset = 0;

    if (value > h->N) {
        log_error("Value not recorded due to overflow: %"PRIu64" is greater"
                "than max value allowed, which is %"PRIu64, value, h->N);

        return HISTO_EOVERFLOW;
    }

    offset = _bucket_offset(value, h->m, h->n, h->G);
    *(h->buckets + offset) += count;

    return HISTO_OK;
}

static inline bool
_greater_dbl(double a, double b) {
    return (a - b) >= DBL_EPSILON;
}

static inline bool
_lesser_dbl(double a, double b) {
    return (b - a) >= DBL_EPSILON;
}

static inline uint64_t
_bucket_low(const struct histo_u32 *h, uint64_t offset)
{
    uint32_t g = offset >> (h->r - h->m - 1); /* bucket offset in terms of G */
    uint64_t b = g - g * h->G;

    /* g < 2 & g >= 2 have different formula */
    return (g < 2) * ((1 << h->m) * b) +
        (g >= 2) * ((1 << (h->r + g - 2)) + (1 << (h->m + g - 1)) * b);
}

static inline uint64_t
_bucket_high(const struct histo_u32 *h, uint64_t offset)
{
    uint32_t g = offset >> (h->r - h->m - 1); /* bucket offset in terms of G */
    uint64_t b = g - g * h->G + 1; /* the next bucket */

    /* g < 2 & g >= 2 have different formula */
    return (g < 2) * ((1 << h->m) * b - 1) +
        (g >= 2) * ((1 << (h->r + g - 2)) + (1 << (h->m + g - 1)) * b - 1);
}

histo_rstatus_e
histo_u32_report(uint64_t *value, const struct histo_u32 *h, double p)
{
    ASSERT(h != NULL);

    uint64_t rthreshold, rcount = 0;
    uint64_t maxb;
    uint64_t offset = 0;
    uint32_t *bucket = h->buckets;

    if (_greater_dbl(p, 1.0f)) {
        log_error("Percentile must be between [0.0, 1.0], %f provided", p);

        return HISTO_EOVERFLOW;
    }
    if (_lesser_dbl(p, 0.0f)) {
        log_error("Percentile must be between [0.0, 1.0], %f provided", p);

        return HISTO_EUNDERFLOW;
    }
    if (h->nrecord == 0) {
        log_info("No value to report due to histogram being empty");

        return HISTO_EEMPTY;
    }

    /* find the lowest non-empty bucket */
    while (offset < h->nbucket && *bucket == 0) {
        bucket++;
        offset++;
    }

    /* calculate number of records we need to count for the percentile depending
     * on over-/under- preference. rthreshold should be no more than h->nrecord
     */
    rthreshold = (uint64_t) ((p * h->nrecord + 1 - DBL_EPSILON) * h->over +
            (p * h->nrecord) * (1 - h->over));

    /* find the first bucket where the record count threshold is met */
    for (; offset < h->nbucket && rcount < rthreshold; ++offset, ++bucket) {
        bool empty = (*bucket == 0);
        rcount += *bucket;
        maxb = maxb * empty + offset * !empty;
    }

    /* this shouldn't happen but sticking the logic here to prevent corner cases
     * due to floating point corner cases when calculating rcount
     */
    if (offset == h->nbucket) {
        offset = maxb;
    }

    if (h->over) {
        *value = _bucket_high(h, offset);
    } else {
        *value = _bucket_low(h, offset);
    }

    return HISTO_OK;
}

histo_rstatus_e
histo_u32_report_multi(struct percentile_profile *pp, const struct histo_u32 *h)
{
    ASSERT(pp != NULL);
    ASSERT(h != NULL);

    uint64_t rthreshold, rcount = 0;
    uint64_t minb, maxb;
    uint64_t offset = 0;
    uint32_t *bucket = h->buckets;
    double *p = pp->percentile;
    uint64_t *v = pp->result;

    if (h->nrecord == 0) {
        log_info("No value to report due to histogram being empty");

        return HISTO_EEMPTY;
    }

    /* find the lowest non-empty bucket */
    while (offset < h->nbucket && *bucket == 0) {
        bucket++;
        offset++;
    }
    minb = offset;
    if (h->over) {
        pp->min = _bucket_high(h, minb);
    } else {
        pp->min = _bucket_low(h, minb);
    }

    for (uint8_t i = 0; i < pp->count; i++, p++, v++) {

        /* calculate number of records we need to count for the percentile depending
         * on over-/under- preference. rthreshold should be no more than h->nrecord
         */
        rthreshold = (uint64_t) ((*p * h->nrecord + 1 - DBL_EPSILON) * h->over +
                (*p * h->nrecord) * (1 - h->over));

        /* find the next smallest bucket where the record count threshold is met */
        for (; offset < h->nbucket && rcount < rthreshold; ++offset, ++bucket) {
            bool empty = (*bucket == 0);
            rcount += *bucket;
            maxb = maxb * empty + offset * !empty;
        }

        if (h->over) {
            *v = _bucket_high(h, offset);
        } else {
            *v = _bucket_low(h, offset);
        }
    }
    if (h->over) {
        pp->max = _bucket_high(h, maxb);
    } else {
        pp->max = _bucket_low(h, maxb);
    }

    return HISTO_OK;
}

struct percentile_profile *
percentile_profile_create(uint8_t cap)
{
    struct percentile_profile *pp;

    pp = cc_alloc(sizeof(struct percentile_profile));
    if (pp == NULL) {
        log_error("Failed to allocate struct percentile_profile");

        return NULL;
    }
    pp->percentile = cc_alloc(cap * sizeof(double));
    if (pp->percentile == NULL) {
        log_error("Failed to allocate percentile in struct percentile_profile");
        cc_free(pp);

        return NULL;
    }
    pp->result = cc_alloc(cap * sizeof(uint64_t));
    if (pp->result == NULL) {
        log_error("Failed to allocate result in struct percentile_profile");
        cc_free(pp->percentile);
        cc_free(pp);

        return NULL;
    }

    pp->cap = cap;
    pp->count = 0;

    log_verb("Created percentile_profile %p with "PRIu8" configurable "
            "percentiles", cap);

    return pp;

}

void
percentile_profile_destroy(struct percentile_profile **pp)
{
    ASSERT(pp != NULL);

    struct percentile_profile *p = *pp;

    if (p == NULL) {
        return;
    }

    cc_free(p->percentile);
    cc_free(p->result);
    cc_free(p);
    *pp = NULL;

    log_verb("Destroyed percentile_profile at %p", p);
}

histo_rstatus_e
percentile_profile_set(struct percentile_profile *pp, const double *percentile, uint8_t count)
{
    const double *src = percentile;
    double *dst = pp->percentile;
    double last = 0.0;

    for (; count > 0; count--, src++, dst++) {
        if (_greater_dbl(*src, 1.0f)) {
            log_error("Percentile must be between [0.0, 1.0], %f provided", *src);

            return HISTO_EOVERFLOW;
        }
        if (_lesser_dbl(*src, 0.0f)) {
            log_error("Percentile must be between [0.0, 1.0], %f provided", *src);

            return HISTO_EUNDERFLOW;
        }
        if (_lesser_dbl(*src, last)) {
            log_error("Percentile being queried must be increasing");

            return HISTO_EORDER;
        }

        last = *src;
        *dst = *src;
    }
    pp->count = count;

    log_verb("Set percentile_profile with %"PRIu8" predefined percentiles", count);

    return HISTO_OK;
}
