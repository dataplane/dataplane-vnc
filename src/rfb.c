/*
 * The Remote Framebuffer (RFB) Protocol - RFC 6143
 *
 *                       handshaking process
 *                    +-----------------------+
 *                    |   PROTOCOL  VERSION   |
 *                    +-----------------------+
 *                                |
 *                                |
 *             +------------------+------------------+
 *             |                                     |
 *          {v3.3}                                {v3.7 onwards}
 *             |                                     |
 *             V                                     V
 * +-----------------------+             +-----------------------+
 * |     SECURITY TYPE     |             |     SECURITY TYPE     |
 * |   (might only take    |             +-----------------------+
 * |    'none' or 'vnc')   |                         |
 * +-{'none'}--+--{'vnc'}--+                         |
 *       |     |   with    |           +-------------+-------------+
 *       |     | CHALLENGE |           |                           |
 *       |     +-----------+        {v3.7}                      {v3.8}
 *       |           |                 |                           |
 *       |           |                 |                           |
 *       |           |         +-------+-------+           +-------+-------+
 *       |           |         |       |       |           |       |       |
 *       |           |     {'none'} {'vnc'} {else}     {'none'} {'vnc'} {else}
 *       |           |         |       |       |           |       |       |
 *       |           |         |       |       |           |       |       |
 *       |<---------)|(--------+       |       +----------)|(-----)|(----->|
 *       |           |                 |                   |       |       |
 *       |           |                 |                   |       |       |
 *       |           |<---------------)|(------------------+       |       |
 *       |           |                 |                           |       |
 *       |           |                 |                           |       |
 *       |           |                 +-------------+-------------+       |
 *       |           |                               |                     |
 *       |           |                               V                     |
 *       |           |                   +-----------------------+         |
 *       |           |                   |       CHALLENGE       |         |
 *       |           |                   +-----------------------+         |
 *       |           |                               |                     |
 *       |           |                               |                     |
 *       |           +---------------+---------------+                     |
 *       |                           |                                     |
 *       |                           V                                     |
 *       |               +-----------------------+                         |
 *       |               |    SECURITY RESULT    |                         |
 *       |               |  (failed on purpose)  |                         |
 *       |               +-----------+--{v3.8}---+                         |
 *       |                           |   with    |                         |
 *       |                           |  REASON   |                         |
 *       |                           +-----------+                         |
 *       |                           |                                     |
 *       |                           V                                     |
 *       +-----------------> connection close! <---------------------------+
 */

#include "fvncd.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "hcs.h"
#include "rfb.h"

int g_version = RFB_DEFAULT_VERSION;	/* RFB protocol version */
int g_sectype = RFB_DEFAULT_SECTYPE;	/* RFB default security type for the version 3.3 */

/* protocol version */
char g_rfb_msg_ver[RFB_VERSIONLENGTH + 1] = {0};
/* security result */
RFB_RESULT g_rfb_msg_res;

/* supported RFB protocol versions */
RFB_VERSION_T g_rfb_supported_versions[] = {
	{3, 3},		/* VNC original */
	{3, 7},		/* VNC extension */
	{3, 8},		/* VNC extension */
/*	{4, 0},	*/	/* RealVNC extension */
	{0, 0}
};

/* supported RFB security types */
RFB_SECTYPE_T g_rfb_supported_types[] = {
/*	{ "none",		RFB_SECTYPE_NONE	}, */	/* VNC original - none authentication */
	{ "vnc",		RFB_SECTYPE_VNC		},	/* VNC original */
	{ "ra2",		RFB_SECTYPE_RA2		},	/* RealVNC extension */
	{ "ra2ne",		RFB_SECTYPE_RA2NE	},	/* RealVNC extension */
/*	{ "sspi",		RFB_SECTYPE_SSPI	}, */
/*	{ "sspine",		RFB_SECTYPE_SSPINE	}, */
	{ "tight",		RFB_SECTYPE_TIGHT	},	/* TightVNC extension */
	{ "ultra",		RFB_SECTYPE_ULTRA	},	/* UltraVNC extension */
	{ "tls",		RFB_SECTYPE_TLS		},
	{ "vencrypt",		RFB_SECTYPE_VENCRYPT	},
	{ "sasl",		RFB_SECTYPE_SASL	},
/*	{ "md5",		RFB_SECTYPE_MD5		}, */
/*	{ "xvp",		RFB_SECTYPE_XVP		}, */
	{ NULL,			-1			}
};

