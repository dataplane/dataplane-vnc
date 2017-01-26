#ifndef _DAEMON_H
#define _DAEMON_H

#include "fvncd.h"

/* default syslog facility */
#define DEFAULT_FACILITY LOG_LOCAL0

/* set a bit except TRUE and FALSE */
#define DAEMON_STARTED 0x2

extern void daemonize(void);
extern void log_final(void);
extern int log_lookup_facilityval(const char *);
extern char *log_lookup_facilityname(const int);

#endif /* !_DAEMON_H */
