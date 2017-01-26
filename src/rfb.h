/*
 * The Remote Framebuffer (RFB) Protocol - RFC 6143
 */

#ifndef _RFB_H
#define _RFB_H

#include "fvncd.h"
#include <netinet/in.h>

/* protocol steps */
#define RFB_STEP_INIT		0
#define RFB_STEP_VERSION	1	/* handshake - protocol version */
#define RFB_STEP_SECTYPE	2	/* handshake - security type */
#define RFB_STEP_CHALLENGE	3	/* handshake - authentication challenge */
#define RFB_STEP_RESULT		4	/* end of handshake */

/* XXX: MUST be same as each index of g_rfb_supported_versions[] */
#define RFB_VERSION_33 0	/* 3.3 */
#define RFB_VERSION_37 1	/* 3.7 */
#define RFB_VERSION_38 2	/* 3.8 */
#define RFB_VERSION_40 3	/* 4.0 */

/* default protocol version */
#define RFB_DEFAULT_VERSION RFB_VERSION_38

/* the version message format */
#define RFB_VERSIONSTRING	"RFB %03d.%03d\n"
/* the reason for the failure */
#define RFB_REASONSTRING	"Authentication failure"

/* length of a version message */
#define RFB_VERSIONLENGTH	12
/* size of a byte array for the challenge */
#define RFB_CHALLENGESIZE	16
/* length of a string describing the reason for the failure */
#define RFB_REASONLENGTH	((int)sizeof(RFB_REASONSTRING) - 1)

/* security types (14) */
#define RFB_SECTYPE_INVALID		0	/* Invalid */
#define RFB_SECTYPE_NONE		1	/* None */
#define RFB_SECTYPE_VNC			2	/* VNC authentication */
#define RFB_SECTYPE_RA2			5	/* RA2 */
#define RFB_SECTYPE_RA2NE		6	/* RA2ne */
#define RFB_SECTYPE_SSPI		7	/* SSPI */
#define RFB_SECTYPE_SSPINE		8	/* SSPIne */
#define RFB_SECTYPE_TIGHT		16	/* Tight */
#define RFB_SECTYPE_ULTRA		17	/* Ultra */
#define RFB_SECTYPE_TLS			18	/* TLS */
#define RFB_SECTYPE_VENCRYPT		19	/* VeNCrypt */
#define RFB_SECTYPE_SASL		20	/* GTK-VNC SASL */
#define RFB_SECTYPE_MD5			21	/* MD5 hash authentication */
#define RFB_SECTYPE_XVP			22	/* Colin Dean xvp */

/* default security type */
#define RFB_DEFAULT_SECTYPE RFB_SECTYPE_VNC

/* security results */
#define RFB_RESULT_OK		0
#define RFB_RESULT_FAILED	1

/* protocol version */
typedef struct rfb_version_t {
	const int major;
	const int minor;
} RFB_VERSION_T;

/* security type */
typedef struct _rfb_sectype_t {
	const char *name;
	const int num;
} RFB_SECTYPE_T;

/* security message for type VNC (with challenge) in version 3.3 */
typedef struct _rfb_security_33_vnc {
	u_int32_t type;				/* one valid security type */
	u_char challenge[RFB_CHALLENGESIZE];	/* challenge */
} GCC_PACKED RFB_SECURITY_33_VNC;

/* security message for types in version 3.7 onwards */
typedef struct _rfb_security_37on {
	u_int8_t num;		/* number of supported security types */
	u_int8_t types[14];	/* list of supported security types */
} GCC_PACKED RFB_SECURITY_37ON;

/* security result message */
typedef struct _rfb_result {
	u_int32_t status;			/* successful or not? */
	u_int32_t rlength;			/* reason length */
	char rstring[RFB_REASONLENGTH];		/* reason string */
} GCC_PACKED RFB_RESULT;

/*
 * RFB error codes
 *
 * XXX: must be negative numbers less than -1 can be one of return values of write())
 */
#define ERROR_RFB_FATAL				-2
#define ERROR_RFB_INVALID_VERSION_FORMAT	-3
#define ERROR_RFB_UNSUPPORTED_VERSION		-4
#define ERROR_RFB_INVALID_SECTYPE_RESPONSE	-5
#define ERROR_RFB_UNSUPPORTED_SECTYPE		-6
#define ERROR_RFB_INVALID_CHALLENGE_RESPONSE	-7

#include "hcs.h"

extern int rfb_version_str2idx(char *);
extern inline int rfb_lookup_version(const int, const int);
extern inline char *rfb_lookup_sectype(const int);
extern void rfb_setup(void);
extern inline int rfb_send_version(const int, SLOT *);
extern inline int rfb_handshake(const int, SLOT *, const u_char *, const ssize_t);
extern inline int rfb_send_security(const int, SLOT *);
extern inline int rfb_send_challenge(const int, SLOT *);
extern inline int rfb_send_result(const int, SLOT *);
extern inline void rfb_get_challenge(u_char *
#ifdef DEBUG
	, const SLOT *
#endif /* DEBUG */
);
extern inline int rfb_decrypt_challenge(const u_char *, const ssize_t, const SLOT *);

#endif /* !_RFB_H */
