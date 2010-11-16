#include "mbbgateway.h"
#include "mbbgwlink.h"
#include "mbbtime.h"

#include "datalink.h"
#include "macros.h"

struct func_data {
	GFunc func;
	gpointer user_data;
};

enum {
	MBB_GATEWAY_SELF,
	MBB_GATEWAY_ID,
	MBB_GATEWAY_NAME,
	MBB_GATEWAY_ADDR
};

static struct data_link_entry dl_entries[] = {
	DATA_LINK_SELF_ENTRY,
	{ G_STRUCT_OFFSET(struct mbb_gateway, id), g_int_hash, g_int_equal },
	{ G_STRUCT_OFFSET(struct mbb_gateway, name), g_pstr_hash, g_pstr_equal },
	{ G_STRUCT_OFFSET(struct mbb_gateway, addr), ipv4_hash, ipv4_equal }
};

static DataLink *dlink = NULL;

static inline void dlink_init(void)
{
	dlink = data_link_new_full(
		dl_entries, NELEM(dl_entries), (GDestroyNotify) mbb_gateway_free
	);
}

struct mbb_gateway *mbb_gateway_new(gint id, gchar *name, ipv4_t addr)
{
	struct mbb_gateway *gw;

	gw = g_new(struct mbb_gateway, 1);
	gw->id = id;
	gw->name = g_strdup(name);
	gw->addr = addr;

	gw->ht = NULL;

	return gw;
}

void mbb_gateway_free(struct mbb_gateway *gw)
{
	if (gw->ht != NULL)
		g_hash_table_destroy(gw->ht);

	g_free(gw->name);
	g_free(gw);
}

void mbb_gateway_join(struct mbb_gateway *gw)
{
	if (dlink == NULL)
		dlink_init();

	data_link_insert(dlink, gw);
}

void mbb_gateway_detach(struct mbb_gateway *gw)
{
	if (dlink != NULL)
		data_link_steal(dlink, MBB_GATEWAY_SELF, gw);
}

void mbb_gateway_remove(struct mbb_gateway *gw)
{
	mbb_gateway_detach(gw);
	mbb_gateway_free(gw);
}

gboolean mbb_gateway_mod_name(struct mbb_gateway *gw, gchar *newname)
{
	if (dlink == NULL || mbb_gateway_get_by_name(newname))
		return FALSE;

	data_link_steal(dlink, MBB_GATEWAY_SELF, gw);
	g_free(gw->name);
	gw->name = g_strdup(newname);
	data_link_insert(dlink, gw);

	return TRUE;
}

gchar *mbb_gateway_get_ename(struct mbb_gateway *gw, gboolean addr)
{
	if (addr)
		return ipv4toa(NULL, gw->addr);

	return g_strdup(gw->name);
}

struct mbb_gateway *mbb_gateway_get_by_id(gint id)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_GATEWAY_ID, &id);
}

struct mbb_gateway *mbb_gateway_get_by_name(gchar *name)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_GATEWAY_NAME, &name);
}

struct mbb_gateway *mbb_gateway_get_by_addr(ipv4_t addr)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_GATEWAY_ADDR, &addr);
}

struct mbb_gateway *mbb_gateway_get_by_ename(gchar *ename)
{
	struct mbb_gateway *gw = NULL;
	struct inet inet;

	if (dlink == NULL)
		return NULL;

	if (atoipv4(ename, &inet) != NULL)
		gw = data_link_lookup(dlink, MBB_GATEWAY_ADDR, &inet.addr);

	if (gw == NULL)
		gw = data_link_lookup(dlink, MBB_GATEWAY_NAME, &ename);

	return gw;
}

gboolean mbb_gateway_can_add(struct mbb_gateway *gw, struct mbb_gwlink *gl,
			     gboolean create)
{
	struct mbb_gwlink *cur;
	GQueue *queue;
	GList *list;

	if (gw->ht == NULL)
		return TRUE;

	queue = g_hash_table_lookup(gw->ht, GINT_TO_POINTER(gl->link));
	if (queue == NULL)
		return TRUE;

	for (list = queue->head; list != NULL; list = list->next) {
		cur = (struct mbb_gwlink *) list->data;
		if (! create && cur == gl)
			continue;

		if (mbb_time_ebcmp(gl->end, cur->start) < 0)
			return TRUE;

		if (mbb_time_becmp(gl->start, cur->end) > 0)
			continue;

		return FALSE;
	}

	return TRUE;
}

static gboolean gw_queue_insert_gl(GQueue *queue, struct mbb_gwlink *gl,
				   struct mbb_gwlink **old)
{
	struct mbb_gwlink *cur;
	GList *list;

