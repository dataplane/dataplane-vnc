#include "fvncd.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#define SYSLOG_NAMES
#include <syslog.h>
#include "daemon.h"

static volatile sig_atomic_t g_sigflag = 0;

int g_daemon = FALSE;			/* flag whether running as daemon or not */
int g_facility = DEFAULT_FACILITY;	/* facility name for syslog */

static void signal_handler(int);
static void tell_parent(pid_t);
static void wait_for_child(pid_t);
static void log_init(void);

void daemonize(void) {
	extern const char *g_program;
	pid_t _pid, _sid;
	struct sigaction _act;

	/* register user signal */
	_act.sa_handler = signal_handler;
	sigemptyset(&_act.sa_mask);
	_act.sa_flags = 0;
	(void)sigaction(SIGUSR1, &_act, NULL);

	if ((_pid = fork()) < 0)
		error("cannot fork the daemon process");

	/* child */
	if (_pid == 0) {
		umask(027);

		if ((_sid = setsid()) < 0)
			error("cannot create new session");

		/* disable terminal outputs */
		log_init();

		g_daemon |= DAEMON_STARTED;

		/* setlogmask(LOG_UPTO(LOG_INFO)); */
		openlog(g_program, LOG_NDELAY | LOG_PID, g_facility);

		/* tell the parent, the child is ready */
		tell_parent(getppid());

		return;
	}

	/* parent */
	wait_for_child(_pid);

	exit(EXIT_SUCCESS);
	/* NOTREACHED */
	return;
}

static void signal_handler(int signo) {
	g_sigflag = 1;

	return;
}

static void tell_parent(pid_t pid) {
	kill(pid, SIGUSR1);

	return;
}

static void wait_for_child(pid_t pid) {
	int _status;

	while (g_sigflag == 0) {
		if (waitpid(pid, &_status, WNOHANG)) {
			/* check whether child is still there */
			if (WIFEXITED(_status) || WIFSIGNALED(_status))
				error("child died unexpectedly");
		}

		sleep(1);
	}

	return;
}

static void log_init(void) {
    int _fd0, _fd1, _fd2;

    /* close stdin, stdout, stderr */
    close(0);
    close(1);
    close(2);

    /* redirect stdin, stdout, stderr to /dev/null  */
    _fd0 = open("/dev/null", O_RDWR);
    _fd1 = dup(0);
    _fd2 = dup(0);

	return;
}

void log_final(void) {
	if (g_daemon & DAEMON_STARTED) {
		closelog();

		g_daemon ^= DAEMON_STARTED;
	}

	return;
}

/* declared in syslog.h */
#define g_facilitynames facilitynames

int log_lookup_facilityval(const char *fname) {
	CODE *_pcode;

	for (_pcode = g_facilitynames; _pcode->c_name; _pcode++)
		if (strlen(_pcode->c_name) == strlen(fname)
			&& strcasecmp(_pcode->c_name, fname) == 0)
			return (int)_pcode->c_val;

	return (int)-1;
}

char * log_lookup_facilityname(const int fval) {
	CODE *_pcode;

	for (_pcode = g_facilitynames; _pcode->c_name; _pcode++)
		if (_pcode->c_val == fval)
			return (char *)_pcode->c_name;

	return (char *)NULL;
}
