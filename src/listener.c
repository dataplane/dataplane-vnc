/*
 * VNC Listener Service
 */

#include "fvncd.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <unistd.h>
#include <errno.h>
#include "listener.h"
#include "rfb.h"
#include "hcs.h"

int g_listener_startup_flag = FALSE;
int g_maxclients = DEFAULT_MAXCLIENTS;	/* maximum number of concurrent clients */
int g_totalclients;			/* total number of current connecting clients */
int g_waittime = DEFAULT_WAITTIME;	/* waittime for TCP connection timeout */

struct pollfd *g_client = NULL;

static inline void close_connection(int *, SLOT *, const int, const int);

void run_listener(void) {
	struct pollfd _client[g_maxclients];
	struct addrinfo _hints, *_res, *_rp;
	char _listen[8] = {0};
	int _sock[MAXIFDEV];
	int _nsock;
	int _sockopt = 1;
	int _client_sock;
	struct sockaddr_storage _server_addr, _client_addr;
	socklen_t _server_len, _client_len;
	int _nread;
	u_char _recvbuf[PKTBUFSIZE] = {0};
	char _destbuf[NI_MAXHOST] = {0};
	char _hostbuf[NI_MAXHOST] = {0};
	char _servbuf[NI_MAXSERV] = {0};
	time_t _ts;
	SLOT *_slot;
	int _timeout = g_waittime;
	register int s, i;

	memset(&_hints, 0x00, sizeof(_hints));
	_hints.ai_flags = AI_PASSIVE;
	_hints.ai_family = AF_UNSPEC;	/* do not care if IPv4 or IPv6 */
	_hints.ai_socktype = SOCK_STREAM;

	/* get socket addresses */
	snprintf(_listen, sizeof(_listen), "%d", LISTEN);
	if (getaddrinfo(NULL /* any addresses */, _listen, &_hints, &_res) != 0)
		error("cannot get the host information");

	for (_nsock = 0, _rp = _res; _rp && _nsock < MAXIFDEV; _rp = _rp->ai_next) {
		switch (_rp->ai_family) {
			/* IPv4 */
			case AF_INET:
#ifdef DEBUG
				debug("setting the socket for an IPv4 address");
#endif /* DEBUG */
				break;

			/* IPv6 */
			case AF_INET6:
#ifdef DEBUG
				debug("setting the socket for an IPv6 address");
#endif /* DEBUG */
				break;

			default:
				warning("unknown address family for the socket -- %d", _rp->ai_family);
				continue;
				break;
		}

		if ((_sock[_nsock] = socket(_rp->ai_family, _rp->ai_socktype, _rp->ai_protocol)) == -1) {
			warning("cannot open the socket for protocol family %d", _rp->ai_family);
			continue;
		}

		if (_rp->ai_family == AF_INET6) {
			int _v6only = -1;

#ifdef IPV6_V6ONLY
			/* disable IPv4 mapped addresses */
			_v6only = setsockopt(_sock[_nsock], IPPROTO_IPV6, IPV6_V6ONLY, (int *)&_sockopt, sizeof(_sockopt));
#endif /* IPV6_V6ONLY */

			if (_v6only == -1) {
				warning("cannot make it bind an IPv6 address only");
				close(_sock[_nsock]);
				continue;
			}
		}
		/* allow the socket to be bound to an address that is already in use */
		if (setsockopt(_sock[_nsock], SOL_SOCKET, SO_REUSEADDR, (int *)&_sockopt, sizeof(_sockopt)) == -1)
			warning("cannot allow the socket to be bound immediately");

		if (bind(_sock[_nsock], _rp->ai_addr, _rp->ai_addrlen) == -1) {
			warning("cannot bind the local address on sock#%d", _sock[_nsock]);
			close(_sock[_nsock]);
			continue;
		}

		if (listen(_sock[_nsock], SOMAXCONN) == -1) {
			warning("cannot listen on sock#%d", _sock[_nsock]);
			close(_sock[_nsock]);
			continue;
		}

		_nsock++;
	}

	/* clean up */
	freeaddrinfo(_res);

	if (_nsock) {
		g_listener_startup_flag = TRUE;

		/* make socket to be respond to POLLIN event */
		for (s = 0; s < _nsock; s++) {
			_client[s].fd = _sock[s];
			_client[s].events = POLLIN;
		}

		/* duplicate the pointer to global variable */
		g_client = _client;

#ifdef DEBUG
		debug("listening on port %hu", LISTEN);
#endif /* DEBUG */
	}
	else {
		error("cannot listen on port %hu", LISTEN);
	}

	/* initialize FDs of array (as -1) */
	for (i = _nsock; i < g_maxclients; i++) {
		_client[i].fd = -1;
		_client[i].events = 0x0000;	/* no events yet */
	}

	g_totalclients = 0;
	while (TRUE) {
		/*
		 * timeout: -1 to block indefinitely,
		 * otherwise recommend a value less than or equal the TCP waittime
		 */
		_nread = poll(_client, g_maxclients, POLLTIMEOUT * 1000);
		if (_nread == 0) {
			/* count down to timeout of waittime */
			if ((_timeout -= POLLTIMEOUT) <= 0) {
				time(&_ts);

				for (i = _nsock; i < g_maxclients; i++) {
					/* check waittime expired */
					if ((_slot = hcs_search(i)) != EMPTY
						&& _ts - _slot->time > g_waittime)
						close_connection(&(_client[i].fd), _slot, TRUE, TRUE);
				}

				_timeout = g_waittime;
			}

			usleep(500000);
			continue;
		}
		if (_nread == -1)
			error("polling failed or permission denied");

		/* input event */
		for (s = 0; s < _nsock; s++) {
			if (_client[s].revents & POLLIN) {
				/* count down to timeout of waittime */
				if ((_timeout -= POLLTIMEOUT) <= 0) {
					time(&_ts);

					for (i = _nsock; i < g_maxclients; i++) {
						if (_client[i].fd < 0)
							break;

						/* check waittime expired */
						if ((_slot = hcs_search(i)) != EMPTY
							&& _ts - _slot->time > g_waittime)
							close_connection(&(_client[i].fd), _slot, TRUE, TRUE);
					}

					_timeout = g_waittime;
				}
				else {
					for (i = _nsock; i < g_maxclients; i++) {
						if (_client[i].fd < 0)
							break;
					}
				}

				if (i < g_maxclients) {
					_client_len = sizeof(_client_addr);
					_client_sock = accept(_client[s].fd, (struct sockaddr *)&_client_addr, &_client_len);

					_client[i].fd = _client_sock;
					_client[i].events = (POLLIN | POLLERR);
				}
				else {
					warning("too many clients");
					continue;
				}

				/*
				 * if (i > g_totalclients)
				 * g_totalclients = i;
				 */

				/* new connection */
				if (--_nread <= 0) {
					if (g_totalclients < g_maxclients)
						g_totalclients++;

					/* destination information (address only) */
					_server_len = sizeof(_server_addr);
					getsockname(_client_sock, (struct sockaddr *)&_server_addr, &_server_len);
					memset(_destbuf, 0x00, sizeof(_destbuf));
					if (getnameinfo((struct sockaddr *)&_server_addr, _server_len,
							_destbuf, sizeof(_destbuf),
							NULL, 0,
							NI_NUMERICHOST))
					{
						warning("cannot get destination address from new connection");
						close_connection(&(_client[i].fd), EMPTY, FALSE, TRUE);
						continue;
					}

					/* source information (address and port) */
					memset(_hostbuf, 0x00, sizeof(_hostbuf));
					memset(_servbuf, 0x00, sizeof(_servbuf));
					if (getnameinfo((struct sockaddr *)&_client_addr, _client_len,
							_hostbuf, sizeof(_hostbuf),
							_servbuf, sizeof(_servbuf),
							NI_NUMERICHOST | NI_NUMERICSERV))
					{
						warning("cannot get source address and port from new connection");
						close_connection(&(_client[i].fd), EMPTY, FALSE, TRUE);
						continue;
					}

#ifdef DEBUG
					debug("new connection from %s.%s fd#%d to %s - total#%d",
							_hostbuf, _servbuf, _client[i].fd, _destbuf, g_totalclients);
#endif /* DEBUG */

					/* create a node */
					if ((_slot = hcs_insert(i)) == EMPTY) {
						close_connection(&(_client[i].fd), EMPTY, FALSE, TRUE);
						continue;
					}

					/* <connection information> address family */
					if (_server_addr.ss_family != _client_addr.ss_family) {
						warning("unmatched address family of %s.%s -- %d:%d",
								_hostbuf, _servbuf,
								_server_addr.ss_family, _client_addr.ss_family);
						close_connection(&(_client[i].fd), _slot, FALSE, TRUE);
						continue;
					}
					switch (_client_addr.ss_family) {
						case AF_INET:
						case AF_INET6:
							_slot->af = _client_addr.ss_family;
							break;

						default:
							warning("unknown address family of %s.%s -- %d",
									_hostbuf, _servbuf,
									_client_addr.ss_family);
							close_connection(&(_client[i].fd), _slot, FALSE, TRUE);
							continue;
							break;
					}
					/* <connection information> destination address */
					inet_pton(_slot->af, _destbuf, (void *)&_slot->daddr);
					/* <connection information> source address */
					inet_pton(_slot->af, _hostbuf, (void *)&_slot->saddr);
					/* <connection information> source port */
					_slot->sport = (in_port_t)atoi(_servbuf);

					if (rfb_send_version(_client[i].fd, _slot) < 0) {
						warning("cannot handshake protocol version with %s.%s",
								_hostbuf, _servbuf);
						close_connection(&(_client[i].fd), _slot, FALSE, TRUE);
					}

					continue;
				}
			}
		}

		/* received message */
		for (i = _nsock; i <= g_totalclients + _nsock - 1; i++) {
			if (_client[i].fd < 0)
				continue;

			if (_client[i].revents & (POLLIN | POLLERR)) {
				ssize_t _len;
				int _retcode;

				if ((_slot = hcs_search(i)) == EMPTY) {
					warning("cannot complete handshaking with fd#%d", _client[i].fd);
					close_connection(&(_client[i].fd), EMPTY, FALSE, TRUE);
					continue;
				}

				memset(_hostbuf, 0x00, sizeof(_hostbuf));
				inet_ntop(_slot->af, (void *)&_slot->saddr, _hostbuf, sizeof(_hostbuf));

				memset(_recvbuf, 0x00, sizeof(_recvbuf));
				if ((_len = read(_client[i].fd, _recvbuf, sizeof(_recvbuf))) > 0) {
					if ((_retcode = rfb_handshake(_client[i].fd, _slot, _recvbuf, _len)) < 0) {
						switch (_retcode) {
							case ERROR_RFB_INVALID_VERSION_FORMAT:
							case ERROR_RFB_UNSUPPORTED_VERSION:
								warning("received abnormal version response from %s.%hu",
										_hostbuf, _slot->sport);
#ifdef DEBUG
								debug_xdata(_recvbuf, _len, XDATA_WITH_ASCII);
#endif /* DEBUG */
								break;

							case ERROR_RFB_INVALID_SECTYPE_RESPONSE:
							case ERROR_RFB_UNSUPPORTED_SECTYPE:
								warning("received abnormal security type selected from %s.%hu",
										_hostbuf, _slot->sport);
#ifdef DEBUG
								debug_xdata(_recvbuf, _len, XDATA_WITHOUT_ASCII);
#endif /* DEBUG */
								break;

							case ERROR_RFB_INVALID_CHALLENGE_RESPONSE:
								warning("received abnormal challenge response from %s.%hu",
										_hostbuf, _slot->sport);
#ifdef DEBUG
								debug_xdata(_recvbuf, _len, XDATA_WITHOUT_ASCII);
#endif /* DEBUG */
								break;

							default:
								warning("cannot complete handshaking with %s.%hu",
										_hostbuf, _slot->sport);
								break;
						}

						close_connection(&(_client[i].fd), _slot, FALSE, TRUE);
						continue;
					}

					if (_slot->step == RFB_STEP_RESULT) {
						close_connection(&(_client[i].fd), _slot, FALSE, FALSE);
						continue;
					}

					hcs_update(_slot);
				}
				else {
					switch (_len) {
						/* received FIN (CLOSE_WAIT) */
						case 0:
							close_connection(&(_client[i].fd), _slot, FALSE, TRUE);
							break;

						/* reading error */
						default:
							warning("cannot read from %s.%hu (ErrNo.%d: %s)",
									_hostbuf, _slot->sport,
									errno, strerror(errno));
							break;
					}

					continue;
				}
			}
		}

		usleep(500000);
	}

	return;
}

