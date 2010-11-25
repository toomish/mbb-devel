/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbdbunit.h"
#include "mbbdb.h"

#include "query.h"

#define BUFLEN 32

gint mbb_db_unit_add(gchar *name, gint con_id, time_t start, time_t end,
		     GError **error)
{
	gint id;

	if (con_id < 0)
		id = mbb_db_insert_ret(error,
			"units", "unit_id", "stt",
			"unit_name", name,
			"time_start", start,
			"time_end", end
		);
	else
		id = mbb_db_insert_ret(error,
			"units", "unit_id", "sdtt",
			"unit_name", name, "consumer_id", con_id,
			"time_start", start,
			"time_end", end
		);

	return id;
}

static inline gchar *unit_where(gchar *buf, gint id)
{
	if (buf == NULL)
		buf = g_strdup_printf("unit_id = %d", id);
	else
		g_snprintf(buf, BUFLEN, "unit_id = %d", id);

	return buf;
}

gboolean mbb_db_unit_mod_name(gint id, gchar *name, GError **error)
{
	gchar *query;
	gchar buf[BUFLEN];

	query = query_update("units", unit_where(buf, id), "unit_name", name);

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_unit_mod_time(gint id, time_t start, time_t end, GError **error)
{
	gchar *query;
	gchar buf[BUFLEN];

	query = query_updatef(
		"units", "tt", unit_where(buf, id),
		"time_start", start, "time_end", end
	);

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_unit_drop(gint id, GError **error)
{
	gchar *query;
	gchar buf[BUFLEN];

	query = query_delete("units", unit_where(buf, id));

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_unit_set_consumer(gint id, gint con_id, GError **error)
{
	gchar *query;
	gchar buf[BUFLEN];

	query = query_updatef(
		"units", "d", unit_where(buf, id), "consumer_id", con_id
	);

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_unit_unset_consumer(gint id, GError **error)
{
	gchar *query;
	gchar buf[BUFLEN];

	query = query_update("units", unit_where(buf, id), "consumer_id", NULL);

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_unit_mod_local(gint id, gboolean local, GError **error)
{
	return mbb_db_update(error,
		"units", "b", "local", local, "unit_id = %d", id
	);
}