/* security for version 3.7 onwards */
RFB_SECURITY_37ON g_rfb_security_37on;
/* size (in byte) of security message format for version 3.7 onwards */
int g_rfb_security_size_37on;

/* input version string -> index of g_rfb_supported_versions[] */
int rfb_version_str2idx(char *str) {
	char *_num = NULL;
	int _major, _minor;
	const char *_delim = "\x2e";
	char *_saveptr;

	/* check format */
	_num = strtok_r(str, _delim, &_saveptr);	/* major */
	if (_num != NULL) {
		if ((_major = isdigit_s(_num)) != -1) {
			_num = strtok_r(NULL, _delim, &_saveptr);	/* minor */
			if (_num != NULL) {
				if ((_minor = isdigit_s(_num)) != -1) {
					/* if exist more */
					_num = strtok_r(NULL, _delim, &_saveptr);
					if (_num != NULL)
						return ERROR_RFB_INVALID_VERSION_FORMAT;
				}
				else return ERROR_RFB_INVALID_VERSION_FORMAT;
			}
			else return ERROR_RFB_INVALID_VERSION_FORMAT;
		}
		else return ERROR_RFB_INVALID_VERSION_FORMAT;
	}
	else return ERROR_RFB_INVALID_VERSION_FORMAT;

	/* matched index */
	return rfb_lookup_version(_major, _minor);
}

inline int rfb_lookup_version(const int major, const int minor) {
	int _index;

	for (_index = 0; g_rfb_supported_versions[_index].major; _index++)
		if (major == g_rfb_supported_versions[_index].major
			&& minor == g_rfb_supported_versions[_index].minor)
			return _index;

	return ERROR_RFB_UNSUPPORTED_VERSION;
}

inline char * rfb_lookup_sectype(const int tnum) {
	extern RFB_SECTYPE_T g_rfb_supported_types[];
	RFB_SECTYPE_T *_ptype;

	for (_ptype = g_rfb_supported_types; _ptype->name; _ptype++)
		if (_ptype->num == tnum)
			return (char *)_ptype->name;

	return (char *)NULL;
}

/* setup some fixed messages for RFB protocol */
void rfb_setup(void) {
	RFB_SECTYPE_T *_psec;

	/* setup protocol version message (default) */
	snprintf(g_rfb_msg_ver, sizeof(g_rfb_msg_ver), RFB_VERSIONSTRING,
			g_rfb_supported_versions[g_version].major, 
			g_rfb_supported_versions[g_version].minor);

	/* setup security message for version 3.7 onwards */
	for (_psec = g_rfb_supported_types, g_rfb_security_size_37on = 0;
		_psec->name;
		_psec++, g_rfb_security_size_37on++)
		g_rfb_security_37on.types[g_rfb_security_size_37on] = _psec->num;
	g_rfb_security_37on.num = g_rfb_security_size_37on;	/* number */
	g_rfb_security_size_37on++;	/* add size for number field */

	/* setup security result message */
	g_rfb_msg_res.status = htonl(RFB_RESULT_FAILED);
	g_rfb_msg_res.rlength = htonl(RFB_REASONLENGTH);
	strncpy(g_rfb_msg_res.rstring, RFB_REASONSTRING, RFB_REASONLENGTH);

	return;
}

/* send a handshake message of protocol version */
inline int rfb_send_version(const int fd, SLOT *slot) {
#ifdef DEBUG
	char _hostbuf[NI_MAXHOST] = {0};
#endif /* DEBUG */

	if (slot == EMPTY)
		return ERROR_RFB_FATAL;

	slot->step = RFB_STEP_VERSION;

#ifdef DEBUG
	memset(_hostbuf, 0x00, sizeof(_hostbuf));
	inet_ntop(slot->af, (void *)&slot->saddr, _hostbuf, sizeof(_hostbuf)); 
	debug("sending protocol version %d.%d to %s.%hu",
			g_rfb_supported_versions[g_version].major,
			g_rfb_supported_versions[g_version].minor,
			_hostbuf, slot->sport);
#endif /* DEBUG */

	return (int)write(fd, g_rfb_msg_ver, RFB_VERSIONLENGTH);
}

/*
 * send a response message for the received message
 *
 * XXX make it return negative error code if unsuccessful
 */
