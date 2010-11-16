#ifndef MBB_DB_INET_POOL_H
#define MBB_DB_INET_POOL_H

#include <glib.h>
#include <time.h>

#include "mbbinetpool.h"

gint mbb_db_inet_pool_add(MbbInetPoolEntry *entry, gint owner_id, GError **error);
gboolean mbb_db_inet_pool_drop(gint id, GError **error);
gboolean mbb_db_inet_pool_mod_time(gint id, time_t start, time_t end, GError **error);
gboolean mbb_db_inet_pool_mod_nice(gint id, guint nice, GError **error);
gboolean mbb_db_inet_pool_clear(gint owner_id, GError **error);

#endif
