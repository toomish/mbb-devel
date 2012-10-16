/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef AUTH_H
#define AUTH_H

#include "xmltag.h"

gboolean auth_plain(const gchar *user, const gchar *pass);
gboolean auth_key(const gchar *key);

void salt_handler(XmlTag *tag);
gboolean auth_salt(const gchar *user, const gchar *pass);

#endif