inline int rfb_handshake(const int fd, SLOT *slot, const u_char *rmsg, const ssize_t rlen) {
	if (slot == EMPTY)
		return ERROR_RFB_FATAL;

	switch (slot->step) {
		/* received the response to protocol version */
		case RFB_STEP_VERSION:
			if (rlen == RFB_VERSIONLENGTH) {
#ifdef DEBUG
				char _hostbuf[NI_MAXHOST] = {0};
#endif /* DEBUG */

				/* null terminate the response */
				/* rmsg[RFB_VERSIONLENGTH] = '\0'; */

				if (strncasecmp((const char *)rmsg, g_rfb_msg_ver, RFB_VERSIONLENGTH) == 0) {
					/* <connection information> protocol version */
					slot->version = g_version;
				}
				/* in case of lower version required by client */
				else {
					int _major = 0, _minor = 0, _index;

					if (sscanf((const char*)rmsg, RFB_VERSIONSTRING, &_major, &_minor) == 2) {
						if ((_index = rfb_lookup_version(_major, _minor)) >= 0) {
							/* <connection information> lower protocol version */
							slot->version = _index;
						}
						else return ERROR_RFB_UNSUPPORTED_VERSION;
					}
					else return ERROR_RFB_INVALID_VERSION_FORMAT;
				}

#ifdef DEBUG
				memset(_hostbuf, 0x00, sizeof(_hostbuf));
				inet_ntop(slot->af, (void *)&slot->saddr, _hostbuf, sizeof(_hostbuf)); 
				debug("received protocol version %d.%d from %s.%hu",
						g_rfb_supported_versions[slot->version].major,
						g_rfb_supported_versions[slot->version].minor,
						_hostbuf, slot->sport);
#endif /* DEBUG */

				return rfb_send_security(fd, slot);
			}
			else return ERROR_RFB_INVALID_VERSION_FORMAT;
			break;

		/* received the response to security type */
		case RFB_STEP_SECTYPE:
			switch (slot->version) {
				/* v3.3 */
				case RFB_VERSION_33:
					switch (slot->sectype) {
						/* none */
						case RFB_SECTYPE_NONE:
							return rfb_send_result(fd, slot);
							break;

						/* vnc (also, received the response to challenge) */
						case RFB_SECTYPE_VNC:
							if (rfb_decrypt_challenge(rmsg, rlen, slot) < 0)
								return ERROR_RFB_INVALID_CHALLENGE_RESPONSE;
							else
								return rfb_send_result(fd, slot);
							break;

						default:
							break;
					}
					break;

				/* v3.7 onwards */
				case RFB_VERSION_37:
				case RFB_VERSION_38:
					if (rlen != 1)
						return ERROR_RFB_INVALID_SECTYPE_RESPONSE;
					/* <connection information> protocol version in v3.7 onwards */
					slot->sectype = (int)rmsg[0];
					switch (slot->sectype) {
						/* none */
						case RFB_SECTYPE_NONE:
							return rfb_send_result(fd, slot);
							break;

						/* vnc */
						case RFB_SECTYPE_VNC:
							return rfb_send_challenge(fd, slot);
							break;

						/* else */
						default:
							if (rfb_lookup_sectype(slot->sectype) != NULL)
								return rfb_send_result(fd, slot);
							else
								return ERROR_RFB_UNSUPPORTED_SECTYPE;
							break;
					}
					break;

				default:
					break;
			}
			break;

		/* received the response to challenge for version 3.7 onwards */
		case RFB_STEP_CHALLENGE:
			if (rfb_decrypt_challenge(rmsg, rlen, slot) < 0)
				return ERROR_RFB_INVALID_CHALLENGE_RESPONSE;
			else
				return rfb_send_result(fd, slot);
			break;

		default:
			break;
	}

	return ERROR_RFB_FATAL;
}

/* send a handshake message of security */
inline int rfb_send_security(const int fd, SLOT *slot) {
	switch (slot->version) {
		/* v3.3 */
		case RFB_VERSION_33:
			/* <connection information> security type in v3.3 */
			slot->sectype = g_sectype;
			switch (slot->sectype) {
				/* vnc */
				case RFB_SECTYPE_VNC: {
						RFB_SECURITY_33_VNC g_rfb_msg_sec;
						u_char _randc[RFB_CHALLENGESIZE];

						/* security type */
						g_rfb_msg_sec.type = htonl(RFB_SECTYPE_VNC);
						/* get random challenge */
						rfb_get_challenge(_randc
#ifdef DEBUG
							, slot
#endif /* DEBUG */
						);
						/* add challenge to the type */
						memcpy(g_rfb_msg_sec.challenge, _randc, RFB_CHALLENGESIZE);

						slot->step = RFB_STEP_SECTYPE;
						return (int)write(fd, &g_rfb_msg_sec, sizeof(RFB_SECURITY_33_VNC));
					}
					break;

				/* none */
				case RFB_SECTYPE_NONE:
				/* might only take 'none' or 'vnc' in the version 3.3, otherwise 'none' by default */
				default: {
						u_int32_t _type = htonl(RFB_SECTYPE_NONE);
						
						slot->step = RFB_STEP_SECTYPE;
						return (int)write(fd, &_type, sizeof(_type));
					}
					break;
			}
			break;

		/* v3.7 onwards */
		case RFB_VERSION_37:
		case RFB_VERSION_38:
			slot->step = RFB_STEP_SECTYPE;
			return (int)write(fd, &g_rfb_security_37on, g_rfb_security_size_37on);
			break;

		default:
			break;
	}

	return ERROR_RFB_FATAL;
}

