#ifndef _CC_SETTINGS_H_
#define _CC_SETTINGS_H_

#include "cc_slabs.h"
#include "cc_time.h"

#include <stdbool.h>
#include <stdint.h>

struct settings {
                                                  /* options with no argument */

    bool            prealloc;                     /* memory  : whether we preallocate for slabs */
    bool            evict_lru;                    /* memory  : if true, use lru eviction; else, random */
    bool            use_freeq;                    /* memory  : whether use items in freeq or not */
    bool            use_cas;                      /* protocol: whether cas is supported */

                                                  /* options with required argument */

    size_t          maxbytes;                     /* memory  : maximum bytes allowed for slabs */
    size_t          slab_size;                    /* memory  : slab size */

                                                  /* global state */

    size_t          profile[SLABCLASS_MAX_IDS];   /* memory  : slab profile */
    uint8_t         profile_last_id;              /* memory  : last id in slab profile */

                                                  /* global state */
    rel_time_t      oldest_live;                  /* data    : ignore existing items older than this */
};

extern struct settings settings;

#endif
