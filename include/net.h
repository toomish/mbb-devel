/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_NET_H
#define MBB_NET_H

#include <sys/types.h>

#define INET_ADDR_MAXSTRLEN 64

int tcp_server(char *host, char *service, size_t *addrlen);
int tcp_client(char *host, char *service);
int write_all(int fd, const void *buf, size_t count);
char *sock_get_peername(int sock, char *buf, unsigned int *port);

#endif
