#include "mbbconsumer.h"
#include "mbbtime.h"
#include "mbbunit.h"

#include "datalink.h"
#include "macros.h"

enum {
	MBB_CONSUMER_SELF,
	MBB_CONSUMER_ID,
	MBB_CONSUMER_NAME
};

static struct data_link_entry dl_entries[] = {
	DATA_LINK_SELF_ENTRY,
	{ G_STRUCT_OFFSET(struct mbb_consumer, id), g_int_hash, g_int_equal },
	{ G_STRUCT_OFFSET(struct mbb_consumer, name), g_pstr_hash, g_pstr_equal }
};

static DataLink *dlink = NULL;

static inline void dlink_init(void)
{
	dlink = data_link_new_full(
		dl_entries, NELEM(dl_entries), (GDestroyNotify) mbb_consumer_free
	);
}

struct mbb_consumer *mbb_consumer_new(gint id, gchar *name, time_t start, time_t end)
{
	struct mbb_consumer *con;

	con = g_new(struct mbb_consumer, 1);
	con->id = id;
	con->name = g_strdup(name);

	con->units = NULL;

	con->start = start;
	con->end = end;

	con->user = NULL;

	return con;
}

void mbb_consumer_free(struct mbb_consumer *con)
{
	g_slist_free(con->units);
	g_free(con->name);
	g_free(con);
}

void mbb_consumer_join(struct mbb_consumer *con)
{
	if (dlink == NULL)
		dlink_init();

	data_link_insert(dlink, con);
}

void mbb_consumer_detach(struct mbb_consumer *con)
{
	if (dlink == NULL)
		return;

	data_link_steal(dlink, MBB_CONSUMER_SELF, con);
}

void mbb_consumer_remove(struct mbb_consumer *con)
{
	mbb_consumer_detach(con);
	mbb_consumer_free(con);
}

gboolean mbb_consumer_mod_name(struct mbb_consumer *con, gchar *newname)
{
	if (dlink == NULL || mbb_consumer_get_by_name(newname))
		return FALSE;

	data_link_steal(dlink, MBB_CONSUMER_SELF, con);
	g_free(con->name);
	con->name = g_strdup(newname);
	data_link_insert(dlink, con);

	return TRUE;
}

gboolean mbb_consumer_mod_time(struct mbb_consumer *con, time_t start, time_t end)
{
	GSList *list;

	if (mbb_time_becmp(start, end) > 0)
		return FALSE;

	for (list = con->units; list != NULL; list = list->next)
		if (mbb_unit_test_time(list->data, start, end) == FALSE)
			return FALSE;

	con->start = start;
	con->end = end;

	return TRUE;
}

struct mbb_consumer *mbb_consumer_get_by_id(gint id)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_CONSUMER_ID, &id);
}

struct mbb_consumer *mbb_consumer_get_by_name(gchar *name)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_CONSUMER_NAME, &name);
}

void mbb_consumer_foreach(GFunc func, gpointer user_data)
{
	data_link_foreach(dlink, func, user_data);
}

void mbb_consumer_forregex(Regex re, GFunc func, gpointer user_data)
{
	data_link_forregex(dlink, re, MBB_CONSUMER_NAME, func, user_data);
}

void mbb_consumer_add_unit(struct mbb_consumer *con, struct mbb_unit *unit)
{
	if (g_slist_find(con->units, unit) == NULL)
		con->units = g_slist_prepend(con->units, unit);
}

void mbb_consumer_del_unit(struct mbb_consumer *con, struct mbb_unit *unit)
{
	con->units = g_slist_remove(con->units, unit);
}

gboolean mbb_consumer_has_unit(struct mbb_consumer *con, struct mbb_unit *unit)
{
	if (g_slist_find(con->units, unit))
		return TRUE;
	return FALSE;
}

gboolean mbb_consumer_mapped(struct mbb_consumer *con)
{
	GSList *list;

	for (list = con->units; list != NULL; list = list->next)
		if (mbb_unit_mapped(list->data))
			return TRUE;

	return FALSE;
}

