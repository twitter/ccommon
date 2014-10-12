#ifndef _CC_POOL_H_
#define _CC_POOL_H_

#include <cc_debug.h>
#include <cc_queue.h>

#include <inttypes.h>
#include <stdbool.h>

#define FREEPOOL(pool, name, type)                                  \
STAILQ_HEAD(name, type);                                            \
struct pool {                                                       \
    struct name     freeq;                                          \
    uint32_t        nfree;                                          \
    uint32_t        nused;                                          \
    uint32_t        nmax;                                           \
    bool            initialized;                                    \
}

#define FREEPOOL_CREATE(pool, max) do {                             \
    STAILQ_INIT(&(pool)->freeq);                                    \
    (pool)->nmax = (max) > 0 ? (max) : UINT32_MAX;                  \
    (pool)->nfree = 0;                                              \
    (pool)->nused = 0;                                              \
    (pool)->initialized = true;                                     \
} while (0)

#define FREEPOOL_DESTROY(var, tvar, pool, field, destroy) do {      \
    ASSERT((pool)->initialized);                                    \
    ASSERT((pool)->nused == 0);                                     \
    STAILQ_FOREACH_SAFE(var, &(pool)->freeq, field, tvar) {         \
        STAILQ_REMOVE_HEAD(&(pool)->freeq, next);                   \
        (pool)->nfree--;                                            \
        destroy(var);                                               \
    }                                                               \
    ASSERT((pool)->nfree == 0);                                     \
    ASSERT(STAILQ_EMPTY(&(pool)->freeq));                           \
} while (0)

#define FREEPOOL_BORROW(var, pool, field, create) do {              \
    ASSERT((pool)->initialized);                                    \
    if (!STAILQ_EMPTY(&(pool)->freeq)) {                            \
        (var) = STAILQ_FIRST(&(pool)->freeq);                       \
        STAILQ_REMOVE_HEAD(&(pool)->freeq, field);                  \
        (pool)->nfree--;                                            \
    } else if ((pool)->nfree + (pool)->nused < (pool)->nmax) {      \
        (var) = create();                                           \
    } else {                                                        \
        (var) = NULL;                                               \
    }                                                               \
    if ((var) != NULL) {                                            \
        (pool)->nused++;                                            \
        STAILQ_NEXT((var), field) = NULL;                           \
    }                                                               \
} while (0)

#define FREEPOOL_RETURN(pool, elm, field) do {                      \
    ASSERT((pool)->initialized);                                    \
    STAILQ_INSERT_HEAD(&(pool)->freeq, elm, field);                 \
    (pool)->nfree++;                                                \
    (pool)->nused--;                                                \
} while (0)

#endif /* _CC_POOL_H_ */
