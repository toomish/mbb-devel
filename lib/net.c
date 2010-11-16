#include <string.h>
#include <unistd.h>

#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "debug.h"
#include "net.h"

/* #define AI_FLAGS AI_PASSIVE | AI_NUMERICSERV */
#define AI_FLAGS_SERVER AI_PASSIVE
#define AI_FLAGS_CLIENT 0

int tcp_server(char *host, char *service, size_t *addrlen)
{
	struct addrinfo *result, *rp;
	struct addrinfo hints;
	int retval;
	int sock;
	int tmp;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_FLAGS_SERVER;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	msg_warn("host %s, service %s", host, service);

	retval = -1;
	result = NULL;
	tmp = getaddrinfo(host, service, &hints, &result);
	if (tmp != 0) {
		msg_warn("getaddrinfo: %s", gai_strerror(tmp));
		goto out;
	}

	tmp = 1;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock < 0) {
			msg_err("socket");
			continue;
		}

		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(tmp)) < 0) {
			msg_err("setsockopt");
			close(sock);
			continue;
		}

		if (bind(sock, rp->ai_addr, rp->ai_addrlen) == 0)
			break;

		close(sock);
	}

	if (rp == NULL) {
		msg_warn("unable to bind");
		goto out;
	}

	if (listen(sock, 5) < 0) {
		close(sock);
		goto out;
	}

	if (addrlen != NULL)
		*addrlen = rp->ai_addrlen;
	retval = sock;

out:
	if (result != NULL)
		freeaddrinfo(result);
	return retval;
}

int write_all(int fd, const void *buf, size_t count)
{
	const char *s;
	int n;

	s = buf;
	while ((n = write(fd, buf, count)) > 0) {
		count -= n;
		s += n;
	}

	return s - (const char *) buf;
}

int tcp_client(char *host, char *service)
{
	struct addrinfo *result, *rp;
	struct addrinfo hints;
	int retval;
	int sock;
	int tmp;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_FLAGS_CLIENT;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	retval = -1;
	result = NULL;
	tmp = getaddrinfo(host, service, &hints, &result);
	if (tmp != 0) {
		msg_warn("getaddrinfo: %s", gai_strerror(tmp));
		goto out;
	}

	tmp = 1;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock < 0) {
			msg_err("socket");
			continue;
		}

		if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0)
			break;

		close(sock);
	}

	if (rp == NULL) {
		msg_warn("unable to connect");
		goto out;
	}

	retval = sock;

out:
	if (result != NULL)
		freeaddrinfo(result);
	return retval;
}

char *sock_get_peername(int sock, char *buf, unsigned int *port)
{
	struct sockaddr_storage ss;
	socklen_t addrlen;
	socklen_t cnt;
	in_port_t in_port;
	void *src;
	int af;

	addrlen = sizeof(ss);
	if (getpeername(sock, (struct sockaddr *) &ss, &addrlen) < 0) {
		msg_err("getpeername");
		return NULL;
	}

	af = ss.ss_family;
	if (af == AF_INET) {
		struct sockaddr_in *sa;

		sa = (struct sockaddr_in *) &ss;
		src = &sa->sin_addr;
		cnt = INET_ADDRSTRLEN;
		in_port = sa->sin_port;
	} else if (af == AF_INET6) {
		struct sockaddr_in6 *sa;

		sa = (struct sockaddr_in6 *) &ss;
		src = &sa->sin6_addr;
		cnt = INET6_ADDRSTRLEN;
		in_port = sa->sin6_port;
	} else {
		msg_warn("unknown address family %d", af);
		return NULL;
	}

	if (inet_ntop(af, src, buf, cnt) == NULL) {
		msg_err("inet_ntop");
		return NULL;
	}

	if (port != NULL)
		*port = ntohs(in_port);

	return buf;
}

