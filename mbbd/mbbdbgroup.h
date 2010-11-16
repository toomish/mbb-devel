#ifndef MBB_DB_GROUP_H
#define MBB_DB_GROUP_H

#include <glib.h>

#include "macros.h"

gboolean mbb_db_group_mod_name(gint id, gchar *name, GError **error) __nonnull((2));
gboolean mbb_db_group_add_user(gint group_id, gint user_id, GError **error);
gboolean mbb_db_group_del_user(gint group_id, gint user_id, GError **error);

#endif
