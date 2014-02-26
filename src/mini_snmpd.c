/* -----------------------------------------------------------------------------
 * Copyright (C) 2008 Robert Ernst <robert.ernst@linux-solutions.at>
 *
 * This file may be distributed and/or modified under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See COPYING for GPL licensing information.
 */



#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "mini_snmpd.h"


void * run_snmpd(void);

static void handle_signal(int signo)
{
	g_quit = 1;
}

static void handle_udp_client(void)
{
	struct my_sockaddr_t sockaddr;
	my_socklen_t socklen;
	int rv;
	char straddr[my_inet_addrstrlen];

	/* Read the whole UDP packet from the socket at once */
	socklen = sizeof (sockaddr);
	rv = recvfrom(g_udp_sockfd, g_udp_client.packet, sizeof (g_udp_client.packet),
		0, (struct sockaddr *)&sockaddr, &socklen);
	if (g_udp_client.size == -1) {
		lprintf(LOG_WARNING, "could not receive packet on UDP port %d: %m\n",
			g_udp_port);
		return;
	}
	g_udp_client.timestamp = time(NULL);
	g_udp_client.sockfd = g_udp_sockfd;
	g_udp_client.addr = sockaddr.my_sin_addr;
	g_udp_client.port = sockaddr.my_sin_port;
	g_udp_client.size = rv;
	g_udp_client.outgoing = 0;
#ifdef DEBUG
	dump_packet(&g_udp_client);
#endif

	/* Call the protocol handler which will prepare the response packet */
	inet_ntop(g_family, &sockaddr.my_sin_addr, straddr, sizeof(straddr));
	if (snmp(&g_udp_client) == -1) {
		lprintf(LOG_WARNING, "could not handle packet from UDP client %s:%d: %m\n",
			straddr, sockaddr.my_sin_port);
		return;
	} else if (g_udp_client.size == 0) {
		lprintf(LOG_WARNING, "could not handle packet from UDP client %s:%d: ignored\n",
			straddr, sockaddr.my_sin_port);
		return;
	}
	g_udp_client.outgoing = 1;

	/* Send the whole UDP packet to the socket at once */
	rv = sendto(g_udp_sockfd, g_udp_client.packet, g_udp_client.size,
		MSG_DONTWAIT, (struct sockaddr *)&sockaddr, socklen);
	inet_ntop(g_family, &sockaddr.my_sin_addr, straddr, sizeof(straddr));
	if (rv == -1) {
		lprintf(LOG_WARNING, "could not send packet to UDP client %s:%d: %m\n",
			straddr, sockaddr.my_sin_port);
	} else if (rv != g_udp_client.size) {
		lprintf(LOG_WARNING, "could not send packet to UDP client %s:%d: "
			"only %d of %d bytes written\n", straddr,
			sockaddr.my_sin_port, rv, (int) g_udp_client.size);
	}
#ifdef DEBUG
	dump_packet(&g_udp_client);
#endif
}

static void handle_tcp_connect(void)
{
	struct my_sockaddr_t tmp_sockaddr;
	struct my_sockaddr_t sockaddr;
	my_socklen_t socklen;
	client_t *client;
	int rv;
	char straddr[my_inet_addrstrlen];

	/* Accept the new connection (remember the client's IP address and port) */
	socklen = sizeof (sockaddr);
	rv = accept(g_tcp_sockfd, (struct sockaddr *)&sockaddr, &socklen);
	if (rv == -1) {
		lprintf(LOG_ERR, "could not accept TCP connection: %m\n");
		return;
	} else if (rv >= FD_SETSIZE) {
		lprintf(LOG_ERR, "could not accept TCP connection: FD set overflow\n");
		close(rv);
		return;
	}

	/* Create a new client control structure or overwrite the oldest one */
	if (g_tcp_client_list_length >= MAX_NR_CLIENTS) {
		client = find_oldest_client();
		if (client == NULL) {
			lprintf(LOG_ERR, "could not accept TCP connection: internal error");
			exit(EXIT_SYSCALL);
		}
		tmp_sockaddr.my_sin_addr = client->addr;
		tmp_sockaddr.my_sin_port = client->port;
		inet_ntop(g_family, &tmp_sockaddr.my_sin_addr, straddr, sizeof(straddr));
		lprintf(LOG_WARNING, "maximum number of %d clients reached, kicking out %s:%d\n",
			MAX_NR_CLIENTS, straddr, tmp_sockaddr.my_sin_port);
		close(client->sockfd);
	} else {
		client = malloc(sizeof (client_t));
		if (client == NULL) {
			lprintf(LOG_ERR, "could not accept TCP connection: %m");
			exit(EXIT_SYSCALL);
		}
		g_tcp_client_list[g_tcp_client_list_length++] = client;
	}

	/* Now fill out the client control structure values */
	inet_ntop(g_family, &sockaddr.my_sin_addr, straddr, sizeof(straddr));
	lprintf(LOG_DEBUG, "connected TCP client %s:%d\n",
		straddr, sockaddr.my_sin_port);
	client->timestamp = time(NULL);
	client->sockfd = rv;
	client->addr = sockaddr.my_sin_addr;
	client->port = sockaddr.my_sin_port;
	client->size = 0;
	client->outgoing = 0;
}

