/*
 * Message for Program Usage
 */

#include "fvncd.h"
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "daemon.h"
#include "listener.h"
#include "rfb.h"
#include "usage.h"

void usage(void) {
	extern const char *g_program;
	extern RFB_VERSION_T g_rfb_supported_versions[];
	extern RFB_SECTYPE_T g_rfb_supported_types[];
	RFB_VERSION_T *_pver;
	RFB_SECTYPE_T *_ptype;
	char *_fname = NULL;

	fprintf(stdout, "%s", g_program);
#ifdef VERSION
	fprintf(stdout, " version %s", VERSION);
#endif /* VERSION */
#ifdef DEBUG
	fprintf(stdout, " compiled in debug mode");
#endif /* DEBUG */
	fprintf(stdout, "\nsupporting RFB @ protocol versions:");
	for (_pver = g_rfb_supported_versions; _pver->major; _pver++)
		fprintf(stdout, " %d.%d", _pver->major, _pver->minor);
	fprintf(stdout, "\n               @ security types:");
	for (_ptype = g_rfb_supported_types; _ptype->name; _ptype++)
		fprintf(stdout, " %s", _ptype->name);
	fprintf(stdout, "\n");

	fprintf(stdout, "Usage: %s [options]\n", g_program);
	fprintf(stdout, "Options:\n");
	fprintf(stdout, "  -c <clients>   define maximum number of concurrent clients"
			" (default: %d)\n", DEFAULT_MAXCLIENTS);
	fprintf(stdout, "  -d             run as daemon\n");
	fprintf(stdout, "  -f <facility>  specify the facility name for syslog"
			" (default: %s)\n",
			(_fname = log_lookup_facilityname(DEFAULT_FACILITY)) ? _fname : "n/a");
	fprintf(stdout, "  -h             display this information\n");
	fprintf(stdout, "  -t <type>      specify one security type for the version 3.3"
			" (default: %s)\n", rfb_lookup_sectype(RFB_DEFAULT_SECTYPE));
	fprintf(stdout, "  -v <version>   specify the default protocol version"
			" (default: %d.%d)\n",
			g_rfb_supported_versions[RFB_DEFAULT_VERSION].major,
			g_rfb_supported_versions[RFB_DEFAULT_VERSION].minor);
	fprintf(stdout, "  -w <waittime>  define waittime for TCP connection timeout"
			" (default: %d sec)\n", DEFAULT_WAITTIME);

	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}
