#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <poll.h>

#include "mbbxmlmsg.h"
#include "mbbthread.h"
#include "mbbuser.h"
#include "mbbauth.h"
#include "mbbfunc.h"
#include "mbblock.h"
#include "mbblog.h"

#include "mbbinit.h"
#include "mbbvar.h"

#include "http.h"

#include "varconv.h"
#include "debug.h"
#include "net.h"

#define HTTP_CONTENT_LENGTH "Content-Length"

#define MBB_HTTP_HEADER_LOGIN "MBB-Auth-Login"
#define MBB_HTTP_HEADER_SECRET "MBB-Auth-Secret"
#define MBB_HTTP_HEADER_KEY "MBB-Auth-Key"

#define JSON_PREFIX "json/"

struct http_thread_env {
	struct thread_env *te;
	XmlTag *root_tag;
	gchar *method;
	gboolean json;
	FILE *fp;
};

static gchar *http_url_prefix = NULL;
static gsize http_url_prefix_len;

static gchar *http_auth_method = NULL;

static struct mbb_var *hup_var = NULL;
static struct mbb_var *ham_var = NULL;

#define push_http_msg(te, ...) push_http_xml_msg(te, mbb_xml_msg(__VA_ARGS__))
#define push_http_error_msg(te, ...) push_http_xml_msg(te, mbb_xml_msg_error(__VA_ARGS__))

static gchar *http_url_prefix_get(gpointer p);
static gboolean http_url_prefix_set(gchar *arg, gpointer p);

static void add_std_headers(HttpResponse *resp, gchar *type)
{
	gchar *ctype;
	gchar *clen;

	if (resp->body == NULL)
		clen = "0";
	else {
		clen = g_strdup_printf("%" G_GSIZE_FORMAT, strlen(resp->body));
		trash_push(&resp->trash, clen, NULL);
	}

	ctype = g_strdup_printf("application/%s; charset=utf-8", type);
	trash_push(&resp->trash, ctype, NULL);

	http_response_add_header(resp, "Server", "mbbd");
	http_response_add_header(resp, "Content-Type", ctype);
	http_response_add_header(resp, "Content-Length", clen);
}

static void push_http_response(struct http_thread_env *hte, HttpResponse *resp)
{
	gchar *msg;

	add_std_headers(resp, hte->json ? "json" : "xml");

	msg = http_response_to_string(resp);
	http_response_free(resp);

	write_all(hte->te->sock, msg, strlen(msg));

	mbb_log_lvl(MBB_LOG_HTTP, "send: %s", msg);
	g_free(msg);
}

static void push_http_xml_msg(struct http_thread_env *hte, XmlTag *tag)
{
	HttpResponse *resp;

	resp = http_response_new(HTTP_STATUS_OK, NULL);

	if (tag == NULL)
		tag = mbb_xml_msg_ok();

	if (hte->json)
		resp->body = xml_tag_to_json(tag);
	else
		resp->body = xml_tag_to_string_escape(tag);

	xml_tag_free(tag);
	trash_push(&resp->trash, resp->body, NULL);

	push_http_response(hte, resp);
}

static gboolean parse_url(gchar *url, gboolean *json, gchar **method)
{
	if (*url != '/')
		return FALSE;

	mbb_base_var_lock(hup_var);

	if (http_url_prefix == NULL)
		url++;
	else if (! strncmp(url, http_url_prefix, http_url_prefix_len))
		url += http_url_prefix_len;
	else
		url = NULL;

	mbb_base_var_unlock(hup_var);

	if (url == NULL)
		return FALSE;

	if (strncmp(url, JSON_PREFIX, STRSIZE(JSON_PREFIX)))
		*json = FALSE;
	else {
		url += STRSIZE(JSON_PREFIX);
		*json = TRUE;
	}

	if (strlen(url))
		*method = url;
	else
		*method = NULL;

	return TRUE;
}

static inline gchar *mbb_base64_decode(gchar *str)
{
	guchar *data;
	gsize len;

	data = g_base64_decode(str, &len);
	if (len == 0) {
		g_free(data);
		return NULL;
	}

	return (gchar *) data;
}

