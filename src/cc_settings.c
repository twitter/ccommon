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

#include <cc_debug.h>
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

/* Initialize settings members */
#define SETTINGS_INIT(_name, _required, _type, _dynamic, _default, _description) \
    ._name = {.required = (_required), .dynamic = (_dynamic), .initialized = false, .desc = _description, .val._type = _default},

struct settings settings = { SETTINGS_MEM(SETTINGS_INIT) };

/* Macro for checking whether or not all required settings have been initialized */
#define SETTINGS_REQUIRED(_name, _required, _type, _dynamic, _default, _description) \
    (!settings._name.required || settings._name.initialized) &&


/* Macro for carrying out config file directives */
#define SETTINGS_EXE(_name, _required, _type, _dynamic, _default, _description)    \
    if(!strcasecmp(argv[0], #_name) && argc >= 2) {                                \
        if(_dynamic && settings_initialized) {                                     \
	    log_debug(LOG_NOTICE, "Cannot overwrite non dynamic setting " #_name); \
	} else {                                                                   \
	    set_setting(&settings._name, #_type, argc, argv);		           \
	    settings._name.initialized = true;                                     \
	}                                                                          \
    } else                                                                         \

static void set_setting(struct setting *setting, char *type, int32_t argc, sds *argv);

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

	/* Split into arguments */
	argv = sdssplitargs(line, &argc);
	sdstolower(argv[0]);

	/* execute config file directives */
	SETTINGS_MEM(SETTINGS_EXE)
	{
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

/* Print out a description of each setting */
#define SETTINGS_PRINT(_name, _required, _type, _dynamic, _default, _description) \
    loga(#_name ": %s", _description);

void
settings_desc(void)
{
    SETTINGS_MEM(SETTINGS_PRINT)
}

/* Sets the given setting, depending on the type */
static void
set_setting(struct setting *setting, char *type, int32_t argc, sds *argv)
{
    ASSERT(type != NULL);

    if(strncmp("bool_val", type, 8) == 0) {
	setting->val.bool_val = atoi(argv[1]);
    } else if(strncmp("uint8_val", type, 9) == 0) {
	setting->val.uint8_val = atoi(argv[1]);
    } else if(strncmp("uint32_val", type, 10) == 0) {
	setting->val.uint32_val = atoi(argv[1]);
    } else if(strncmp("uint64_val", type, 10) == 0) {
	setting->val.uint64_val = atol(argv[1]);
    } else if(strncmp("reltime_val", type, 11) == 0) {
	setting->val.reltime_val = atoi(argv[1]);
    } else if(strncmp("uint32ptr_val", type, 13) == 0) {
	uint32_t i;
	setting->val.uint32ptr_val = cc_alloc((argc - 1) * sizeof(uint32_t));
	for(i = 0; i < argc - 1; ++i) {
	    setting->val.uint32ptr_val[i] = atoi(argv[i + 1]);
	}
    } else {
	log_debug(LOG_CRIT, "fatal: unrecognized setting type");
	exit(1);
    }
}
