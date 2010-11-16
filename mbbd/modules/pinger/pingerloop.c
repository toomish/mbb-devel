#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define __USE_BSD
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#undef __USE_BSD

#include <unistd.h>
#include <string.h>
#include <time.h>
#include <poll.h>

#include "mbblog.h"

#include "pingerloop.h"
#include "strerr.h"
#include "inet.h"

struct icmpmsg {
	struct icmp hdr;
	time_t timestamp;
};

static gint pinger_socket = -1;

guint16 checksum(guint16 *buf, guint len)
{
	guint32 sum = 0;

	while (len > 1) {
		sum += *buf++;
		len -= 2;
	}

	if (len > 0)
		sum += *(guint8 *) buf;

	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

gboolean pinger_send_echo(ipv4_t ip, guint16 id, guint16 seq, time_t t)
{
	struct sockaddr_in sai;
	struct sockaddr *sa;
	struct icmpmsg msg;

	memset(&msg, 0, sizeof(msg));
	msg.hdr.icmp_type = ICMP_ECHO;
	msg.hdr.icmp_id = id;
	msg.hdr.icmp_seq = seq;
	msg.timestamp = t;

	msg.hdr.icmp_cksum = checksum(&msg, sizeof(msg));

	memset(&sa, 0, sizeof(sa));
	sai.sin_family = AF_INET;
	sai.sin_addr.s_addr = ipv4ton(ip);
	sai.sin_port = g_htons(IPPROTO_ICMP);

	sa = &sai;
	if (sendto(pinger_socket, &msg, sizeof(msg), 0, sa, sizeof(sai)) < 0) {
		gchar *tmp = strerr(errno);
		mbb_log("sendto failed: %s", strerr(errno));
		g_free(tmp);

		return FALSE;
	}

	return TRUE;
}

static gboolean pinger_loop_work(gpointer data)
{
	struct pollfd pfd;

	pfd.fd = GPOINTER_TO_INT(data);
	pfd.events = POLLIN;

	if (poll(&pfd, 1, 1000) < 0) {
		gchar *tmp = strerr(errno);
		mbb_log("poll failed: %s", tmp);
		g_free(tmp);

		return FALSE;
	}

gboolean pinger_loop_init(void)
{
	gint fd;

	fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (fd < 0)
		return FALSE;

	pinger_socket = fd;
	return TRUE;
}

gboolean pinger_loop_isrun(void)
{
	return pinger_socket >= 0;
}

void pinger_loop_end(void)
{
	if (pinger_socket >= 0) {
		close(pinger_socket);
		pinger_socket = -1;
	}
}

