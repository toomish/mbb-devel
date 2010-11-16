#ifndef MBB_DB_GWLINK_H
#define MBB_DB_GWLINK_H

#include <glib.h>

struct mbb_gwlink;

gint mbb_db_gwlink_add(struct mbb_gwlink *gl, GError **error);
gboolean mbb_db_gwlink_drop(gint id, GError **error);
gboolean mbb_db_gwlink_mod_time(gint id, time_t start, time_t end, GError **error);

#endif
