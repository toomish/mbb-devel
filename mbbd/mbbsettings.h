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

gboolean readselfconfig(GError **error);

#endif
