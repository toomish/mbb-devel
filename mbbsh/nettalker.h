#ifndef NET_TALKER_H
#define NET_TALKER_H

#include <glib.h>

#include "xmltag.h"

gboolean talk_init(gchar *host, gchar *service);
void talk_register_handler(gchar *name, void (*func)(XmlTag *));
void talk_get_fields(XmlTag *tag, gchar **result, gchar **desc);
gboolean talk_response_handle(XmlTag *tag);
void talk_write(gchar *buf);
void talk_fini(void);

#endif
