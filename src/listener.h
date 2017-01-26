#ifndef _LISTENER_H
#define _LISTENER_H

#include "fvncd.h"

/* service port */
#define LISTEN 5900

/* maximum number of interface devices */
#define MAXIFDEV 12

/* buffer size for receiving message */
#define PKTBUFSIZE 1500

/* default maximum clients count */
#define DEFAULT_MAXCLIENTS 1024

/* default waittime for TCP connection timeout (in seconds) */
#define DEFAULT_WAITTIME 20

/* a timeout argument of poll syscall (in seconds) */
#define POLLTIMEOUT 5

extern void run_listener(void);
extern void close_listener(void);

#endif /* !_LISTENER_H */
