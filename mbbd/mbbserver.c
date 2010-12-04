/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <sys/socket.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>

#include "mbbthread.h"
#include "mbblock.h"
#include "mbbuser.h"
#include "mbblog.h"

#include "debug.h"
#include "net.h"

struct thread_param {
	GStaticMutex mutex;
	gboolean http;
	int sock;
};

static gpointer mbb_thread_client(struct thread_param *tp);

static GStaticPrivate thread_key = G_STATIC_PRIVATE_INIT;

static gboolean start_thread(int listen_sock, gboolean http)
{
	static struct thread_param tp = { G_STATIC_MUTEX_INIT, -1, FALSE };

	GThread *thread;
	int fd;

	fd = accept(listen_sock, NULL, NULL);
	if (fd < 0) {
		msg_err("accept");
		return FALSE;
	}

	g_static_mutex_lock(&tp.mutex);
	tp.sock = fd;
	tp.http = http;
	thread = g_thread_create((GThreadFunc) mbb_thread_client, &tp, FALSE, NULL);
	if (thread == NULL) {
		msg_err("g_thread_create failed");
		close(fd);

		g_static_mutex_unlock(&tp.mutex);
	}

	return TRUE;
}

static void poll_accept(int xml_sock, int http_sock)
{
	struct pollfd pfd[2];
	int n;

	pfd[0].fd = xml_sock;
	pfd[1].fd = http_sock;

	pfd[0].events = POLLIN;
	pfd[1].events = POLLIN;

	for (;;) {
		pfd[0].revents = 0;
		pfd[1].revents = 0;

		n = poll(pfd, 2, -1);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			msg_err("poll");
			break;
		} else if (n == 0)
			continue;

		if (pfd[0].revents & POLLIN) {
			if (start_thread(xml_sock, FALSE) == FALSE)
				break;
		}

		if (pfd[1].revents & POLLIN) {
			if (start_thread(http_sock, TRUE) == FALSE)
				break;
		}
	}

	close(xml_sock);
	close(http_sock);
}

void mbb_server(char *host, char *service, char *http_port)
{
	int xml_sock;

	xml_sock = tcp_server(host, service, NULL);
	if (xml_sock < 0)
		err_quit("tcp_server %s:%s failed", host, service);

	if (http_port != NULL) {
		int http_sock;

		http_sock = tcp_server(host, http_port, NULL);
		if (http_sock < 0) {
			close(xml_sock);
			err_quit("tcp_server %s:%s failed", host, http_port);
		}

		poll_accept(xml_sock, http_sock);
	} else {
		while (start_thread(xml_sock, FALSE));

		close(xml_sock);
	}
}

mbb_cap_t mbb_thread_get_cap(void)
{
	struct mbb_user *user;

	if ((user = mbb_thread_get_user()) != NULL)
		return user->cap_mask;

	return 0;
}

gint mbb_thread_get_tid(void)
{
	struct thread_env *te;

	te = g_static_private_get(&thread_key);
	if (te == NULL)
		return -1;

	return te->sid;
}

gint mbb_thread_get_uid(void)
{
	struct mbb_user *user;

	if ((user = mbb_thread_get_user()) != NULL)
		return user->id;

	return -1;
}

struct mbb_user *mbb_thread_get_user(void)
{
	struct thread_env *te;

	te = g_static_private_get(&thread_key);
	if (te == NULL)
		return NULL;

	return te->ss.user;
}

gchar *mbb_thread_get_peer(void)
{
	struct thread_env *te;

	te = g_static_private_get(&thread_key);
	if (te == NULL)
		return NULL;
	return te->ss.peer;
}

MbbMsgQueue *mbb_thread_get_msg_queue(void)
{
	struct thread_env *te;

	te = g_static_private_get(&thread_key);
	if (te == NULL)
		return NULL;
	return te->msg_queue;
}

Signaller *mbb_thread_get_signaller(void)
{
	struct thread_env *te;

	te = g_static_private_get(&thread_key);
	if (te == NULL)
		return NULL;
	return te->signaller;
}

struct mbb_session *current_session(void)
{
	struct thread_env *te;

	te = g_static_private_get(&thread_key);
	if (te == NULL)
		return NULL;
	return &te->ss;
}

static gpointer mbb_thread_client(struct thread_param *tp)
{
	struct thread_env te;
	gchar buf[INET_ADDR_MAXSTRLEN];
	gboolean http;

	http = tp->http;
	te.sock = tp->sock;
	g_static_mutex_unlock(&tp->mutex);

	te.ss.user = NULL;
	te.ss.port = 0;
	te.ss.peer = g_strdup(sock_get_peername(te.sock, buf, &te.ss.port));

	te.ss.type = http ? MBB_SESSION_HTTP : MBB_SESSION_XML;

	te.parser = NULL;
	te.msg_queue = NULL;
	te.signaller = NULL;

	g_static_private_set(&thread_key, &te, NULL);
	te.sid = mbb_session_new(&te.ss);

	mbb_log("new %s session from %s:%d",
		http ? "http" : "xml", te.ss.peer, te.ss.port
	);

	if (! http)
		mbb_thread_xml_client(&te);
	else
		mbb_thread_http_client(&te);

	mbb_log("exit");

	mbb_session_quit(te.sid);

	if (te.parser != NULL)
		xml_parser_free(te.parser);

	g_free(te.ss.peer);

	return NULL;
}

