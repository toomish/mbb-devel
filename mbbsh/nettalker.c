/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "xmlparser.h"
#include "debug.h"
#include "net.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib.h>

struct talk_handler {
	gchar *name;
	void (*func)(XmlTag *tag);
};

static gint sock_fd = -1;
static GStaticMutex thread_mutex = G_STATIC_MUTEX_INIT;
static GThread *thread;
GSList *handler_list = NULL;

static gpointer talk_thread(gpointer arg);

static gboolean fd_set_cloexec(gint fd)
{
	glong set;

	set = fcntl(fd, F_GETFD);
	if (set < 0) {
		msg_err("fcntl");
		return FALSE;
	}

	set |= FD_CLOEXEC;
	if (fcntl(fd, F_SETFD, set) < 0) {
		msg_err("fcntl");
		return FALSE;
	}

	return TRUE;
}

gboolean talk_init(gchar *host, gchar *service)
{
	if (thread != NULL)
		err_quit("talk is already initialized");

	sock_fd = tcp_client(host, service);
	if (sock_fd < 0)
		return FALSE;

	if (fd_set_cloexec(sock_fd) == FALSE) {
		close(sock_fd);
		return FALSE;
	}

	thread = g_thread_create(talk_thread, NULL, TRUE, NULL);
	if (thread == NULL) {
		close(sock_fd);
		msg_warn("g_thread_create failed");
		return FALSE;
	}

	return TRUE;
}

void talk_register_handler(gchar *name, void (*func)(XmlTag *))
{
	struct talk_handler *th;

	th = g_new(struct talk_handler, 1);
	th->name = name;
	th->func = func;

	handler_list = g_slist_prepend(handler_list, th);
}

static gboolean talk_dispatcher(XmlTag *tag, gpointer user_data G_GNUC_UNUSED)
{
	struct talk_handler *th;
	GSList *list;

	g_static_mutex_lock(&thread_mutex);

	for (list = handler_list; list != NULL; list = list->next) {
		th = (struct talk_handler *) list->data;
		if (!strcmp(tag->name, th->name)) {
			g_static_mutex_unlock(&thread_mutex);
			th->func(tag);
			return TRUE;
		}
	}

	g_static_mutex_unlock(&thread_mutex);
	g_printerr("WARN: unhandled message '%s'\n", tag->name);

	return TRUE;
}

static gpointer talk_thread(gpointer arg G_GNUC_UNUSED)
{
	GError *error = NULL;
	XmlParser *parser;
	gchar buf[2048];
	gint n;

	parser = xml_parser_new(talk_dispatcher, NULL);

	while ((n = read(sock_fd, buf, sizeof(buf))) > 0) {
		if (xml_parser_parse(parser, buf, n, &error) == FALSE)
			err_quit("xml_parser_parse: %s", error->message);
	}

	xml_parser_free(parser);
	return NULL;
}

void talk_fini(void)
{
	if (thread == NULL)
		err_quit("talk is already finalized");

	close(sock_fd);

	g_static_mutex_lock(&thread_mutex);

	g_slist_foreach(handler_list, (GFunc) g_free, NULL);
	g_slist_free(handler_list);
	handler_list = NULL;

	g_static_mutex_unlock(&thread_mutex);
}

void talk_write(gchar *buf)
{
	write_all(sock_fd, buf, strlen(buf));
}

void talk_get_fields(XmlTag *tag, gchar **result, gchar **desc)
{
	Variant *var;

	if (strcmp(tag->name, "response"))
		err_quit("invalid response: %s", tag->name);

	var = xml_tag_get_attr(tag, "result");
	if (var == NULL)
		err_quit("invalid response: attr 'result' missed");
	*result = variant_get_string(var);

	var = xml_tag_get_attr(tag, "desc");
	if (var == NULL)
		*desc = NULL;
	else
		*desc = variant_get_string(var);
}

gboolean talk_response_handle(XmlTag *tag)
{
	gchar *result, *desc;

	talk_get_fields(tag, &result, &desc);
	if (!strcmp(result, "ok"))
		return TRUE;

	g_print("ERR: method failed");
	if (desc != NULL)
		g_print(": %s", desc);
	g_print("\n");

	return FALSE;
}

