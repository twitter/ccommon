/*
 * ccommon - a cache common library.
 * Copyright (C) 2013 Twitter, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _CC_SETTINGS_H_
#define _CC_SETTINGS_H_

#include <cc_define.h>
#include <cc_time.h>

#include <stdbool.h>
#include <stdint.h>

/*
 * Each setting is described by a 6-tuple (NAME, REQUIRED, TYPE, DYNAMIC, DEFAULT, DESCRIPTION).
 *   - NAME will be the name of the struct setting member in the global struct settings.
 *   - REQUIRED is true if the setting needs to be initialized on the first configuration load of the
 *     system, and false if it does not.
 *   - TYPE is the type of the setting's payload (in union val_u)
 *   - DYNAMIC is true if the setting is allowed to change while the system is running, and false if
 *     it is not.
 *   - DEFAULT is the default value of the setting.
 *   - DESCRIPTION is a brief description of what the setting does.
 *
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


/*
 * Macro used to declare each individual setting in struct settings
 */
#define SETTINGS_DECLARE(_name, _required, _type, _dynamic, _default, _description) \
    struct setting _name;


/* Struct that holds all of the settings specified in the settings matrix */
struct settings {
    struct setting {
	const bool required;
	const bool dynamic;
	bool initialized;
	const char *desc;
	union val_u {
	    bool bool_val;
	    uint8_t uint8_val;
	    uint32_t uint32_val;
	    uint64_t uint64_val;
	    rel_time_t reltime_val;
	    uint32_t *uint32ptr_val;
	} val;
    };

    SETTINGS_MEM(SETTINGS_DECLARE)
};

extern struct settings settings;

/* Load the settings from the provided config file */
rstatus_t settings_load(char *config_file);

/* Print out a description of each of the settings */
void settings_desc(void);

#endif
