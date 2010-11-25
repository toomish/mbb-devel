/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "nettalker.h"
#include "handler.h"

#include <glib.h>

#include <stdarg.h>

static GMutex *recv_mutex;
static GCond *recv_cond;

static GMutex *cont_mutex;
static GCond *cont_cond;

static GOnce sync_once = G_ONCE_INIT;

static XmlTag *xml_tag = NULL;

static gpointer init_sync(gpointer arg G_GNUC_UNUSED)
{
	recv_mutex = g_mutex_new();
	recv_cond = g_cond_new();

	cont_mutex = g_mutex_new();
	cont_cond = g_cond_new();

	return NULL;
}

void sync_handler(XmlTag *tag)
{
	g_once(&sync_once, init_sync, NULL);

	xml_tag = tag;

	g_mutex_lock(recv_mutex);
	g_cond_signal(recv_cond);
	g_mutex_unlock(recv_mutex);

	g_mutex_lock(cont_mutex);
	while (xml_tag != NULL)
		g_cond_wait(cont_cond, cont_mutex);
	g_mutex_unlock(cont_mutex);
}

XmlTag *sync_handler_get_tag(void)
{
	g_once(&sync_once, init_sync, NULL);

	g_mutex_lock(recv_mutex);
	while (xml_tag == NULL)
		g_cond_wait(recv_cond, recv_mutex);
	g_mutex_unlock(recv_mutex);

	return xml_tag;
}

void sync_handler_cont(void)
{
	xml_tag = NULL;

	g_mutex_lock(cont_mutex);
	g_cond_signal(cont_cond);
	g_mutex_unlock(cont_mutex);
}

void log_handler(XmlTag *tag)
{
	GString *string;
	Variant *var;

	string = g_string_new(NULL);
	var = xml_tag_get_attr(tag, "sid");
	if (var != NULL)
		g_string_append_printf(string, "#%s ", variant_get_string(var));

	var = xml_tag_get_attr(tag, "user");
	if (var != NULL)
		g_string_append_printf(string, "%s@", variant_get_string(var));

	var = xml_tag_get_attr(tag, "peer");
	if (var != NULL)
		g_string_append_printf(string, "%s", variant_get_string(var));

	var = xml_tag_get_attr(tag, "task_id");
	if (var != NULL)
		g_string_append_printf(string, ":%s", variant_get_string(var));

	var = xml_tag_get_attr(tag, "domain");
	if (var != NULL)
		g_string_append_printf(string, "[%s]", variant_get_string(var));

	g_string_append(string, ": ");

	var = xml_tag_path_attr(tag, "message", "value");
	if (var != NULL)
		g_string_append_printf(string, "%s", variant_get_string(var));

	if (string->len > 0)
		g_printerr("LOG: %s\n", string->str);

	g_string_free(string, TRUE);
}

void talk_say(gchar *msg, void (*func)(XmlTag *, gpointer), gpointer data)
{
	XmlTag *tag;

	talk_write(msg);
	tag = sync_handler_get_tag();
	func(tag, data);
	sync_handler_cont();
}

void talk_say_std(gchar *msg)
{
	XmlTag *tag;

	talk_write(msg);
	tag = sync_handler_get_tag();
	talk_response_handle(tag);
	sync_handler_cont();
}

void talk_say_stdv(gchar *fmt, ...)
{
	gchar *request;
	va_list ap;

	va_start(ap, fmt);
	request = g_markup_vprintf_escaped(fmt, ap);
	va_end(ap);

	talk_say_std(request);
	g_free(request);
}

XmlTag *talk_half_say(gchar *msg)
{
	static gboolean cont_wait = FALSE;

	if (cont_wait) {
		cont_wait = FALSE;
		sync_handler_cont();
	}

	if (msg == NULL)
		return NULL;

	cont_wait = TRUE;
	talk_write(msg);

	return sync_handler_get_tag();
}

