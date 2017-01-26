/*
 * Utility Functions
 */

#include "fvncd.h"
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <syslog.h>
#include "daemon.h"

extern const char *g_program;
extern int g_daemon;
extern int g_facility;

void warning(const char *msg, ...){
	/* int _errnum = errno; */
	va_list _argptr;

	va_start(_argptr, msg);

	if (g_daemon & DAEMON_STARTED) {
		vsyslog(g_facility | LOG_DEBUG, msg, _argptr);
	}
	else {
		fprintf(stderr, "%s: warning: ", g_program);
		/*
		 * if (_errnum)
		 * fprintf(stderr, "%s: ", strerror(_errnum));
		 */
		vfprintf(stderr, msg, _argptr);
		putc('\n', stderr);
		fflush(stderr);
	}

	va_end(_argptr);

	return;
}

void
error( const char *msg, ...) {
	/* int _errnum = errno; */
	va_list _argptr;

	va_start(_argptr, msg);

	if (g_daemon & DAEMON_STARTED) {
		vsyslog(g_facility | LOG_DEBUG, msg, _argptr);
	}
	else {
		fflush(stdout);
		fprintf(stderr, "%s: error: ", g_program);
		/*
		 * if (_errnum)
		 * fprintf(stderr, "%s: ", strerror(_errnum));
		 */
		vfprintf(stderr, msg, _argptr);
		putc('\n', stderr);
		fflush(stderr);
	}

	va_end(_argptr);

	cleanup();

	exit(EXIT_FAILURE);
	/* NOTREACHED */
}

#ifdef DEBUG
void debug( const char *msg, ...) {
	va_list _argptr;

	va_start(_argptr, msg);

	if (g_daemon & DAEMON_STARTED) {
		vsyslog(g_facility | LOG_DEBUG, msg, _argptr);
	}
	else {
		fprintf(dbgout, "%s: debug: ", g_program);
		vfprintf(dbgout, msg, _argptr);
		putc('\n', dbgout);
		fflush(dbgout);
	}

	va_end(_argptr);

	return;
}

#define _XHUNK 16	/* size of data to print in a line */
#define _XHEX (		/* length of hex data string */		\
	((2 * 2) * (_XHUNK / 2))	/* hex data string */	\
	+							\
	((_XHUNK / 2) - 1)		/* sum of each space */	\
)
#define _XIDENT "\x20"
#define _XSPACE "\x20\x20"
#define _XLINE (		/* total length of a whole line */	\
	sizeof(_XIDENT) - 1	/* indentation */			\
	+ 6			/* offset (0x0000) */			\
	+ 1			/* delimiter (:) */			\
	+ sizeof(_XSPACE) - 1	/* spaces */				\
	+ _XHEX			/* hex */				\
	+ sizeof(_XSPACE) - 1	/* spaces */				\
	+ _XHUNK		/* ASCII */				\
)
#define _XBUF(n) (	 /* buffer size needed for (n) bytes */	\
	(								\
		((n) / _XHUNK)						\
		*							\
		(_XLINE + sizeof(char))					\
	)								\
	+								\
	(								\
		((n) % _XHUNK)						\
		?							\
		_XLINE - _XHUNK + ((n) % _XHUNK) + sizeof(char)		\
		:							\
		sizeof(char) - sizeof(char)				\
	)								\
)

void debug_xdata(const u_char *data, const ssize_t len, const int ascii) {
	char _xdata[_XBUF(len)];
	int _wlen;
	int _offset = 0;
	register int i, j;
	register char *_pdata = _xdata;
	register int c1, c2;

	/* fill with space */
	memset(_xdata, 0x20, sizeof(_xdata));

	for (i = len / 2, j = 1; i > 0; i--, j++) {
		/* offset */
		if (j == 1) {
			*_pdata++ = '\n';

			_wlen = snprintf(_pdata,
							sizeof(_xdata) - (_pdata - _xdata),
							_XIDENT"0x%04x:"_XSPACE,
							_offset);
			_pdata += _wlen;
			*_pdata = 0x20;
		}
		else {
			_pdata++;	/* space */
		}

		c1 = *data++;
		c2 = *data++;

		/* hex */
		_wlen = snprintf(_pdata,
						sizeof(_xdata) - (_pdata - _xdata),
						"%02x%02x",
						c1, c2);
		_pdata += _wlen;
		*_pdata = 0x20;

		/* ASCII */
		if (ascii) {
			*(_pdata
				+ ((_XHEX - ((2 * 2) * j) - (j - 1) - 1)
					+ sizeof(_XSPACE)
					+ ((j * 2) - 2)))
				= (isgraph(c1) ? c1 : '.');
			*(_pdata
				+ ((_XHEX - ((2 * 2) * j) - (j - 1) - 1)
					+ sizeof(_XSPACE)
					+ ((j * 2) - 1)))
				= (isgraph(c2) ? c2 : '.');
		}

		if (j >= _XHUNK / 2) {
			if (ascii)
				_pdata += (sizeof(_XSPACE) + _XHUNK - 1);

			j = 0;
			_offset += _XHUNK;
		}
	}

	if (len & 0x1) {
		/* offset */
		if (j == 1) {
			*_pdata++ = '\n';

			_wlen = snprintf(_pdata,
							sizeof(_xdata) - (_pdata - _xdata),
							_XIDENT"0x%04x:"_XSPACE,
							_offset);
			_pdata += _wlen;
			*_pdata = 0x20;
		}
		else {
			_pdata++;	/* space */
		}

		c1 = *data++;

		/* hex */
		_wlen = snprintf(_pdata,
						sizeof(_xdata) - (_pdata - _xdata),
						"%02x",
						c1);
		_pdata += _wlen;
		*_pdata = 0x20;

		/* ASCII */
		if (ascii) {
			_pdata += ((_XHEX - (((2 * 2) * j) - 2) - (j - 1) - 1)
						+ sizeof(_XSPACE)
						+ ((j * 2) - 2));
			*_pdata = (isgraph(c1) ? c1 : '.');
			_pdata++;
		}
	}
	else if (j-- > 1 && ascii) {
		_pdata += ((_XHEX - ((2 * 2) * j) - (j - 1) - 1)
					+ sizeof(_XSPACE)
					+ (j * 2));
	}
	*_pdata = '\0';

	debug("dumping the raw data of %d byte%s in hex%s...%s",
			len, len > 1 ? "s" : "", ascii ? " and ASCII" : "", _xdata);

	return;
}
#endif /* DEBUG */

void info(const char *msg, ...) {
	va_list _argptr;

	va_start(_argptr, msg);

	if (g_daemon & DAEMON_STARTED) {
		vsyslog(g_facility | LOG_INFO, msg, _argptr);
	}
	else {
		fprintf(stdout, "%s: ", g_program);
		vfprintf(stdout, msg, _argptr);
		putc('\n', stdout);
		fflush(stdout);
	}

	va_end(_argptr);

	return;
}

/*
 * check whether string consists of digit characters or not
 *
 * return values:
 * integer type of the string, if the tests true
 * -1, if the tests false
 */
int isdigit_s(const char *str) {
	char *_pstr = (char *)str;

	do {
		if (isdigit(*_pstr) == 0)
			return -1;
	}
	while (*(++_pstr));

	return atoi(str);
}
