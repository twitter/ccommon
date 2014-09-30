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

#ifndef _CC_LOG_H_
#define _CC_LOG_H_

#include <stdbool.h>

#define LOG_MAX_LEN 256 /* max length of log message */

/*
 * The first 8 levels are the same as log levels in syslog(),
 * and we added a few levels of verbose logging because we are chatty.
 *
 * TODO(yao): a reasonable guideline for using these different levels. In fact
 * this many levels is most certainly an overkill for most cases, come back to
 * that later... but I'm inclined to ban the use of LOG_ALERT and LOG_CRIT,
 * merge LOG_NOTICE and LOG_INFO, LOG_DEBUG and LOG_VERB. That'll give us 6
 * levels which is probably enough for any sane person. If only someone can
 * explain to me how syslog() explains these levels.
 */
#define LOG_EMERG   0   /* system in unusable */
#define LOG_ALERT   1   /* action must be taken immediately */
#define LOG_CRIT    2   /* critical conditions */
#define LOG_ERR     3   /* error conditions */
#define LOG_WARN    4   /* warning conditions */
#define LOG_NOTICE  5   /* normal but significant condition (default) */
#define LOG_INFO    6   /* informational */
#define LOG_DEBUG   7   /* debug messages */
#define LOG_VERB    8   /* verbose messages */
#define LOG_VVERB   9   /* verbose messages on crack */

/* NOTE(yao): it may be useful to have a sampled log func for bursty events */
/* TODO(yao): add a config option to completely disable logging above a certain
 * level at compile time.
 */

/*
 * log_stderr   - log to stderr
 *
 * loga         - log always
 * loga_hexdump - log hexdump always
 *
 * log_panic    - log messages followed by a panic, when LOG_EMERG is met
 * log_error    - error log messages
 * log_warn     - warning log messages
 * ...
 *
 * log          - debug log messages based on a log level (subject to config)
 * log_hexdump  - hexadump -C of a log buffer (subject to config)
 */

#define loga(...) _log(__FILE__, __LINE__, -1, __VA_ARGS__)

#define loga_hexdump(_data, _datalen, ...) do {                             \
    _log(__FILE__,__LINE__, -1, __VA_ARGS__);                               \
    _log_hexdump(-1, (char *)(_data), (int)(_datalen));                     \
} while (0)                                                                 \

#define log_panic(...) do {                                                 \
    _log(__FILE__, __LINE__, LOG_EMERG, __VA_ARGS__);                       \
    abort();                                                                \
} while (0)

#if defined CC_LOG && CC_LOG == 1

#define log_emerg(...)  _log(__FILE__, __LINE__, LOG_EMERG, __VA_ARGS__)
#define log_alert(...)  _log(__FILE__, __LINE__, LOG_ALERT, __VA_ARGS__)
#define log_crit(...)   _log(__FILE__, __LINE__, LOG_CRIT, __VA_ARGS__)
#define log_error(...)  _log(__FILE__, __LINE__, LOG_ERR, __VA_ARGS__)
#define log_warn(...)   _log(__FILE__, __LINE__, LOG_WARN, __VA_ARGS__)
#define log_notice(...) _log(__FILE__, __LINE__, LOG_NOTICE, __VA_ARGS__)
#define log_info(...)   _log(__FILE__, __LINE__, LOG_INFO, __VA_ARGS__)
#define log_debug(...)  _log(__FILE__, __LINE__, LOG_DEBUG, __VA_ARGS__)
#define log_verb(...)   _log(__FILE__, __LINE__, LOG_VERB, __VA_ARGS__)
#define log_vverb(...)  _log(__FILE__, __LINE__, LOG_VVERB, __VA_ARGS__)

#define log(_level, ...) _log(__FILE__, __LINE__, _level, __VA_ARGS__)

#define log_hexdump(_level, _data, _datalen, ...) do {                      \
    _log(__FILE__,__LINE__, _level, __VA_ARGS__);                           \
    _log_hexdump(_level, (char *)(_data), (int)(_datalen));                 \
} while (0)

#else

#define log_emerg(...)
#define log_alert(...)
#define log_crit(...)
#define log_error(...)
#define log_warn(...)
#define log_notice(...)
#define log_info(...)
#define log_debug(...)
#define log_verb(...)
#define log_vverb(...)

#define log(_level, ...)
#define log_hexdump(_level, _data, _datalen, ...)

#endif


int log_setup(int level, char *filename);
void log_teardown(void);

void log_level_up(void);
void log_level_down(void);
void log_level_set(int level);

void log_reopen(void);

void log_stderr(const char *fmt, ...);

void _log(const char *file, int line, int level, const char *fmt, ...);
void _log_hexdump(int level, char *data, int datalen);

#endif /* _CC_LOG_H_ */
