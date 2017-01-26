/*
 * Hash Chaining Scheme
 */

#ifndef _HCS_H
#define _HCS_H

#include "fvncd.h"
#include <time.h>
#include <netinet/in.h>

#define TABLE_SIZE 107	/* a prime number */

#define EMPTY NULL

typedef int keysa_t;

/* hash value */
#define HASH(key) ((key) % TABLE_SIZE)

typedef struct _slot SLOT;

#include "rfb.h"

/* a type of address for IPv4 or IPv6 */
typedef union _inaddr46_t {
	/* IPv4 */
	struct {
		struct in_addr v4;
		u_int8_t padding[12];
	} addr4;
	/* IPv6 */
	struct in6_addr v6;
} INADDR46_T;

/* a node for chaining */
struct _slot {
	/* pollfd index */
	keysa_t key;

	/* the latest access time */
	time_t time;

	/* address family (IPv4 or IPv6) */
	int af;

	/* destination (server side) */
	INADDR46_T daddr;			/* destination address */

	/* source (client side) */
	INADDR46_T saddr;			/* source address */
	in_port_t sport;			/* source port */

	/* related to the RFB */
	int step;				/* current RFB protocol step in progress */
	int version;				/* protocol version (index) */
	int sectype;				/* security type selected */
	u_char challenge[RFB_CHALLENGESIZE];	/* challenge from server */

	struct _slot *prev;
	struct _slot *next;
};

/* a bucket of hash table */
typedef struct _bucket {
	SLOT *head;
	SLOT *tail;
} GCC_PACKED BUCKET;

extern void hcs_init(void);
extern inline SLOT *hcs_search(const keysa_t);
extern inline SLOT *hcs_insert(const keysa_t);
extern inline void hcs_update(SLOT *slot);
extern inline void hcs_delete(SLOT *);
extern void hcs_final(void);

#endif /* !_HCS_H */