static inline void close_connection(
	int *fd,
	SLOT *slot,
	const int bytimeout,	/* whether closing connection timed out or not */
	const int byanomaly		/* whether closing connection handshaking abnormally or not */
) {
	usleep(1000);

	if (slot != EMPTY) {
		extern RFB_VERSION_T g_rfb_supported_versions[];
		char _destbuf[NI_MAXHOST];
		char _hostbuf[NI_MAXHOST];
		char _version[4] = {0};
		char _sectype[12] = {0};

		/* destination address */
		memset(_destbuf, 0x00, sizeof(_destbuf));
		inet_ntop(slot->af, (void *)&slot->daddr, _destbuf, sizeof(_destbuf));
		/* source address */
		memset(_hostbuf, 0x00, sizeof(_hostbuf));
		inet_ntop(slot->af, (void *)&slot->saddr, _hostbuf, sizeof(_hostbuf));
		/* protocol version */
		if (slot->version != -1) {
			snprintf(_version, sizeof(_version), "%d.%d",
					g_rfb_supported_versions[slot->version].major,
					g_rfb_supported_versions[slot->version].minor);
		}
		else {
			snprintf(_version, sizeof(_version), "n/a");
		}
		/* security type */
		if (slot->sectype != -1) {
			if (rfb_lookup_sectype(slot->sectype) != NULL) {
				snprintf(_sectype, sizeof(_sectype), "%s",
						rfb_lookup_sectype(slot->sectype));
			}
			else {
				snprintf(_sectype, sizeof(_sectype), "unknown");
			}
		}
		else {
			snprintf(_sectype, sizeof(_sectype), "n/a");
		}

		/*
		 * the resultant output - main information
		 *
		 * TODO: skipped username and password
		 *       because of the decryption problem for the present
		 */
		info("daddr %s;"	/* destination address */
			" saddr %s;"	/* source address */
			" sport %hu;"	/* source port */
			" version %s;"	/* protocol version */
			" sectype %s;"	/* security type */
			"%s",			/* option (whether a benign connection or not) */
			_destbuf,
			_hostbuf,
			slot->sport,
			_version,
			_sectype,
			bytimeout ? " (timeout)" : (byanomaly ? " (anomaly)" : ""));

		/* try to close */
		if ((*fd) < 0) {
			warning("already closed %s.%hu fd#%d",
					_hostbuf, slot->sport, (*fd));
		}
		else if (close(*fd) == -1) {
			warning("cannot close connection from %s.%hu fd#%d%s",
					_hostbuf, slot->sport, (*fd),
					bytimeout ? " (timeout)" : (byanomaly ? " (anomaly)" : ""));
		}
		else {
			if (g_totalclients > 0)
				g_totalclients--;

#ifdef DEBUG
			debug("closed connection from %s.%hu fd#%d%s - total#%d",
					_hostbuf, slot->sport, (*fd),
					bytimeout ? " (timeout)" : (byanomaly ? " (anomaly)" : ""),
					g_totalclients);
#endif /* DEBUG */

			(*fd) = -1;
		}

		hcs_delete(slot);
	}
	else {
		/* try to close */
		if ((*fd) < 0) {
			warning("already closed fd#%d",
					(*fd));
		}
		else if (close(*fd) == -1) {
			warning("cannot close connection from fd#%d%s",
					(*fd),
					bytimeout ? " (timeout)" : (byanomaly ? " (anomaly)" : ""));
		}
		else {
			if (g_totalclients > 0)
				g_totalclients--;

#ifdef DEBUG
			debug("closed connection from fd#%d%s - total#%d",
					(*fd),
					bytimeout ? " (timeout)" : (byanomaly ? " (anomaly)" : ""),
					g_totalclients);
#endif /* DEBUG */

			(*fd) = -1;
		}
	}

	return;
}

void close_listener(void) {
	if (g_listener_startup_flag) {
		register int i;

		for (i = g_maxclients - 1; i >= 0; i--) {
			if (g_client[i].fd > 0) {
				if (close(g_client[i].fd) == -1)
					warning("cannot close connection from fd#%d",
							g_client[i].fd);
				g_client[i].fd = -1;
			}
			g_client[i].events = 0x0000;
		}

		g_listener_startup_flag = FALSE;

#ifdef DEBUG
		debug("done");
#endif /* DEBUG */
	}

	return;
}
