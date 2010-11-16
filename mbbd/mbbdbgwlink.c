#include "mbbdbgwlink.h"
#include "mbboperator.h"
#include "mbbgateway.h"
#include "mbbgwlink.h"
#include "mbbdb.h"

#include "query.h"

#define BUFLEN 32

gint mbb_db_gwlink_add(struct mbb_gwlink *gl, GError **error)
{
	gint id;

	id = mbb_db_insert_ret(error,
		"gwlinks", "gwlink_id", "dddtt",
		"oper_id", gl->op->id,
		"gw_id", gl->gw->id,
		"link", gl->link,
		"time_start", gl->start,
		"time_end", gl->end
	);

	return id;
}

static gchar *gwlink_where(gchar *buf, gint id)
{
	if (buf == NULL)
		buf = g_strdup_printf("gwlink_id = %d", id);
	else
		g_snprintf(buf, BUFLEN, "gwlink_id = %d", id);

	return buf;
}

gboolean mbb_db_gwlink_drop(gint id, GError **error)
{
	gchar buf[BUFLEN];
	gchar *query;

	query = query_delete("gwlinks", gwlink_where(buf, id));

	return mbb_db_query(query, NULL, NULL, error);
}


gboolean mbb_db_gwlink_mod_time(gint id, time_t start, time_t end, GError **error)
{
	gchar buf[BUFLEN];
	gchar *query;

	query = query_updatef(
		"gwlinks", "tt", gwlink_where(buf, id),
		"time_start", start, "time_end", end
	);

	return mbb_db_query(query, NULL, NULL, error);
}

