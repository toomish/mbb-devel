/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef POSIX_REGEX_H
#define POSIX_REGEX_H

#include <glib.h>

typedef gpointer Regex;

Regex re_new(gchar *str);
gboolean re_ismatch(Regex re, gchar *str);
void re_free(Regex re);

#endif
