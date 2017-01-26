/*
 * Fake VNC Listener
 * based on RFB protocol
 *
 * authored by snohw <ygescape@gmail.com>
 */

#include "fvncd.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "daemon.h"
#include "listener.h"
#include "hcs.h"
#include "rfb.h"

const char *g_program;	/* program name */

static void initialize(void);
static void finish(int) GCC_NORETURN;

int main(int argc, char **argv) {
	extern char *optarg;
	extern int opterr, optind, optopt;
	extern int g_maxclients;
	extern int g_daemon;
	extern int g_facility;
	extern int g_sectype;
	extern RFB_SECTYPE_T g_rfb_supported_types[];
	extern int g_version;
	extern int g_waittime;
	char *_pt;
	int _op;
	struct sigaction _act, _oact;

	g_program = ((_pt = strrchr(argv[0], '/')) == NULL)
				? argv[0] : (char *)(_pt + sizeof(char));

	opterr = 0;
	while ((_op = getopt(argc, argv, "c:df:ht:v:w:")) != -1) {
		switch (_op) {
			case 'c':	/* max clients possible */
				g_maxclients = atoi(optarg);
				break;

			case 'd':	/* daemon process mode */
				g_daemon = TRUE;
				break;

			case 'f':	/* facility for syslog, in the deaemon process mode */
				g_facility = log_lookup_facilityval(optarg);
				if (g_facility == -1)
					error("invalid facility name");
				break;

			case 'h':	/* help message */
				usage();
				break;

			case 't':	/* default security type */
				{
					RFB_SECTYPE_T *_ptype;

					for (_ptype = g_rfb_supported_types; _ptype->name; _ptype++) {
						if (strlen(_ptype->name) == strlen(optarg) && strcasecmp(_ptype->name, optarg) == 0) {
							g_sectype = _ptype->num;
							break;
						}
					}
					if (_ptype->name == NULL)
						error("unsupported security type");
					break;
				}

			case 'v':	/* default protocol version */
				g_version = rfb_version_str2idx(optarg);
				switch (g_version) {
					case ERROR_RFB_INVALID_VERSION_FORMAT:
						error("invalid protocol version format");
						break;

					case ERROR_RFB_UNSUPPORTED_VERSION:
						error("unsupported protocol version");
						break;

					default:
						break;
				}
				break;

			case 'w':	/* waittime for timeout of TCP connection */
				g_waittime = atoi(optarg);
				break;

			default:
				error("invalid option -- %c", optopt);
				break;
		}
	}
	if (argv[optind] != NULL)
		error("invalid option -- %s", argv[optind]);

	/* run as daemon */
	if (g_daemon) {
		daemonize();
#ifdef DEBUG
		debug("starting daemon");
#endif /* DEBUG */
	}

	/* register signals */
	_act.sa_handler = finish;
	sigfillset(&_act.sa_mask);
	_act.sa_flags = 0;
	(void)sigaction(SIGINT, &_act, NULL);
	(void)sigaction(SIGTERM, &_act, NULL);
	(void)sigaction(SIGHUP, &_act, &_oact);
	if (_oact.sa_handler != SIG_DFL) {
		_act.sa_handler = _oact.sa_handler;
		(void)sigaction(SIGHUP, &_act, NULL);
	}

	/* prepare the service */
	initialize();

	/* launch the service */
	run_listener();

	/* release the resources */
	cleanup();

	return EXIT_SUCCESS;
}

static void initialize(void) {
	/* setup some fixed messages for RFB protocol */
	rfb_setup();

	/* initialize data structure */
	hcs_init();

	return;
}

static void finish(int signo) {
	/* release resources */
	cleanup();

	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}

void cleanup(void) {
	/* close the opened connections */
	close_listener();

	/* release the allocated memories */
	hcs_final();

	/* close the opened syslog */
	log_final();

	return;
}
