/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef HANDLER_H
#define HANDLER_H

#include "xmltag.h"

#define RETURN_FUNC_NAME(name) \
	if (argc == 0) { \
		*argv = name; \
		return; \
	}

void sync_handler(XmlTag *tag);
XmlTag *sync_handler_get_tag(void);
void sync_handler_cont(void);

void talk_say(gchar *msg, void (*func)(XmlTag *, gpointer), gpointer data);
void talk_say_std(gchar *msg);
void talk_say_stdv(gchar *fmt, ...) G_GNUC_PRINTF(1, 2);

XmlTag *talk_half_say(gchar *msg);

void log_handler(XmlTag *tag);
void kill_handler(XmlTag *tag);

#endif
