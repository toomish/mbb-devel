/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbdbconsumer.h"
#include "mbbdb.h"

#include "query.h"

#define BUFLEN 32

gint mbb_db_consumer_add(gchar *name, time_t start, time_t end, GError **error)
{
	gint id;

	id = mbb_db_insert_ret(error,
		"consumers", "consumer_id", "stt",
		"consumer_name", name,
		"time_start", start, "time_end", end
	);

	return id;
}

static inline gchar *consumer_where(gchar *buf, gint id)
{
	if (buf == NULL)
		buf = g_strdup_printf("consumer_id = %d", id);
	else
		g_snprintf(buf, BUFLEN, "consumer_id = %d", id);

	return buf;
}

gboolean mbb_db_consumer_mod_name(gint id, gchar *name, GError **error)
{
	gchar *query;
	gchar buf[BUFLEN];

	query = query_update(
		"consumers", consumer_where(buf, id), "consumer_name", name
	);

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_consumer_mod_time(gint id, time_t start, time_t end, GError **error)
{
	gchar *query;
	gchar buf[BUFLEN];

	query = query_updatef(
		"consumers", "tt", consumer_where(buf, id),
		"time_start", start, "time_end", end
	);

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_consumer_drop(gint id, GError **error)
{
	gchar *query;
	gchar buf[BUFLEN];

	query = query_delete("consumers", consumer_where(buf, id));

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_consumer_set_user(gint id, gint user_id, GError **error)
{
	return mbb_db_update(error,
		"consumers", "d", "user_id", user_id, "consumer_id = %d", id
	);
}

gboolean mbb_db_consumer_unset_user(gint id, GError **error)
{
	return mbb_db_update(error,
		"consumers", "s", "user_id", NULL, "consumer_id = %d", id
	);
}