/* send a handshake message of challenge */
inline int rfb_send_challenge(const int fd, SLOT *slot) {
	u_char _randc[RFB_CHALLENGESIZE];

	/* get random challenge */
	rfb_get_challenge(_randc
#ifdef DEBUG
		, slot
#endif /* DEBUG */
	);

	slot->step = RFB_STEP_CHALLENGE;
	return (int)write(fd, _randc, RFB_CHALLENGESIZE);
}

/* send a handshake message of security result */
inline int rfb_send_result(const int fd, SLOT *slot) {
	slot->step = RFB_STEP_RESULT;

	if (slot->sectype != RFB_SECTYPE_NONE
		&& slot->sectype != RFB_SECTYPE_VNC)
		return 0;

	switch (slot->version) {
		/* v3.7 downwards */
		case RFB_VERSION_33:
		case RFB_VERSION_37:
			/* bypass the result message if 'none' */
			if (slot->sectype == RFB_SECTYPE_NONE)
				return 0;
			return (int)write(fd, &g_rfb_msg_res, sizeof(g_rfb_msg_res.status));
			break;

		/* v3.8 */
		case RFB_VERSION_38:
			return (int)write(fd, &g_rfb_msg_res, sizeof(RFB_RESULT));
			break;

		default:
			break;
	}

	return ERROR_RFB_FATAL;
}

/* get random challenge for authentication */
inline void rfb_get_challenge( u_char *randc
#ifdef DEBUG
	, const SLOT *slot
#endif /* DEBUG */
) {
	time_t _ts;
	register int i;
#ifdef DEBUG
	char _hexs[(RFB_CHALLENGESIZE * 2) + RFB_CHALLENGESIZE + 1];
	char _hostbuf[NI_MAXHOST] = {0};
#endif /* DEBUG */

	time(&_ts);
	srandom(_ts);

	for (i = 0; i < RFB_CHALLENGESIZE; i++)
		randc[i] = (u_char)(random() & 255);

#ifdef DEBUG
	for (i = 0; i < RFB_CHALLENGESIZE; i++)
		snprintf(_hexs + (i * 3), 4, "%02x:", randc[i]);
	_hexs[(i * 3) - 1] = '\0';
	memset(_hostbuf, 0x00, sizeof(_hostbuf));
	inet_ntop(slot->af, (void *)&slot->saddr, _hostbuf, sizeof(_hostbuf)); 
	debug("random challenge %s to %s.%hu", _hexs,
			_hostbuf, slot->sport);
#endif /* DEBUG */

	return;
}

/* TODO: decrypt password (or also username) from challenge response */
inline int rfb_decrypt_challenge(const u_char *rmsg, const ssize_t rlen, const SLOT *slot) {
	switch (slot->sectype) {
		/* vnc */
		case RFB_SECTYPE_VNC:
			if (rlen == RFB_CHALLENGESIZE) {
#ifdef DEBUG
				char _hexs[(RFB_CHALLENGESIZE * 2) + RFB_CHALLENGESIZE + 1];
				char _hostbuf[NI_MAXHOST] = {0};
				register int i;

				for (i = 0; i < RFB_CHALLENGESIZE; i++)
					snprintf(_hexs + (i * 3), 4, "%02x:", rmsg[i]);
				_hexs[(i * 3) - 1] = '\0';
				memset(_hostbuf, 0x00, sizeof(_hostbuf));
				inet_ntop(slot->af, (void *)&slot->saddr, _hostbuf, sizeof(_hostbuf)); 
				debug("challenge response %s from %s.%hu", _hexs,
						_hostbuf, slot->sport);
#endif /* DEBUG */
			}
			else return ERROR_RFB_INVALID_CHALLENGE_RESPONSE;
			break;

		default:
			break;
	}

	return 0;
}
