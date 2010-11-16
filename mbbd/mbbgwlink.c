#include "mbboperator.h"
#include "mbbgateway.h"
#include "mbbgwlink.h"

#include "datalink.h"
#include "macros.h"

enum {
	MBB_GWLINK_SELF,
	MBB_GWLINK_ID,
};

static struct data_link_entry dl_entries[] = {
	DATA_LINK_SELF_ENTRY,
	{ G_STRUCT_OFFSET(struct mbb_gwlink, id), g_int_hash, g_int_equal }
};

static DataLink *dlink = NULL;

static void dlink_init(void)
{
	dlink = data_link_new_full(
		dl_entries, NELEM(dl_entries), (GDestroyNotify) mbb_gwlink_free
	);
}

struct mbb_gwlink *mbb_gwlink_new(gint id)
{
	struct mbb_gwlink *gl;

	gl = g_new(struct mbb_gwlink, 1);
	gl->id = id;

	gl->gw = NULL;
	gl->op = NULL;
	gl->link = 0;

	gl->start = gl->end = 0;

	return gl;
}

void mbb_gwlink_free(struct mbb_gwlink *gl)
{
	if (gl->gw != NULL)
		mbb_gateway_del_link(gl->gw, gl);

	if (gl->op != NULL)
		mbb_operator_del_link(gl->op, gl);

	g_free(gl);
}

void mbb_gwlink_join(struct mbb_gwlink *gl)
{
	if (dlink == NULL)
		dlink_init();

	data_link_insert(dlink, gl);
}

void mbb_gwlink_detach(struct mbb_gwlink *gl)
{
	if (dlink != NULL)
		data_link_steal(dlink, MBB_GWLINK_SELF, gl);
}

void mbb_gwlink_remove(struct mbb_gwlink *gl)
{
	mbb_gwlink_detach(gl);
	mbb_gwlink_free(gl);
}

struct mbb_gwlink *mbb_gwlink_get_by_id(gint id)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_GWLINK_ID, &id);
}

void mbb_gwlink_foreach(GFunc func, gpointer user_data)
{
	data_link_foreach(dlink, func, user_data);
}

