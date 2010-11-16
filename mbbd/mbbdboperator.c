#include "mbbdboperator.h"
#include "mbbdb.h"

#include "query.h"

#define BUFLEN 32

gint mbb_db_operator_add(gchar *name, GError **error)
{
	gint id;

	id = mbb_db_insert_ret(error,
		"operators", "oper_id", "s",
		"oper_name", name
	);

	return id;
}

static gchar *operator_where(gchar *buf, gint id)
{
	if (buf == NULL)
		buf = g_strdup_printf("oper_id = %d", id);
	else
		g_snprintf(buf, BUFLEN, "oper_id = %d", id);

	return buf;
}

gboolean mbb_db_operator_mod_name(gint id, gchar *name, GError **error)
{
	gchar buf[BUFLEN];
	gchar *query;

	query = query_update("operators",
		operator_where(buf, id), "oper_name", name
	);

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_operator_drop(gint id, GError **error)
{
	gchar buf[BUFLEN];
	gchar *query;

	query = query_delete("operators", operator_where(buf, id));

	return mbb_db_query(query, NULL, NULL, error);
}

