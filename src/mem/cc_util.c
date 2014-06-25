#include "cc_util.h"

#include <ctype.h>

static void
mc_skip_space(const char **str, size_t *len)
{
    while(*len > 0 && isspace(**str)) {
	(*str)++;
	(*len)--;
    }
}

bool
mc_strtoull_len(const char *str, uint64_t *out, size_t len)
{
    *out = 0ULL;

    mc_skip_space(&str, &len);

    while (len > 0 && (*str) >= '0' && (*str) <= '9') {
        if (*out >= UINT64_MAX / 10) {
            /*
             * At this point the integer is considered out of range,
             * by doing so we convert integers up to (UINT64_MAX - 6)
             */
            return false;
        }
        *out = *out * 10 + *str - '0';
        str++;
        len--;
    }

    mc_skip_space(&str, &len);

    if (len == 0) {
        return true;
    } else {
        return false;
    }
}
