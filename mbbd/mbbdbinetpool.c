/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbdbinetpool.h"
#include "mbbdb.h"
#include "inet.h"

#include "query.h"

#define BUFLEN 32

gint mbb_db_inet_pool_add(MbbInetPoolEntry *entry, gint owner_id, GError **error)
{
	inet_buf_t buf;
	gint id;

	inettoa(buf, &entry->inet);
	id = mbb_db_insert_ret(error,
		"unit_ip_pool", "self_id", "dsbttd",
		"unit_id", owner_id,
		"inet_addr", buf,
		"inet_flag", entry->flag,
		"time_start", entry->start,
		"time_end", entry->end,
		"nice", entry->nice
	);

	return id;
}

static inline gchar *entry_where(gchar *buf, gint id)
{
	if (buf == NULL)
		buf = g_strdup_printf("self_id = %d", id);
	else
		g_snprintf(buf, BUFLEN, "self_id = %d", id);

	return buf;
}

gboolean mbb_db_inet_pool_drop(gint id, GError **error)
{
	gchar *query;
	gchar buf[BUFLEN];

	query = query_delete("unit_ip_pool", entry_where(buf, id));

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_inet_pool_clear(gint owner_id, GError **error)
{
	return mbb_db_delete(error, "unit_ip_pool", "unit_id = %d", owner_id);
}

gboolean mbb_db_inet_pool_mod_time(gint id, time_t start, time_t end, GError **error)
{
	gchar *query;
	gchar buf[BUFLEN];

	query = query_updatef(
		"unit_ip_pool", "tt", entry_where(buf, id),
		"time_start", start, "time_end", end
	);

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_inet_pool_mod_nice(gint id, guint nice, GError **error)
{
	gchar *query;
	gchar buf[BUFLEN];

	query = query_updatef(
		"unit_ip_pool", "d", entry_where(buf, id), "nice", nice
	);

	return mbb_db_query(query, NULL, NULL, error);
}

