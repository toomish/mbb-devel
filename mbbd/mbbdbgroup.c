/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbdbgroup.h"
#include "mbbdb.h"

#include "query.h"

gboolean mbb_db_group_mod_name(gint id, gchar *name, GError **error)
{
	return mbb_db_update(error,
		"groups", NULL, "group_name", name, NULL, "group_id = %d", id
	);
}

gboolean mbb_db_group_add_user(gint group_id, gint user_id, GError **error)
{
	return mbb_db_insert(error,
		"groups_pool", "dd", "group_id", group_id, "user_id", user_id
	);
}

gboolean mbb_db_group_del_user(gint group_id, gint user_id, GError **error)
{
	return mbb_db_delete(error, "groups_pool",
		"group_id = %d and user_id = %d", group_id, user_id
	);
}

