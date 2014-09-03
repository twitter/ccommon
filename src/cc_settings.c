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

#include <stdlib.h>

rstatus_t
settings_set(struct setting *setting, struct setting_val *val)
{
    switch(val->type) {
    case bool_val_setting:
	setting->val.bool_val = *((bool *)val->val);
	setting->initialized = true;
	cc_free(val->val);
	break;
    case uint8_val_setting:
	setting->val.uint8_val = *((uint8_t *)val->val);
	setting->initialized = true;
	cc_free(val->val);
	break;
    case uint32_val_setting:
	setting->val.uint32_val = *((uint32_t *)val->val);
	setting->initialized = true;
	cc_free(val->val);
	break;
    case uint64_val_setting:
	setting->val.uint64_val = *((uint64_t *)val->val);
	setting->initialized = true;
	cc_free(val->val);
	break;
    case reltime_val_setting:
	setting->val.reltime_val = *((rel_time_t *)val->val);
	setting->initialized = true;
	cc_free(val->val);
	break;
    case uint32ptr_val_setting: {
	setting->val.uint32ptr_val = val->val;
	setting->initialized = true;
	break;
    }
    default:
	log_debug(LOG_CRIT, "fatal: unrecognized setting type");
	return CC_ERROR;
    }

    return CC_OK;
}

void *
settings_str_to_val(settings_type_t type, int32_t argc, sds *argv)
{
    ASSERT(argc >= 2);
    void *ret;

    switch(type) {
    case bool_val_setting:
	ret = cc_alloc(sizeof(bool));
	*((bool *)ret) = atoi(argv[1]);
	break;
    case uint8_val_setting:
	ret = cc_alloc(sizeof(uint8_t));
	*((uint8_t *)ret) = atoi(argv[1]);
	break;
    case uint32_val_setting:
	ret = cc_alloc(sizeof(uint32_t));
	*((uint32_t *)ret) = atoi(argv[1]);
	break;
    case uint64_val_setting:
	ret = cc_alloc(sizeof(uint64_t));
	*((uint64_t *)ret) = atol(argv[1]);
	break;
    case reltime_val_setting:
	ret = cc_alloc(sizeof(rel_time_t));
	*((rel_time_t *)ret) = atoi(argv[1]);
	break;
    case uint32ptr_val_setting: {
	uint32_t i;

	ret = cc_alloc((argc - 1) * sizeof(uint32_t));
	for(i = 0; i < argc - 1; ++i) {
	    ((uint32_t *)ret)[i] = atoi(argv[i + 1]);
	}
	break;
    }
    default:
	log_debug(LOG_CRIT, "unknown setting type");
	ret = NULL;
	break;
    }

    return ret;
}
