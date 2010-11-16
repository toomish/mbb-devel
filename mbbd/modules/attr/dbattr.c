#include "dbattr.h"

#include "mbbdb.h"
#include "query.h"

gint db_attr_add(gint group_id, gchar *name, GError **error)
{
	return mbb_db_insert_ret(error,
		"attributes", "attr_id", "ds",
		"group_id", group_id, "attr_name", name
	);
}

gboolean db_attr_del(gint attr_id, GError **error)
{
	return mbb_db_delete(error, "attributes",
		"attr_id = %d", attr_id
	);
}

gboolean db_attr_rename(gint attr_id, gchar *name, GError **error)
{
	return mbb_db_update(error, "attributes", "s",
		"attr_name", name, "attr_id = %d", attr_id
	);
}

gboolean db_attr_set(gint attr_id, gchar *val, gint obj_id, GError **error)
{
	gchar *query;

	query = query_format(
		"select attr_set(%d, %s, %d);",
		attr_id, val, obj_id
	);

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean db_attr_unset(gint attr_id, gint obj_id, GError **error)
{
	return mbb_db_delete(error, "attribute_pool",
		"attr_id = %d and obj_id = %d", attr_id, obj_id
	);
}

