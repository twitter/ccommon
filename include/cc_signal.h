#ifndef _CC_SIGNAL_H_
#define _CC_SIGNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <cc_define.h>

#include <inttypes.h>

#define SIGNAL_MIN 1
#define SIGNAL_MAX 31

#ifndef sig_t
typedef void (*sig_t)(int);
#endif

struct signal {
    char *info;
    int flags;
    sig_t handler;
    uint32_t mask;  /* additional singals to mask */
};

/**
 * to customize signal handling, users are suppose to overwrite entries in
 * signals after it is initialized.
 *
 * Note: the library has already provided handler for the following signals:
 * - SIGHUP: reload config file
 * - SIGTTIN: reload log file
 * - SIGSEGV: print stacktrace before reraise segfault again
 * - SIGPIPE: ignored, this prevents service from exiting when pipe closes
 */
struct signal signals[SIGNAL_MAX]; /* there are only 31 signals from 1 to 31 */

int signal_override(int signo, char *info, int flags, uint32_t mask, sig_t handler);

int signal_pipe_ignore(void);

int signal_segv_stacktrace(void);

int signal_ttin_logrotate(void);

#ifdef __cplusplus
}
#endif

#endif /* _CC_SIGNAL_H_ */
