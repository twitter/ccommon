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
#include <cc_sds.h>
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
 */


/*
 * Macro used to declare each individual setting in struct settings
 */
#define SETTINGS_DECLARE(_name, _required, _type, _dynamic, _default, _description) \
    struct setting _name;

/* Initialize settings members */
#define SETTINGS_INIT(_name, _required, _type, _dynamic, _default, _description) \
    ._name = {.initialized = false, .val._type = _default},

/* Print out a description of each setting */
#define SETTINGS_PRINT(_name, _required, _type, _dynamic, _default, _description) \
    loga(#_name ": %s", _description);


/* Union containing payload for setting */
union val_u {
    bool bool_val;
    uint8_t uint8_val;
    uint32_t uint32_val;
    uint64_t uint64_val;
    rel_time_t reltime_val;
    uint32_t *uint32ptr_val;
};

/* Struct containing data for one individual setting */
struct setting {
    bool initialized;
    union val_u val;
};

/* Enum used to match setting to type in order to set values */
typedef enum settings_type {
    bool_val_setting,
    uint8_val_setting,
    uint32_val_setting,
    uint64_val_setting,
    reltime_val_setting,
    uint32ptr_val_setting,
} settings_type_t;

struct setting_val {
    settings_type_t type;
    void *val;
};

rstatus_t settings_set(struct setting *setting, struct setting_val *val);

void *settings_str_to_val(settings_type_t type, int32_t argc, sds *argv);

#endif