static gboolean get_base64_header(HttpRequest *req, gchar *name, gchar **value)
{
	gchar *header;
	gchar *tmp;

	header = http_request_get_header(req, name);
	if (header == NULL)
		return TRUE;


	tmp = mbb_base64_decode(header);
	if (tmp == NULL) {
		mbb_log_lvl(MBB_LOG_HTTP, "header %s has invalid value", name);
		return FALSE;
	}

	*value = tmp;
	return TRUE;
}

static gboolean process_http_auth(struct thread_env *te, HttpRequest *req)
{
	gboolean cleanup = FALSE;
	gchar *login, *secret;
	gchar *type;
	gboolean ret;

	login = secret = type = NULL;
	if (! get_base64_header(req, MBB_HTTP_HEADER_KEY, &login))
		return FALSE;

	if (login != NULL)
		type = "key";
	else {
		if (! get_base64_header(req, MBB_HTTP_HEADER_LOGIN, &login))
			return FALSE;

		if (login == NULL) {
			mbb_log_lvl(MBB_LOG_HTTP, "auth header missed");
			return FALSE;
		}

		if (! get_base64_header(req, MBB_HTTP_HEADER_SECRET, &secret)) {
			g_free(login);
			return FALSE;
		}

		mbb_base_var_lock(ham_var);
		type = g_strdup(http_auth_method);
		mbb_base_var_unlock(ham_var);

		cleanup = TRUE;
	}

	ret = mbb_session_auth(&te->ss, login, secret, type);

	g_free(login);
	g_free(secret);

	if (cleanup)
		g_free(type);

	return ret;
}

static void process_request(struct http_thread_env *hte)
{
	gchar *method;
	XmlTag *ans;

	method = hte->method;

	if (method != NULL) {
		if (hte->root_tag == NULL)
			hte->root_tag = xml_tag_newc("request");

		xml_tag_set_attr(hte->root_tag,
			"name", variant_new_static_string(method)
		);

		if (mbb_func_call(method, hte->root_tag, &ans) == FALSE)
			ans = mbb_xml_msg(MBB_MSG_UNKNOWN_METHOD, method);
	} else {
		Variant *var;
		XmlTag *tag;
		XmlTag *xt;

		ans = mbb_xml_msg_ok();
		xt = xml_tag_get_child(hte->root_tag, "request");
		xml_tag_reorder(xt);

		for (; xt != NULL; xt = xt->next) {
			var = xml_tag_get_attr(xt, "name");
			if (var == NULL)
				continue;

			method = variant_get_string(var);
			if (mbb_func_call(method, xt, &tag) == FALSE)
				tag = mbb_xml_msg(MBB_MSG_UNKNOWN_METHOD, method);
			else if (tag == NULL)
				tag = mbb_xml_msg_ok();

			xml_tag_set_attr(tag,
				"origin", variant_new_static_string(method)
			);
			xml_tag_add_child(ans, tag);
		}

		xml_tag_reorder(xml_tag_get_child(ans, "response"));
	}

	push_http_xml_msg(hte, ans);
}

static void process_http(struct http_thread_env *hte, HttpRequest *req)
{
	if (process_http_auth(hte->te, req) == FALSE) {
		push_http_msg(hte, MBB_MSG_UNAUTHORIZED);
		return;
	}

	if (req->body) {
		GError *error = NULL;
		guint len;

		len = strlen(req->body);
		if (! xml_parser_parse(hte->te->parser, req->body, len, &error)) {
			mbb_log_lvl(MBB_LOG_HTTP, "xml_parser failed");
			g_error_free(error);

			push_http_error_msg(hte, "invalid xml");
			return;
		}
	}

	process_request(hte);
}

static gboolean process_http_body(struct http_thread_env *hte, HttpRequest *req)
{
	guint clen = 0;
	gchar *value;

	value = http_request_get_header(req, HTTP_CONTENT_LENGTH);
	if (value == NULL) {
		push_http_msg(hte, MBB_MSG_HEADER_MISSED, HTTP_CONTENT_LENGTH);
		return FALSE;
	}

	if (var_conv_uint(value, &clen) == FALSE) {
		push_http_error_msg(hte,
			"invalid header '%s: %s'", HTTP_CONTENT_LENGTH, value
		);
		return FALSE;
	}

	if (clen) {
		gsize n;

		req->body = g_malloc(clen + 1);
		req->body[clen] = '\0';

		if ((n = fread(req->body, 1, clen, hte->fp)) != (gsize) clen) {
			push_http_error_msg(hte,
				"http body incomplete: recv %d, must be %d",
				n, clen
			);
			return FALSE;
		}

		mbb_log_lvl(MBB_LOG_HTTP, "body: %s", req->body);
	}

	return TRUE;
}

