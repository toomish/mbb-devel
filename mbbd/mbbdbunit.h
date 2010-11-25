/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_DB_ACC_UNIT_H
#define MBB_DB_ACC_UNIT_H

#include <glib.h>
#include <time.h>

gint mbb_db_unit_add(gchar *name, gint con_id, time_t start, time_t end,
			 GError **error);
gboolean mbb_db_unit_mod_name(gint id, gchar *name, GError **error);
gboolean mbb_db_unit_mod_time(gint id, time_t start, time_t end, GError **error);
gboolean mbb_db_unit_drop(gint id, GError **error);
gboolean mbb_db_unit_set_consumer(gint id, gint con_id, GError **error);
gboolean mbb_db_unit_unset_consumer(gint id, GError **error);
gboolean mbb_db_unit_mod_local(gint id, gboolean local, GError **error);

#endif