static void handle_tcp_client_write(client_t *client)
{
	struct my_sockaddr_t sockaddr;
	int rv;
	char straddr[my_inet_addrstrlen];

	/* Send the packet atomically and close socket if that did not work */
	sockaddr.my_sin_addr = client->addr;
	sockaddr.my_sin_port = client->port;
	rv = send(client->sockfd, client->packet, client->size, 0);
	inet_ntop(g_family, &sockaddr.my_sin_addr, straddr, sizeof(straddr));
	if (rv == -1) {
		lprintf(LOG_WARNING, "could not send packet to TCP client %s:%d: %m\n",
			straddr, sockaddr.my_sin_port);
		close(client->sockfd);
		client->sockfd = -1;
		return;
	} else if (rv != client->size) {
		lprintf(LOG_WARNING, "could not send packet to TCP client %s:%d: "
			"only %d of %d bytes written\n", straddr,
			sockaddr.my_sin_port, rv, (int) client->size);
		close(client->sockfd);
		client->sockfd = -1;
		return;
	}
#ifdef DEBUG
	dump_packet(client);
#endif

	/* Put the client into listening mode again */
	client->size = 0;
	client->outgoing = 0;
}

static void handle_tcp_client_read(client_t *client)
{
	struct my_sockaddr_t sockaddr;
	int rv;
	char straddr[my_inet_addrstrlen];

	/* Read from the socket what arrived and put it into the buffer */
	sockaddr.my_sin_addr = client->addr;
	sockaddr.my_sin_port = client->port;
	rv = read(client->sockfd, client->packet + client->size,
		sizeof (client->packet) - client->size);
	inet_ntop(g_family, &sockaddr.my_sin_addr, straddr, sizeof(straddr));
	if (rv == -1) {
		lprintf(LOG_WARNING, "could not read packet from TCP client %s:%d: %m\n",
			straddr, sockaddr.my_sin_port);
		close(client->sockfd);
		client->sockfd = -1;
		return;
	} else if (rv == 0) {
		lprintf(LOG_DEBUG, "disconnected TCP client %s:%d\n",
			straddr, sockaddr.my_sin_port);
		close(client->sockfd);
		client->sockfd = -1;
		return;
	}
	client->timestamp = time(NULL);
	client->size += rv;

	/* Check whether the packet was fully received and handle packet if yes */
	rv = snmp_packet_complete(client);
	if (rv == -1) {
		lprintf(LOG_WARNING, "could not handle packet from TCP client %s:%d: %m\n",
			straddr, sockaddr.my_sin_port);
		close(client->sockfd);
		client->sockfd = -1;
		return;
	} else if (rv == 0) {
		return;
	}
	client->outgoing = 0;
#ifdef DEBUG
	dump_packet(client);
#endif

	/* Call the protocol handler which will prepare the response packet */
	if (snmp(client) == -1) {
		lprintf(LOG_WARNING, "could not handle packet from TCP client %s:%d: %m\n",
			straddr, sockaddr.my_sin_port);
		close(client->sockfd);
		client->sockfd = -1;
		return;
	} else if (client->size == 0) {
		lprintf(LOG_WARNING, "could not handle packet from TCP client %s:%d: ignored\n",
			straddr, sockaddr.my_sin_port);
		close(client->sockfd);
		client->sockfd = -1;
		return;
	}
	client->outgoing = 1;
}



/* -----------------------------------------------------------------------------
 * Main program
 */

