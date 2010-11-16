#ifndef DB_ATTR_H
#define DB_ATTR_H

#include <glib.h>

gint db_attr_add(gint group_id, gchar *name, GError **error);
gboolean db_attr_del(gint attr_id, GError **error);
gboolean db_attr_rename(gint attr_id, gchar *name, GError **error);
gboolean db_attr_set(gint attr_id, gchar *val, gint obj_id, GError **error);
gboolean db_attr_unset(gint attr_id, gint obj_id, GError **error);

#endif
