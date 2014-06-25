#ifndef _CC_UTIL_H_
#define _CC_UTIL_H_

#include <stdbool.h>
#include <stdlib.h>

#define KB           (1024)
#define MB           (1024 * KB)

#define CC_OK        0
#define CC_ERROR    -1
#define CC_EAGAIN   -2
#define CC_ENOMEM   -3

#define INCR_MAX_STORAGE_LEN 24

typedef int rstatus_t; /* return type */

bool mc_strtoull_len(const char *str, uint64_t *out, size_t len);

#endif 