void * run_snmpd(void)
{

	int c;

	union {
		struct sockaddr_in sa;
#ifdef __IPV6__
		struct sockaddr_in6 sa6;
#endif
	} sockaddr;
	my_socklen_t socklen;
	struct timeval tv_last;
	struct timeval tv_now;
	struct timeval tv_sleep;
	struct ifreq ifreq;
	int ticks;
	fd_set rfds;
	fd_set wfds;
	int nfds;
	int i;

	/* Prevent TERM and HUP signals from interrupting system calls */
	signal(SIGTERM, handle_signal);
	signal(SIGHUP, handle_signal);
	siginterrupt(SIGTERM, 0);
	siginterrupt(SIGHUP, 0);

	/* Open the syslog connection if needed */
#ifdef SYSLOG
	openlog("mini_snmpd", LOG_CONS | LOG_PID, LOG_DAEMON);
#endif


	/* Print a starting message (so the user knows the args were ok) */
	if (g_bind_to_device[0] != '\0') {
		lprintf(LOG_INFO, "started, listening on port %d/udp and %d/tcp on interface %s\n",
			g_udp_port, g_tcp_port, g_bind_to_device);
	} else {
		lprintf(LOG_INFO, "started, listening on port %d/udp and %d/tcp\n",
			g_udp_port, g_tcp_port);
	}

	/* Store the starting time since we need it for MIB updates */
	if (gettimeofday(&tv_last, NULL) == -1) {
		memset(&tv_last, 0, sizeof (tv_last));
		memset(&tv_sleep, 0, sizeof (&tv_sleep));
	} else {
		tv_sleep.tv_sec = g_timeout / 100;
		tv_sleep.tv_usec = (g_timeout % 100) * 10000;
	}

	/* Build the MIB and execute the first MIB update to get actual values */
	if (mib_build() == -1) {
		exit(EXIT_SYSCALL);
	} else if (mib_update(1) == -1) {
		exit(EXIT_SYSCALL);
	}
#ifdef DEBUG
	dump_mib(g_mib, g_mib_length);
#endif

	/* Open the server's UDP port and prepare it for listening */
	g_udp_sockfd = socket((g_family == AF_INET) ? PF_INET : PF_INET6, SOCK_DGRAM, 0);
	if (g_udp_sockfd == -1) {
		lprintf(LOG_ERR, "could not create UDP socket: %m\n");
		exit(EXIT_SYSCALL);
	}
	if (g_family == AF_INET) {
		sockaddr.sa.sin_family = g_family;
		sockaddr.sa.sin_port = htons(g_udp_port);
		sockaddr.sa.sin_addr = inaddr_any;
		socklen = sizeof(sockaddr.sa);
#ifdef __IPV6__
	} else {
		sockaddr.sa6.sin6_family = g_family;
		sockaddr.sa6.sin6_port = htons(g_udp_port);
		sockaddr.sa6.sin6_addr = in6addr_any;
		socklen = sizeof(sockaddr.sa6);
#endif
	}
	if (bind(g_udp_sockfd, (struct sockaddr *)&sockaddr, socklen) == -1) {
		lprintf(LOG_ERR, "could not bind UDP socket to port %d: %m\n", g_udp_port);
		exit(EXIT_SYSCALL);
	}
	if (g_bind_to_device[0] != '\0') {
		snprintf(ifreq.ifr_ifrn.ifrn_name, sizeof (ifreq.ifr_ifrn.ifrn_name), "%s", g_bind_to_device);
		if (setsockopt(g_udp_sockfd, SOL_SOCKET, SO_BINDTODEVICE, (char *)&ifreq, sizeof(ifreq)) == -1) {
			lprintf(LOG_WARNING, "could not bind UDP socket to device %s: %m\n", g_bind_to_device);
		    exit(EXIT_SYSCALL);    
		}
	}

	/* Open the server's TCP port and prepare it for listening */
	g_tcp_sockfd = socket((g_family == AF_INET) ? PF_INET : PF_INET6, SOCK_STREAM, 0);
	if (g_tcp_sockfd == -1) {
		lprintf(LOG_ERR, "could not create TCP socket: %m\n");
		exit(EXIT_SYSCALL);
	}
	if (g_bind_to_device[0] != '\0') {
		snprintf(ifreq.ifr_ifrn.ifrn_name, sizeof (ifreq.ifr_ifrn.ifrn_name), "%s", g_bind_to_device);
		if (setsockopt(g_tcp_sockfd, SOL_SOCKET, SO_BINDTODEVICE, (char *)&ifreq, sizeof(ifreq)) == -1) {
			lprintf(LOG_WARNING, "could not bind TCP socket to device %s: %m\n", g_bind_to_device);
		    exit(EXIT_SYSCALL);    
		}
	}
	i = 1;
	if (setsockopt(g_tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &c, sizeof (i)) == -1) {
		lprintf(LOG_WARNING, "could not set SO_REUSEADDR on TCP socket: %m\n");
		exit(EXIT_SYSCALL);
	}
	if (g_family == AF_INET) {
		sockaddr.sa.sin_family = g_family;
		sockaddr.sa.sin_port = htons(g_udp_port);
		sockaddr.sa.sin_addr = inaddr_any;
		socklen = sizeof(sockaddr.sa);
#ifdef __IPV6__
	} else {
		sockaddr.sa6.sin6_family = g_family;
		sockaddr.sa6.sin6_port = htons(g_udp_port);
		sockaddr.sa6.sin6_addr = in6addr_any;
		socklen = sizeof(sockaddr.sa6);
#endif
	}
	if (bind(g_tcp_sockfd, (struct sockaddr *)&sockaddr, socklen) == -1) {
		lprintf(LOG_ERR, "could not bind TCP socket to port %d: %m\n", g_tcp_port);
		exit(EXIT_SYSCALL);
	}
	if (listen(g_tcp_sockfd, 128) == -1) {
		lprintf(LOG_ERR, "could not prepare TCP socket for listening: %m\n");
		exit(EXIT_SYSCALL);
	}

	/* Handle incoming connect requests and incoming data */
	while (!g_quit) {
		/* Sleep until we get a request or the timeout is over */
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_SET(g_udp_sockfd, &rfds);
		FD_SET(g_tcp_sockfd, &rfds);
		nfds = (g_udp_sockfd > g_tcp_sockfd) ? g_udp_sockfd : g_tcp_sockfd;
		for (i = 0; i < g_tcp_client_list_length; i++) {
			if (g_tcp_client_list[i]->outgoing) {
				FD_SET(g_tcp_client_list[i]->sockfd, &wfds);
			} else {
				FD_SET(g_tcp_client_list[i]->sockfd, &rfds);
			}
			if (nfds < g_tcp_client_list[i]->sockfd) {
				nfds = g_tcp_client_list[i]->sockfd;
			}
		}
		if (select(nfds + 1, &rfds, &wfds, NULL, &tv_sleep) == -1) {
			if (g_quit) {
				break;
			}
			lprintf(LOG_ERR, "could not select from sockets: %m\n");
			exit(EXIT_SYSCALL);
		}
		/* Determine whether to update the MIB and the next ticks to sleep */
		ticks = ticks_since(&tv_last, &tv_now);
		if (ticks < 0 || ticks >= g_timeout) {
			lprintf(LOG_DEBUG, "updating the MIB (full)\n");
			if (mib_update(1) == -1) {
				exit(EXIT_SYSCALL);
			}
			memcpy(&tv_last, &tv_now, sizeof (tv_now));
			tv_sleep.tv_sec = g_timeout / 100;
			tv_sleep.tv_usec = (g_timeout % 100) * 10000;
		} else {
			lprintf(LOG_DEBUG, "updating the MIB (partial)\n");
			if (mib_update(0) == -1) {
				exit(EXIT_SYSCALL);
			}
			tv_sleep.tv_sec = (g_timeout - ticks) / 100;
			tv_sleep.tv_usec = ((g_timeout - ticks) % 100) * 10000;
		}
#ifdef DEBUG
		dump_mib(g_mib, g_mib_length);
#endif
		/* Handle UDP packets, TCP packets and TCP connection connects */
		if (FD_ISSET(g_udp_sockfd, &rfds)) {
			handle_udp_client();
		}
		if (FD_ISSET(g_tcp_sockfd, &rfds)) {
			handle_tcp_connect();
		}
		for (i = 0; i < g_tcp_client_list_length; i++) {
			if (g_tcp_client_list[i]->outgoing) {
				if (FD_ISSET(g_tcp_client_list[i]->sockfd, &wfds)) {
					handle_tcp_client_write(g_tcp_client_list[i]);
				}
			} else {
				if (FD_ISSET(g_tcp_client_list[i]->sockfd, &rfds)) {
					handle_tcp_client_read(g_tcp_client_list[i]);
				}
			}
		}
		/* If there was a TCP disconnect, remove the client from the list */
		for (i = 0; i < g_tcp_client_list_length; i++) {
			if (g_tcp_client_list[i]->sockfd == -1) {
				g_tcp_client_list_length--;
				if (i < g_tcp_client_list_length) {
					free(g_tcp_client_list[i]);
					memmove(&g_tcp_client_list[i], &g_tcp_client_list[i + 1],
						(g_tcp_client_list_length - i) * sizeof (g_tcp_client_list[i]));
				}
			}
		}
	}

	/* We were killed, print a message and exit */
	lprintf(LOG_INFO, "stopped\n");

	//return EXIT_OK;
}



/* vim: ts=4 sts=4 sw=4 nowrap
 */
