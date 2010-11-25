/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_AUTH_H
#define MBB_AUTH_H

#include "mbbuser.h"

typedef MbbUser *(*mbb_auth_func_t)(gchar *login, gchar *secret);

MbbUser *mbb_auth(gchar *login, gchar *secret, gchar *type);
MbbUser *mbb_auth_plain(gchar *login, gchar *secret);

gpointer mbb_auth_method_register(gchar *name, mbb_auth_func_t func);
void mbb_auth_method_unregister(gpointer method_key);

#endif
