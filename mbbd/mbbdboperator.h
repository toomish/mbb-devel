/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_DB_OPERATOR_H
#define MBB_DB_OPERATOR_H

#include <glib.h>

gint mbb_db_operator_add(gchar *name, GError **error);
gboolean mbb_db_operator_mod_name(gint id, gchar *name, GError **error);
gboolean mbb_db_operator_drop(gint id, GError **error);

#endif
