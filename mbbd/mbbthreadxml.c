/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

#include "mbbthread.h"
#include "mbbfunc.h"
#include "mbbuser.h"
#include "mbbauth.h"
#include "mbblock.h"
#include "mbblog.h"
#include "mbbxtv.h"

#include "macros.h"
#include "debug.h"

static void push_error_message(MbbMsgQueue *msg_queue, gchar *msg)
{
	gchar *output;

	output = g_markup_printf_escaped(
		"<response result='error' desc='%s'/>", msg
	);

	mbb_msg_queue_push_alloc(msg_queue, output, strlen(output));
}

#define MSG_AUTH_FAILED "<auth result='failed'/>"
#define MSG_AUTH_OK "<auth result='ok'/>"

static void process_auth(struct thread_env *te, XmlTag *tag)
{
	DEFINE_XTVN(xtvars, XTV_AUTH_LOGIN_, XTV_AUTH_SECRET_, XTV_AUTH_TYPE_);
	gchar *login, *secret, *type;

	login = secret = type = NULL;
	xml_tag_scan(tag, xtvars, NULL, &login, &secret, &type);

	if (login == NULL) final {
		mbb_log_lvl(MBB_LOG_XML, "send: %s", MSG_AUTH_FAILED);
		mbb_msg_queue_push_const(
			te->msg_queue, MSG_AUTH_FAILED, STRSIZE(MSG_AUTH_FAILED)
		);
	}

	if (mbb_session_auth(&te->ss, login, secret, type) == FALSE) final {
		mbb_log_lvl(MBB_LOG_XML, "send: %s", MSG_AUTH_FAILED);
		mbb_msg_queue_push_const(
			te->msg_queue, MSG_AUTH_FAILED, STRSIZE(MSG_AUTH_FAILED)
		);
	}

	mbb_log_lvl(MBB_LOG_XML, "send: %s", MSG_AUTH_OK);
	mbb_msg_queue_push_const(
		te->msg_queue, MSG_AUTH_OK, STRSIZE(MSG_AUTH_OK)
	);
}

static void push_response(MbbMsgQueue *msg_queue, gchar *result, gchar *desc)
{
	gchar *output;

	if (desc == NULL)
		output = g_strdup_printf("<response result='%s'/>", result);
	else {
		output = g_strdup_printf(
			"<response result='%s' desc='%s'/>",
			result, desc
		);
	}

	mbb_log_lvl(MBB_LOG_XML, "send: %s", output);
	mbb_msg_queue_push_alloc(msg_queue, output, strlen(output));
}

static void process_request(struct thread_env *te, XmlTag *tag)
{
	gchar *func_name;
	Variant *var;
	XmlTag *ans;

	var = xml_tag_get_attr(tag, "name");
	if (var == NULL) final
		push_response(te->msg_queue, "error", "bad format");

	func_name = variant_get_string(var);

	if (mbb_func_call(func_name, tag, &ans) == FALSE) final
		push_response(te->msg_queue, "error", "no such function");

	if (ans == NULL)
		push_response(te->msg_queue, "ok", NULL);
	else {
		gchar *output;

		output = xml_tag_to_string_escape(ans);
		xml_tag_free(ans);

		mbb_log_lvl(MBB_LOG_XML, "send: %s", output);
		mbb_msg_queue_push_alloc(te->msg_queue, output, strlen(output));
	}
}

static gboolean process_xml(XmlTag *tag, gpointer data)
{
	struct thread_env *te;
	gchar *str;

	te = (struct thread_env *) data;

	str = xml_tag_to_string(tag);
	mbb_log_lvl(MBB_LOG_XML, "recv: %s", str);
	g_free(str);

	if (! strcmp(tag->name, "auth"))
		process_auth(te, tag);
	else if (! strcmp(tag->name, "request")) {
		if (te->ss.user == NULL)
			push_response(te->msg_queue, "error", "unauthorized");
		else
			process_request(te, tag);
	} else
		push_response(te->msg_queue, "error", "unknown query");

	return TRUE;
}

static void xml_client_loop(struct thread_env *te)
{
	GError *error = NULL;
	gboolean send_only;
	struct pollfd pfd;
	char buf[2048];
	int n;

	send_only = FALSE;
	pfd.fd = te->sock;

	for (;;) {
		pfd.events = 0;
		if (send_only == FALSE)
			pfd.events = POLLIN;

		if (! mbb_msg_queue_is_empty(te->msg_queue))
			pfd.events |= POLLOUT;
		else if (send_only)
			break;

		signaller_unblock(te->signaller);
		n = poll(&pfd, 1, -1);
		signaller_block(te->signaller);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			msg_err("poll");
			break;
		} else if (n == 0)
			continue;

		if (pfd.revents & POLLIN) {
			n = read(te->sock, buf, sizeof(buf));
			if (n < 0) {
				msg_err("read");
				break;
			} else if (n == 0)
				break;

			if (! xml_parser_parse(te->parser, buf, n, &error)) {
				mbb_log("xml_parser: %s", error->message);
				g_error_free(error);

				push_error_message(te->msg_queue,
					"invalid xml, kill yourself please"
				);

				send_only = TRUE;
				continue;
			}
		}

		if (pfd.revents & POLLOUT) {
			if (mbb_msg_queue_pop(te->msg_queue) < 0) {
				break;
			}
		}
	}
}

static gint mbb_thread_send(gchar *text, gsize len, gpointer user_data)
{
	gint sock;
	gint n;

	sock = GPOINTER_TO_INT(user_data);
	n = write(sock, text, len);

	return n;
}

void mbb_thread_xml_client(struct thread_env *te)
{
	te->parser = xml_parser_new(process_xml, te);

	te->msg_queue = mbb_msg_queue_new(mbb_thread_send, GINT_TO_POINTER(te->sock));
	te->signaller = signaller_new(SIGUSR1);
	signaller_block(te->signaller);

	xml_client_loop(te);

	mbb_log_unregister();

	mbb_msg_queue_free(te->msg_queue);
	signaller_free(te->signaller);

	close(te->sock);
}

