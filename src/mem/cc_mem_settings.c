#include <mem/cc_mem_settings.h>

#include <cc_log.h>
#include <cc_mm.h>

#include <stdio.h>
#include <strings.h>

//#include <stdlib.h>
//#include <strings.h>

#define SETTING_LINE_MAX 1024

/* Whether or not settings have been initialized for the first time */
static bool settings_initialized = false;

/* Struct containing all of the mem module settings */
struct mem_settings mem_settings = { SETTINGS_MEM(SETTINGS_INIT) };

/* Macro for carrying out config file directives. */
#define SETTINGS_LOAD_FILE(_name, _required, _type, _dynamic, _default, _description)\
    if(!strcasecmp(argv[0], #_name) && argc >= 2) {                                  \
        if(_dynamic && settings_initialized) {                                       \
	    log_debug(LOG_NOTICE, "Cannot overwrite non dynamic setting " #_name);   \
	} else {                                                                     \
	    struct setting_val val;                                                  \
	    val.type = _type ## _setting;                                            \
	    val.val = settings_str_to_val(_type ## _setting, argc, argv);            \
	    settings_set(&(mem_settings._name), &val);		                     \
	}                                                                            \
    } else

/* Macro for checking whether or not all required settings have been initialized */
#define SETTINGS_REQUIRED(_name, _required, _type, _dynamic, _default, _description) \
    (!_required || mem_settings._name.initialized) &&

rstatus_t
mem_settings_load_from_file(char *config_file)
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

    while(fgets(buf, SETTING_LINE_MAX, fp) != NULL) {
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
	SETTINGS_MEM(SETTINGS_LOAD_FILE)
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

void
mem_settings_desc(void)
{
    SETTINGS_MEM(SETTINGS_PRINT)
}
