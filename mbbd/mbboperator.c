#include "mbboperator.h"
#include "mbbgwlink.h"

#include "datalink.h"
#include "macros.h"

enum {
	MBB_OPERATOR_SELF,
	MBB_OPERATOR_ID,
	MBB_OPERATOR_NAME
};

static struct data_link_entry dl_entries[] = {
	DATA_LINK_SELF_ENTRY,
	{ G_STRUCT_OFFSET(struct mbb_operator, id), g_int_hash, g_int_equal },
	{ G_STRUCT_OFFSET(struct mbb_operator, name), g_pstr_hash, g_pstr_equal }
};

static DataLink *dlink = NULL;

static void dlink_init(void)
{
	dlink = data_link_new_full(
		dl_entries, NELEM(dl_entries), (GDestroyNotify) mbb_operator_free
	);
}

struct mbb_operator *mbb_operator_new(gint id, gchar *name)
{
	struct mbb_operator *op;

	op = g_new(struct mbb_operator, 1);
	op->id = id;
	op->name = g_strdup(name);
	op->links = NULL;

	return op;
}

void mbb_operator_free(struct mbb_operator *op)
{
	g_slist_free(op->links);
	g_free(op->name);
	g_free(op);
}

void mbb_operator_join(struct mbb_operator *op)
{
	if (dlink == NULL)
		dlink_init();

	data_link_insert(dlink, op);
}

void mbb_operator_detach(struct mbb_operator *op)
{
	if (dlink != NULL)
		data_link_steal(dlink, MBB_OPERATOR_SELF, op);
}

void mbb_operator_remove(struct mbb_operator *op)
{
	mbb_operator_detach(op);
	mbb_operator_free(op);
}

gboolean mbb_operator_mod_name(struct mbb_operator *op, gchar *newname)
{
	if (dlink == NULL || mbb_operator_get_by_name(newname))
		return FALSE;

	data_link_steal(dlink, MBB_OPERATOR_SELF, op);
	g_free(op->name);
	op->name = g_strdup(newname);
	data_link_insert(dlink, op);

	return TRUE;
}

struct mbb_operator *mbb_operator_get_by_id(gint id)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_OPERATOR_ID, &id);
}

struct mbb_operator *mbb_operator_get_by_name(gchar *name)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_OPERATOR_NAME, &name);
}

void mbb_operator_foreach(GFunc func, gpointer user_data)
{
	data_link_foreach(dlink, func, user_data);
}

void mbb_operator_add_link(struct mbb_operator *op, struct mbb_gwlink *gl)
{
	op->links = g_slist_prepend(op->links, gl);
}

void mbb_operator_del_link(struct mbb_operator *op, struct mbb_gwlink *gl)
{
	op->links = g_slist_remove(op->links, gl);
}

