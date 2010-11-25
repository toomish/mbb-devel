/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_DB_LOAD_H
#define MBB_DB_LOAD_H

#include <glib.h>

#define MBB_DB_LOAD_ERROR (mbb_db_load_error_quark())

typedef enum {
	MBB_DB_LOAD_ERROR_INVALID,
	MBB_DB_LOAD_ERROR_INSIDE
} MbbDbLoadError;

GQuark mbb_db_load_error_quark(void);

gboolean mbb_db_load_users(GError **error);
gboolean mbb_db_load_groups(GError **error);
gboolean mbb_db_load_groups_pool(GError **error);
gboolean mbb_db_load_consumers(GError **error);
gboolean mbb_db_load_units(GError **error);
gboolean mbb_db_load_unit_inet_pool(GError **error);
gboolean mbb_db_load_gateways(GError **error);
gboolean mbb_db_load_operators(GError **error);
gboolean mbb_db_load_gwlinks(GError **error);

#endif
