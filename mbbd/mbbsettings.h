/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_SETTINGS_H
#define MBB_SETTINGS_H

#include <glib.h>

struct mbb_settings {
	gchar *host;
	gchar *port;
	gchar *http_port;
	gchar *dbtype;

	gchar *db_host;
	gchar *db_name;
	gchar *db_user;
	gchar *db_pass;

	GSList *modules;
};

extern struct mbb_settings settings;

gboolean readselfconfig(gchar *path, GError **error);

#endif
