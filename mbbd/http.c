/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "http.h"

#include <strings.h>
#include <string.h>

#define CRLF "\r\n"

HttpResponse *http_response_new(guint status, gchar *reason)
{
	HttpResponse *resp;

	resp = g_new(HttpResponse, 1);
	resp->status = status;
	resp->body = NULL;

	if (status == HTTP_STATUS_OK && reason == NULL)
		reason = "OK";

	resp->reason = reason;
	resp->headers = (GQueue) G_QUEUE_INIT;
	resp->trash = (Trash) TRASH_INITIALIZER;

	return resp;
}

void http_response_free(HttpResponse *resp)
{
	g_queue_foreach(&resp->headers, (GFunc) g_free, NULL);
	trash_empty(&resp->trash);
	g_free(resp);
}

static HttpHeader *http_header_new(gchar *name, gchar *value)
{
	HttpHeader *header;

	header = g_new(HttpHeader, 1);
	header->name = name;
	header->value = value;

	return header;
}

static HttpHeader *headers_get_header(GQueue *queue, gchar *name)
{
	HttpHeader *header;
	GList *list;

	for (list = queue->head; list != NULL; list = list->next) {
		header = (HttpHeader *) list->data;

		if (! strcmp(header->name, name))
			return header;
	}

	return NULL;
}

static gboolean headers_set_header(GQueue *queue, gchar *name, gchar *value)
{
	HttpHeader *header;

	header = headers_get_header(queue, name);

	if (header != NULL) {
		header->value = value;
		return TRUE;
	}

	header = http_header_new(name, value);
	g_queue_push_tail(queue, header);

	return FALSE;
}

gchar *http_response_get_header(HttpResponse *resp, gchar *name)
{
	HttpHeader *header;

	header = headers_get_header(&resp->headers, name);

	if (header == NULL)
		return NULL;
	return header->value;
}

gboolean http_response_set_header(HttpResponse *resp, gchar *name, gchar *value)
{
	return headers_set_header(&resp->headers, name, value);
}

void http_response_add_header(HttpResponse *resp, gchar *name, gchar *value)
{
	HttpHeader *header;

	header = http_header_new(name, value);
	g_queue_push_tail(&resp->headers, header);
}

gchar *http_response_to_string(HttpResponse *resp)
{
	HttpHeader *header;
	GString *string;
	GList *list;

	string = g_string_new("HTTP/1.0");
	g_string_append_printf(string, " %d", resp->status);
	if (resp->reason != NULL)
		g_string_append_printf(string, " %s", resp->reason);
	else
		g_string_append_printf(string, " %d", resp->status);
	g_string_append(string, CRLF);

	for (list = resp->headers.head; list != NULL; list = list->next) {
		header = (HttpHeader *) list->data;

		g_string_append_printf(string, "%s: %s", header->name, header->value);
		g_string_append(string, CRLF);
	}

	g_string_append(string, CRLF);

	if (resp->body != NULL)
		g_string_append(string, resp->body);

	return g_string_free(string, FALSE);
}

gchar *http_auth_basic(gchar *name, gchar *pass)
{
	GString *string;
	gchar *data;

	string = g_string_new(NULL);
	g_string_append(string, name);
	if (pass != NULL)
		g_string_append_printf(string, ":%s", pass);
	data = g_base64_encode((guchar *) string->str, string->len);
	g_string_printf(string, "Basic %s", data);
	g_free(data);

	return g_string_free(string, FALSE);
}

static gchar *http_request_get_url(gchar *request, http_method_t *meth)
{
	gchar **strv;
	gchar *url;

	strv = g_strsplit(request, " ", 0);
	if (g_strv_length(strv) < 2) {
		g_strfreev(strv);
		return FALSE;
	}

	if (! strcmp(strv[0], "POST"))
		*meth = HTTP_METHOD_POST;
	else if (! strcmp(strv[0], "GET"))
		*meth = HTTP_METHOD_GET;
	else {
		g_strfreev(strv);
		return FALSE;
	}

	url = g_strdup(strv[1]);
	g_strfreev(strv);

	return url;
}

static gboolean g_strcase_equal(gconstpointer v1, gconstpointer v2)
{
	return strcasecmp(v1, v2) == 0;
}

static guint g_strcase_hash(gconstpointer v)
{
	gchar buf[strlen(v) + 1];
	const gchar *p = v;
	gchar *pbuf = buf;

	for (; *p != '\0'; p++, pbuf++)
		*pbuf = g_ascii_tolower(*p);
	*pbuf = '\0';

	return g_str_hash(buf);
}

HttpRequest *http_request_new(gchar *request)
{
	http_method_t method;
	HttpRequest *req;
	gchar *url;

	url = http_request_get_url(request, &method);
	if (url == NULL)
		return NULL;

	req = g_new(HttpRequest, 1);
	req->method = method;
	req->url = url;
	req->body = NULL;
	req->ht = g_hash_table_new_full(
		g_strcase_hash, g_strcase_equal, g_free, g_free
	);

	return req;
}

gboolean http_request_add_header(HttpRequest *req, gchar *line)
{
	gchar **strv;

	strv = g_strsplit(line, ": ", 2);
	if (g_strv_length(strv) != 2) {
		g_strfreev(strv);
		return FALSE;
	}

	g_hash_table_replace(req->ht, strv[0], strv[1]);
	g_free(strv);

	return TRUE;
}

gchar *http_request_get_header(HttpRequest *req, gchar *name)
{
	return g_hash_table_lookup(req->ht, name);
}

void http_request_free(HttpRequest *req)
{
	g_hash_table_destroy(req->ht);
	g_free(req->body);
	g_free(req->url);
	g_free(req);
}

gboolean http_auth_parse(gchar *data, gchar **user, gchar **pass)
{
	static gchar *prefix = "Basic ";
	static gsize prefix_len = 0;

	if (!prefix_len)
		prefix_len = strlen(prefix);

	gsize combo_len;
	gchar *combo;
	gchar *s;

	if (strncmp(data, prefix, prefix_len))
		return FALSE;

	data += prefix_len;
	combo = (gchar *) g_base64_decode(data, &combo_len);
	if (combo_len == 0) {
		g_free(combo);
		return FALSE;
	}

	s = strchr(combo, ':');
	if (s == combo) {
		g_free(combo);
		return FALSE;
	}

	if (s == NULL) {
		*pass = NULL;
	} else {
		*s++ = '\0';
		if (*s == '\0')
			*pass = NULL;
		else
			*pass = g_strdup(s);
	}

	*user = g_strdup(combo);
	g_free(combo);

	return TRUE;
}