	for (list = queue->head; list != NULL; list = list->next) {
		cur = (struct mbb_gwlink *) list->data;

		if (mbb_time_becmp(gl->start, cur->end) > 0)
			continue;

		if (mbb_time_ebcmp(gl->end, cur->start) < 0)
			break;

		if (old != NULL)
			*old = cur;

		return FALSE;
	}

	if (list == NULL)
		g_queue_push_tail(queue, gl);
	else
		g_queue_insert_before(queue, list, gl);

	return TRUE;
}

gboolean mbb_gateway_reorder_link(struct mbb_gateway *gw, struct mbb_gwlink *gl)
{
	GQueue *queue;
	GList *next;
	GList *list;

	if (gw->ht == NULL)
		return FALSE;

	queue = g_hash_table_lookup(gw->ht, GINT_TO_POINTER(gl->link));
	if (queue == NULL)
		return FALSE;

	list = g_queue_find(queue, gl);
	if (list == NULL)
		return FALSE;

	if (queue->length == 1)
		return TRUE;

	next = list->next;
	g_queue_delete_link(queue, list);

	if (gw_queue_insert_gl(queue, gl, NULL) == FALSE) {
		if (next == NULL)
			g_queue_push_tail(queue, gl);
		else
			g_queue_insert_before(queue, next, gl);

		return FALSE;
	}

	return TRUE;
}

gboolean mbb_gateway_add_link(struct mbb_gateway *gw, struct mbb_gwlink *gl,
			      struct mbb_gwlink **old)
{
	GQueue *queue = NULL;
	gboolean ret;

	if (gw->ht == NULL) {
		gw->ht = g_hash_table_new_full(
			g_direct_hash, g_direct_equal,
			NULL, (GDestroyNotify) g_queue_free
		);
	} else {
		queue = g_hash_table_lookup(
			gw->ht, GINT_TO_POINTER(gl->link)
		);
	}

	if (queue != NULL)
		ret = gw_queue_insert_gl(queue, gl, old);
	else {
		queue = g_queue_new();
		g_queue_push_head(queue, gl);
		g_hash_table_insert(gw->ht, GINT_TO_POINTER(gl->link), queue);

		ret = TRUE;
	}

	return ret;
}

void mbb_gateway_del_link(struct mbb_gateway *gw, struct mbb_gwlink *gl)
{
	GQueue *queue;
	GList *list;

	if (gw->ht == NULL)
		return;

	queue = g_hash_table_lookup(gw->ht, GINT_TO_POINTER(gl->link));
	if (queue == NULL)
		return;

	for (list = queue->head; list != NULL; list = list->next)
		if (list->data == gl)
			break;

	if (list != NULL) {
		g_queue_delete_link(queue, list);

		if (queue->length == 0)
			g_hash_table_remove(gw->ht, GINT_TO_POINTER(gl->link));

		if (g_hash_table_size(gw->ht) == 0) {
			g_hash_table_destroy(gw->ht);
			gw->ht = NULL;
		}
	}
}

guint mbb_gateway_nlink(struct mbb_gateway *gw)
{
	if (gw->ht == NULL)
		return 0;

	return g_hash_table_size(gw->ht);
}

struct mbb_gwlink *mbb_gateway_get_link(struct mbb_gateway *gw, guint link, time_t t)
{
	GQueue *queue;
	GList *list;

	if (gw->ht == NULL)
		return NULL;

	queue = g_hash_table_lookup(gw->ht, GINT_TO_POINTER(link));
	if (queue == NULL)
		return NULL;

	for (list = queue->head; list != NULL; list = list->next) {
		struct mbb_gwlink *gl;
		gint tmp;

		gl = list->data;
		tmp = mbb_time_rcmp(t, gl->start, gl->end);

		if (tmp == 0) return gl;
		if (tmp < 0) return NULL;
	}

	return NULL;
}

struct mbb_gwlink *mbb_gateway_find_link(ipv4_t ip, guint link, time_t t)
{
	struct mbb_gateway *gw;

	gw = mbb_gateway_get_by_addr(ip);
	if (gw == NULL)
		return NULL;

	return mbb_gateway_get_link(gw, link, t);
}

void mbb_gateway_foreach(GFunc func, gpointer user_data)
{
	data_link_foreach(dlink, func, user_data);
}

static void link_foreach(gpointer key, GQueue *queue, struct func_data *fd)
{
	GList *list;

	(void) key;

	for (list = queue->head; list != NULL; list = list->next)
		fd->func(list->data, fd->user_data);
}

void mbb_gateway_link_foreach(struct mbb_gateway *gw, GFunc func, gpointer user_data)
{
	struct func_data fd = { func, user_data };

	if (gw->ht != NULL)
		g_hash_table_foreach(gw->ht, (GHFunc) link_foreach, &fd);
}

