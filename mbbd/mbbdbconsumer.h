/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_DB_CONSUMER_H
#define MBB_DB_CONSUMER_H

#include <glib.h>
#include <time.h>

gint mbb_db_consumer_add(gchar *name, time_t start, time_t end, GError **error);
gboolean mbb_db_consumer_mod_name(gint id, gchar *name, GError **error);
gboolean mbb_db_consumer_mod_time(gint id, time_t start, time_t end, GError **error);
gboolean mbb_db_consumer_drop(gint id, GError **error);
gboolean mbb_db_consumer_set_user(gint id, gint user_id, GError **error);
gboolean mbb_db_consumer_unset_user(gint id, GError **error);

#endif
