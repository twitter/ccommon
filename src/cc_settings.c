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

#include <cc_settings.h>

#include <cc_define.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <cc_sds.h>

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#define SETTING_LINE_MAX 1024

static bool settings_initialized = false; /* Whether or not settings have been initialized for the first time */

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

#define SETTINGS_REQUIRED(_name, _required, _type, _dynamic, _default, _description) \
    (!settings._name.required || settings._name.initialized) &&

#define SETTINGS_INIT(_name, _required, _type, _dynamic, _default, _description) \
    ._name = {.required = (_required), .dynamic = (_dynamic), .initialized = false, .desc = _description, .val._type = _default},

struct settings settings = { SETTINGS_MEM(SETTINGS_INIT) };

rstatus_t
settings_load(char *config_file)
{
    FILE *fp;
    char buf[SETTING_LINE_MAX];
    uint32_t linenum = 0;
    sds line = NULL;

    if(config_file[0] == '-' && config_file[1] == '\0') {
	fp = stdin;
    } else {
	if((fp = fopen(config_file, "r")) == NULL) {
	    log_debug(LOG_CRIT, "Could not open config file %s!", config_file);
	    return CC_ERROR;
	}
    }

    while(fgets(buf, SETTING_LINE_MAX + 1, fp) != NULL) {
	sds *argv;
	int32_t argc;
	uint32_t i;

	++linenum;
	line = sdsnew(buf);
	line = sdstrim(line, " \t\r\n");

	/* Skip comments and blank lines */
	if(line[0] == '#' || line[0] == '\0') {
	    sdsfree(line);
	    continue;
	}

	printf("line: %s\n", line);

	/* Split into arguments */
	argv = sdssplitargs(line, &argc);
	sdstolower(argv[0]);

	/* execute config file directives */
	/* Currently, the settings members and types are hard coded, will think about
	   how to do this based on the SETTINGS_MEM macro */
	if(!strcasecmp(argv[0], "prealloc") && argc == 2) {
	    if(settings_initialized && !settings.prealloc.dynamic) {
		/* Attempting to overwrite static setting */
		log_debug(LOG_NOTICE, "Cannot overwrite non dynamic setting %s!", argv[0]);
	    } else {
		settings.prealloc.val.bool_val = atoi(argv[1]);
		settings.prealloc.initialized = true;
	    }
	} else if(!strcasecmp(argv[0], "evict_lru") && argc == 2) {
	    if(settings_initialized && !settings.evict_lru.dynamic) {
		log_debug(LOG_NOTICE, "Cannot overwrite non dynamic setting %s!", argv[0]);
	    } else {
		settings.evict_lru.val.bool_val = atoi(argv[1]);
		settings.evict_lru.initialized = true;
	    }
	} else if(!strcasecmp(argv[0], "use_freeq") && argc == 2) {
	    if(settings_initialized && !settings.use_freeq.dynamic) {
		log_debug(LOG_NOTICE, "Cannot overwrite non dynamic setting %s!", argv[0]);
	    } else {
		settings.use_freeq.val.bool_val = atoi(argv[1]);
		settings.use_freeq.initialized = true;
	    }
	} else if(!strcasecmp(argv[0], "use_cas") && argc == 2) {
	    if(settings_initialized && !settings.use_cas.dynamic) {
		log_debug(LOG_NOTICE, "Cannot overwrite non dynamic setting %s!", argv[0]);
	    } else {
		settings.use_cas.val.bool_val = atoi(argv[1]);
		settings.use_cas.initialized = true;
	    }
	} else if(!strcasecmp(argv[0], "maxbytes") && argc == 2) {
	    if(settings_initialized && !settings.maxbytes.dynamic) {
		log_debug(LOG_NOTICE, "Cannot overwrite non dynamic setting %s!", argv[0]);
	    } else {
		settings.maxbytes.val.uint64_val = atol(argv[1]);
		settings.maxbytes.initialized = true;
	    }
	} else if(!strcasecmp(argv[0], "slab_size") && argc == 2) {
	    if(settings_initialized && !settings.slab_size.dynamic) {
		log_debug(LOG_NOTICE, "Cannot overwrite non dynamic setting %s!", argv[0]);
	    } else {
		settings.slab_size.val.uint32_val = atoi(argv[1]);
		settings.slab_size.initialized = true;
	    }
	} else if(!strcasecmp(argv[0], "hash_power") && argc == 2) {
	    if(settings_initialized && !settings.hash_power.dynamic) {
		log_debug(LOG_NOTICE, "Cannot overwrite non dynamic setting %s!", argv[0]);
	    } else {
		settings.hash_power.val.uint8_val = atoi(argv[1]);
		settings.hash_power.initialized = true;
	    }
	} else if(!strcasecmp(argv[0], "profile")) {
	    if(settings_initialized && !settings.profile.dynamic) {
		log_debug(LOG_NOTICE, "Cannot overwrite non dynamic setting %s!", argv[0]);
	    } else {
		settings.profile.val.uint32ptr_val = cc_alloc((argc - 1) * sizeof(uint32_t));
		for(i = 0; i < argc - 1; ++i) {
		    settings.profile.val.uint32ptr_val[i] = atoi(argv[i + 1]);
		}
		settings.profile.initialized = true;
	    }
	} else if(!strcasecmp(argv[0], "profile_last_id") && argc == 2) {
	    if(settings_initialized && !settings.profile_last_id.dynamic) {
		log_debug(LOG_NOTICE, "Cannot overwrite non dynamic setting %s!", argv[0]);
	    } else {
		settings.profile_last_id.val.uint8_val = atoi(argv[1]);
		settings.profile_last_id.initialized = true;
	    }
	} else if(!strcasecmp(argv[0], "oldest_live") && argc == 2) {
	    if(settings_initialized && !settings.oldest_live.dynamic) {
		log_debug(LOG_NOTICE, "Cannot overwrite non dynamic setting %s!", argv[0]);
	    } else {
		settings.oldest_live.val.reltime_val = atoi(argv[1]);
		settings.oldest_live.initialized = true;
	    }
	} else {
	    log_debug(LOG_CRIT, "Error in config file: incorrect number or type of elements at line %u", linenum);
	    log_debug(LOG_CRIT, ">>> '%s'", line);

	    for(i = 0; i < argc; ++i) {
		sdsfree(argv[i]);
	    }
	    cc_free(argv);
	    sdsfree(line);

	    return CC_ERROR;
	}

	for(i = 0; i < argc; ++i) {
	    sdsfree(argv[i]);
	}
	cc_free(argv);
	sdsfree(line);
    }

    if(fp != stdin) {
	fclose(fp);
    }

    if(!settings_initialized) {
	if(!(SETTINGS_MEM(SETTINGS_REQUIRED)1)) {
	    /* Not all required settings were initialized */
	    log_debug(LOG_CRIT, "Error in config file: some required options were not initialized");
	    return CC_ERROR;
	}
	settings_initialized = true;
    }

    return CC_OK;
}
