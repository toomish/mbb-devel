#ifndef MBB_DB_USER_H
#define MBB_DB_USER_H

#include <glib.h>

#include "macros.h"

gint mbb_db_user_add(gchar *name, gchar *secret, GError **error) __nonnull((1));
gboolean mbb_db_user_mod_name(gint id, gchar *name, GError **error) __nonnull((2));
gboolean mbb_db_user_mod_pass(gint id, gchar *secret, GError **error) __nonnull((2));
gboolean mbb_db_user_drop(gint id, GError **error);

#endif
