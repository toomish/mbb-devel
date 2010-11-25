/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbdbgateway.h"
#include "mbbdb.h"

#include "query.h"

#define BUFLEN 32

gint mbb_db_gateway_add(gchar *name, ipv4_t ip, GError **error)
{
	ipv4_buf_t buf;
	gint id;

	ipv4toa(buf, ip);
	id = mbb_db_insert_ret(error,
		"gateways", "gw_id", "ss",
		"gw_name", name, "gw_ip", buf
	);

	return id;
}

static inline gchar *gateway_where(gchar *buf, gint id)
{
	if (buf == NULL)
		buf = g_strdup_printf("gw_id = %d", id);
	else
		g_snprintf(buf, BUFLEN, "gw_id = %d", id);

	return buf;
}

gboolean mbb_db_gateway_mod_name(gint id, gchar *name, GError **error)
{
	gchar buf[BUFLEN];
	gchar *query;

	query = query_update("gateways", gateway_where(buf, id), "gw_name", name);

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_gateway_drop(gint id, GError **error)
{
	gchar buf[BUFLEN];
	gchar *query;

	query = query_delete("gateways", gateway_where(buf, id));

	return mbb_db_query(query, NULL, NULL, error);
}

