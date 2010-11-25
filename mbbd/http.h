/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef HTTP_PROCESSING_H
#define HTTP_PROCESSING_H

#include <glib.h>

#include "trash.h"

#define	HTTP_STATUS_OK 			200
#define	HTTP_STATUS_BAD_REQUEST 	400
#define	HTTP_STATUS_UNAUTHORIZED 	401
#define	HTTP_STATUS_FORBIDDEN 		403
#define	HTTP_STATUS_NOT_FOUND 		404
#define HTTP_STATUS_LENGTH_REQUIRED	411

typedef enum {
	HTTP_METHOD_GET,
	HTTP_METHOD_POST
} http_method_t;

typedef struct http_header HttpHeader;
typedef struct http_response HttpResponse;
typedef struct http_request HttpRequest;

struct http_header {
	gchar *name;
	gchar *value;
};

struct http_response {
	guint status;
	gchar *reason;
	gchar *body;

	GQueue headers;
	Trash trash;
};

struct http_request {
	http_method_t method;
	gchar *url;
	gchar *body;

	GHashTable *ht;
};

HttpResponse *http_response_new(guint status, gchar *reason);
void http_response_free(HttpResponse *resp);

gchar *http_response_get_header(HttpResponse *resp, gchar *name);
void http_response_add_header(HttpResponse *resp, gchar *name, gchar *value);
gboolean http_response_set_header(HttpResponse *resp, gchar *name, gchar *value);

gchar *http_response_to_string(HttpResponse *resp);

gchar *http_auth_basic(gchar *name, gchar *pass);

HttpRequest *http_request_new(gchar *request);
void http_request_free(HttpRequest *req);

gboolean http_request_add_header(HttpRequest *req, gchar *line);
gchar *http_request_get_header(HttpRequest *req, gchar *name);

gboolean http_auth_parse(gchar *data, gchar **user, gchar **pass);

#endif
