#include "datalink.h"

struct data_link_entry_wrap {
	guint off;
	GHashTable *ht;
};

struct data_link {
	guint nentries;
	GDestroyNotify destroy_func;
	struct data_link_entry_wrap entries[];
};

struct data_link *data_link_new(struct data_link_entry *entries, guint nentries)
{
	struct data_link_entry_wrap *dl_entry;
	struct data_link_entry *entry;
	struct data_link *dlink;
	gsize dlink_size;
	guint n;

	dlink_size = sizeof(struct data_link);
	dlink_size += nentries * sizeof(struct data_link_entry_wrap);
	dlink = g_malloc(dlink_size);

	dlink->nentries = nentries;
	dlink->destroy_func = NULL;

	entry = entries;
	dl_entry = dlink->entries;
	for (n = 0; n < nentries; n++, entry++, dl_entry++) {
		dl_entry->off = entry->off;
		dl_entry->ht = g_hash_table_new(
			entry->hash_func, entry->key_equal_func
		);
	}

	return dlink;
}

struct data_link *data_link_new_full(struct data_link_entry *entries,
				     guint nentries,
				     GDestroyNotify func)
{
	struct data_link *dlink;

	dlink = data_link_new(entries, nentries);
	dlink->destroy_func = func;

	return dlink;
}

void data_link_insert(struct data_link *dlink, gpointer data)
{
	struct data_link_entry_wrap *entry;
	gpointer key;
	guint n;

	entry = dlink->entries;
	for (n = 0; n < dlink->nentries; n++, entry++) {
		key = G_STRUCT_MEMBER_P(data, entry->off);
		g_hash_table_insert(entry->ht, key, data);
	}
}

gpointer data_link_lookup(struct data_link *dlink, guint entry_no, gpointer key)
{
	struct data_link_entry_wrap *entry;
	gpointer data;

	g_return_val_if_fail(entry_no < dlink->nentries, NULL);

	entry = &dlink->entries[entry_no];
	data = g_hash_table_lookup(entry->ht, key);

	return data;
}

static void data_link_unlink(struct data_link *dlink, gpointer data)
{
	struct data_link_entry_wrap *entry;
	gpointer key;
	guint n;

	entry = dlink->entries;
	for (n = 0; n < dlink->nentries; n++, entry++) {
		key = G_STRUCT_MEMBER_P(data, entry->off);
		g_hash_table_remove(entry->ht, key);
	}
}

void data_link_steal(struct data_link *dlink, guint entry_no, gpointer key)
{
	gpointer data;

	data = data_link_lookup(dlink, entry_no, key);
	if (data != NULL)
		data_link_unlink(dlink, data);
}

void data_link_remove(struct data_link *dlink, guint entry_no, gpointer key)
{
	gpointer data;

	data = data_link_lookup(dlink, entry_no, key);
	if (data != NULL) {
		data_link_unlink(dlink, data);
		if (dlink->destroy_func != NULL)
			dlink->destroy_func(data);
	}
}

void data_link_free(struct data_link *dlink)
{
	struct data_link_entry_wrap *entry;
	GList *list = NULL;
	guint n;

	entry = dlink->entries;
	if (dlink->destroy_func != NULL) 
		list = g_hash_table_get_values(entry->ht);

	for (n = 0; n < dlink->nentries; n++, entry++)
		g_hash_table_destroy(entry->ht);

	if (list != NULL) {
		g_list_foreach(list, (GFunc) dlink->destroy_func, NULL);
		g_list_free(list);
	}

	g_free(dlink);
}

gboolean g_pstr_equal(gconstpointer pa, gconstpointer pb)
{
	return g_str_equal(*(const gchar **) pa, *(const gchar **) pb);
}

guint g_pstr_hash(gconstpointer p)
{
	return g_str_hash(*(const gchar **) p);
}

gboolean g_ptr_equal(gconstpointer pa, gconstpointer pb)
{
	return *(gpointer *) pa == *(gpointer *) pb;
}

guint g_ptr_hash(gconstpointer p)
{
	return GPOINTER_TO_UINT(*(gpointer *) p);
}

void data_link_foreach(struct data_link *dlink, GFunc func, gpointer user_data)
{
	GHashTableIter iter;
	gpointer value;

	if (dlink == NULL)
		return;

	g_hash_table_iter_init(&iter, dlink->entries->ht);
	while (g_hash_table_iter_next(&iter, NULL, &value))
		func(value, user_data);

	/*
	data_list = g_hash_table_get_values(dlink->entries->ht);
	for (list = data_list; list != NULL; list = list->next)
		func(list->data, user_data);

	g_list_free(data_list);
	*/
}

/*
gboolean data_link_iter_next(struct data_link_iter *iter, gpointer *data)
{
	if (iter->init == FALSE) {
		if (iter->dlink == NULL)
			return FALSE;

		g_hash_table_iter_init(&iter->ht_iter, iter->dlink->entries->ht);
		iter->init = TRUE;
	}

	return g_hash_table_iter_next(&iter->ht_iter, NULL, data);
}
*/

void data_link_forregex(struct data_link *dlink, Regex re, guint entry_no,
			GFunc func, gpointer user_data)
{
	GList *data_list;
	gpointer key;
	guint off;
	GList *list;

	if (re == NULL) {
		data_link_foreach(dlink, func, user_data);
		return;
	}

	if (dlink == NULL)
		return;

	if (entry_no >= dlink->nentries)
		return;

	off = dlink->entries[entry_no].off;
	data_list = g_hash_table_get_values(dlink->entries->ht);
	for (list = data_list; list != NULL; list = list->next) {
		key = G_STRUCT_MEMBER_P(list->data, off);

		if (re_ismatch(re, *(gchar **) key))
			func(list->data, user_data);
	}

	g_list_free(data_list);
}

