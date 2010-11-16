#include "mbbinetpool.h"
#include "mbbtime.h"

#include "debug.h"

struct mbb_inet_pool {
	GHashTable *ht;
};

struct mbb_inet_pool *mbb_inet_pool_new(void)
{
	struct mbb_inet_pool *pool;

	pool = g_new(struct mbb_inet_pool, 1);
	pool->ht = g_hash_table_new_full(
		g_direct_hash, g_direct_equal,
		NULL, (GDestroyNotify) mbb_inet_pool_entry_free
	);

	return pool;
}

void mbb_inet_pool_entry_init(struct mbb_inet_pool_entry *entry)
{
	entry->node_list = NULL;
}

struct mbb_inet_pool_entry *mbb_inet_pool_entry_new(struct mbb_object *owner)
{
	struct mbb_inet_pool_entry *entry;

	entry = g_new(struct mbb_inet_pool_entry, 1);
	entry->owner = owner;
	entry->node_list = NULL;

	return entry;
}

void mbb_inet_pool_entry_free(struct mbb_inet_pool_entry *entry)
{
	g_free(entry);
}

void mbb_inet_pool_entry_delete(struct mbb_inet_pool_entry *entry)
{
	mbb_object_emit_del(entry->owner, entry);
}

gboolean mbb_inet_pool_entry_test_time(struct mbb_inet_pool_entry *entry,
				       time_t start, time_t end)
{
	if (mbb_time_pbecmp(entry->start, entry->end, start, end) <= 0)
		return TRUE;

	return FALSE;
}

time_t mbb_inet_pool_entry_get_start(MbbInetPoolEntry *entry)
{
	if (entry->start >= 0)
		return entry->start;

	return mbb_object_get_start(entry->owner);
}

time_t mbb_inet_pool_entry_get_end(MbbInetPoolEntry *entry)
{
	if (entry->end >= 0)
		return entry->end;

	return mbb_object_get_end(entry->owner);
}

void mbb_inet_pool_add(struct mbb_inet_pool *pool, struct mbb_inet_pool_entry *entry)
{
	g_hash_table_insert(pool->ht, GINT_TO_POINTER(entry->id), entry);
}

void mbb_inet_pool_del(struct mbb_inet_pool *pool, gint id)
{
	g_hash_table_remove(pool->ht, GINT_TO_POINTER(id));
}

struct mbb_inet_pool_entry *mbb_inet_pool_get(struct mbb_inet_pool *pool, gint id)
{
	if (pool == NULL)
		return NULL;

	return g_hash_table_lookup(pool->ht, GINT_TO_POINTER(id));
}

void mbb_inet_pool_foreach(struct mbb_inet_pool *pool, GFunc func, gpointer user_data)
{
	GList *data_list;
	GList *list;

	if (pool == NULL)
		return;

	data_list = g_hash_table_get_values(pool->ht);
	for (list = data_list; list != NULL; list = list->next)
		func(list->data, user_data);

	g_list_free(data_list);
}

void mbb_inet_pool_map_add_handler(MapNode *node)
{
	MbbInetPoolEntry *entry;

	entry = map_node_get_data(node);
	node = map_node_dup(node);
	entry->node_list = g_list_prepend(entry->node_list, node);

	MSG_WARN("entry %d: add %p", entry->id, (void *) node);
}

void mbb_inet_pool_map_del_handler(MapNode *node)
{
	MbbInetPoolEntry *entry;
	GList *list;

	entry = map_node_get_data(node);
	list = g_list_find_custom(
		entry->node_list, node, (GCompareFunc) map_node_cmp
	);

	if (list != NULL) {
		MSG_WARN("entry %d: del %p", entry->id, (void *) list->data);

		g_free(list->data);
		entry->node_list = g_list_delete_link(entry->node_list, list);
	}
}

