#ifndef _CC_MEM_SETTINGS_H_
#define _CC_MEM_SETTINGS_H_

#include <cc_settings.h>

/* Sample Config File */
/*
  prealloc 1
  evict_lru 0
  use_freeq 1
  use_cas 1
  maxbytes 4294967296
  slab_size 1050000
  # Hash power not included, it is not required
  profile 128 256 512 1024 2048 4096 8192 16387 32768 65536 131072 262144 524288 1048576
  profile_last_id 14
  oldest_live 6000
 */

/*
 *         NAME             REQUIRED        TYPE                DYNAMIC         DEFAULT             DESCRIPTION
 */
#define SETTINGS_MEM(ACTION)                                                                                                                                        \
    ACTION(prealloc,        false,          bool_val,           false,          true,               "Whether or not slabs are preallocated upon startup")           \
    ACTION(evict_lru,       false,          bool_val,           true,           true,               "Whether we use an LRU eviction scheme or random eviction")     \
    ACTION(use_freeq,       false,          bool_val,           true,           true,               "Whether we use items in the free queue or not")                \
    ACTION(use_cas,         false,          bool_val,           false,          false,              "Whether or not check-and-set is supported")                    \
    ACTION(maxbytes,        true,           uint64_val,         false,          0,                  "Maximum bytes allowed for slabs")                              \
    ACTION(slab_size,       true,           uint32_val,         false,          0,                  "Number of bytes in each slab")                                 \
    ACTION(hash_power,      false,          uint8_val,          false,          0,                  "Default hash table power")                                     \
    ACTION(profile,         true,           uint32ptr_val,      false,          NULL,               "Slab profile - slab class sizes")                              \
    ACTION(profile_last_id, true,           uint8_val,          false,          0,                  "Last id in the slab profile array")                            \
    ACTION(oldest_live,     false,          reltime_val,        true,           6000,               "Ignore existing items older than this")                        \


/* Struct that holds all of the settings specified in the settings matrix */
struct mem_settings {
    SETTINGS_MEM(SETTINGS_DECLARE)
};

extern struct mem_settings mem_settings;

rstatus_t mem_settings_load_from_file(char *config_file);

void mem_settings_desc(void);

#endif
