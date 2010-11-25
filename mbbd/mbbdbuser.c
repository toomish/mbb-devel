/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbdbuser.h"
#include "mbbdb.h"

#include "query.h"

#define BUFLEN 32

gint mbb_db_user_add(gchar *name, gchar *secret, GError **error)
{
	gint id;

	if (secret == NULL)
		id = mbb_db_insert_ret(error,
			"users", "user_id", "s", "user_name", name
		);
	else
		id = mbb_db_insert_ret(error,
			"users", "user_id", "ss",
			"user_name", name, "user_secret", secret
		);


	return id;
}

static inline gchar *user_where(gchar *buf, gint id)
{
	if (buf == NULL)
		buf = g_strdup_printf("user_id = %d", id);
	else
		g_snprintf(buf, BUFLEN, "user_id = %d", id);

	return buf;
}

gboolean mbb_db_user_mod_name(gint id, gchar *name, GError **error)
{
	gchar buf[BUFLEN];
	gchar *query;

	query = query_update("users", user_where(buf, id), "user_name", name);

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_user_mod_pass(gint id, gchar *secret, GError **error)
{
	gchar buf[BUFLEN];
	gchar *query;

	query = query_update("users", user_where(buf, id), "user_secret", secret);

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_user_drop(gint id, GError **error)
{
	gchar buf[BUFLEN];
	gchar *query;

	query = query_delete("users", user_where(buf, id));

	return mbb_db_query(query, NULL, NULL, error);
}