static void http_client_loop(struct http_thread_env *hte)
{
	gchar *line = NULL;
	gsize nbyte = 0;
	HttpRequest *req = NULL;
	gint n;

	while ((n = getline(&line, &nbyte, hte->fp)) >= 0) {
		line[n] = '\0';
		if (n && line[n - 1] == '\n') line[--n] = '\0';
		if (n && line[n - 1] == '\r') line[--n] = '\0';

		if (n) mbb_log_lvl(MBB_LOG_HTTP, "recv: %s", line);

		if (req == NULL) {
			if ((req = http_request_new(line)) == NULL) {
				push_http_msg(hte, MBB_MSG_INVALID_HEADER, line);
				break;
			}

			if (! parse_url(req->url, &hte->json, &hte->method)) {
				push_http_error_msg(hte, "invalid url %s", req->url);
				break;
			}
		} else if (n) {
			if (http_request_add_header(req, line) == FALSE) {
				mbb_log_lvl(MBB_LOG_HTTP, "invalid header");
				push_http_msg(hte, MBB_MSG_INVALID_HEADER, line);
				break;
			}
		} else {
			if (req->method == HTTP_METHOD_POST) {
				if (process_http_body(hte, req) == FALSE)
					break;
			}

			process_http(hte, req);
			break;
		}
	}

	if (req != NULL)
		http_request_free(req);

	if (hte->root_tag != NULL) {
		xml_tag_free(hte->root_tag);
		hte->root_tag = NULL;
	}

	g_free(line);
}

static gboolean process_tag(XmlTag *tag, gpointer data)
{
	struct http_thread_env *hte;

	hte = (struct http_thread_env *) data;

	if (hte->root_tag == NULL && ! strcmp(tag->name, "request"))
		hte->root_tag = tag;
	else
		xml_tag_free(tag);

	return FALSE;
}

void mbb_thread_http_client(struct thread_env *te)
{
	struct http_thread_env hte;

	hte.fp = fdopen(te->sock, "r");
	if (hte.fp == NULL) {
		msg_err("fdopen");
		return;
	}

	hte.te = te;
	hte.root_tag = NULL;
	hte.method = NULL;
	hte.json = FALSE;

	te->parser = xml_parser_new(process_tag, &hte);

	http_client_loop(&hte);

	fclose(hte.fp);
}

static gchar *http_url_prefix_get(gpointer p G_GNUC_UNUSED)
{
	gchar *s;

	if (http_url_prefix == NULL)
		s = g_strdup("/");
	else
		s = g_strdup(http_url_prefix);

	return s;
}

static gboolean http_url_prefix_set(gchar *arg, gpointer p G_GNUC_UNUSED)
{
	if (! var_conv_dir(arg, &http_url_prefix))
		return FALSE;

	http_url_prefix_len = strlen(arg);

	return TRUE;
}

static gchar *http_auth_method_get(gpointer ptr)
{
	gchar *s = *(gchar **) ptr;

	if (s == NULL)
		s = "plain";

	return g_strdup(s);
}

MBB_VAR_DEF(hup_def) {
	.op_read = http_url_prefix_get,
	.op_write = http_url_prefix_set,
	.cap_read = MBB_CAP_ALL,
	.cap_write = MBB_CAP_ROOT
};

MBB_VAR_DEF(ham_def) {
	.op_read = http_auth_method_get,
	.op_write = var_conv_dup,
	.cap_read = MBB_CAP_ALL,
	.cap_write = MBB_CAP_ROOT
};

static void init_vars(void)
{
	hup_var = mbb_base_var_register("http.url.prefix", &hup_def, NULL);
	ham_var = mbb_base_var_register("http.auth.method", &ham_def, &http_auth_method);
}

static void __init init(void)
{
	static struct mbb_init_struct entry = MBB_INIT_VARS;

	mbb_init_push(&entry);
}

