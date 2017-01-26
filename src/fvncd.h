#ifndef _FVNCD_H
#define _FVNCD_H

#include <stdio.h>
#include <sys/types.h>

#ifdef __GNUC__
#define GCC_UNUSED __attribute__((unused))
#define GCC_NORETURN __attribute__((noreturn))
#define GCC_PACKED __attribute__((packed))
#else
#define GCC_UNUSED
#define GCC_NORETURN
#define GCC_PACKED
#endif /* __GNUC__ */

#ifndef TRUE
#define TRUE	1
#endif /* !TRUE */
#ifndef FALSE
#define FALSE	0
#endif /* !FALSE */

/* fvncd.c */
extern void cleanup(void);

/* usage.c */
extern void usage(void) GCC_NORETURN;

/* util.c */
extern void warning(const char *, ...);
extern void error(const char *, ...) GCC_NORETURN;
#ifdef DEBUG
#define dbgout stderr
extern void debug(const char *, ...);
#define XDATA_WITH_ASCII	TRUE
#define XDATA_WITHOUT_ASCII	FALSE
extern void debug_xdata(const u_char *, const ssize_t, const int);
#endif /* DEBUG */
extern void info(const char *, ...);
extern int isdigit_s(const char *);
#define FREE(p) if ((p)) { free((p)); (p) = NULL; }

#endif /* !_FVNCD_H */
