/* Copyright (C) 2012 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbmodule.h"
#include "mbbthread.h"
#include "mbblog.h"
#include "mbbvar.h"

#include "macros.h"
#include "debug.h"

#include "interface.h"

struct private_entry {
	gpointer data;
	GDestroyNotify destroy;
};

static struct mbb_var *private_var = NULL;
static GSList *ht_list = NULL;
static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

static inline struct private_entry *private_entry_new(GDestroyNotify destroy)
{
	struct private_entry *entry;

	entry = g_new(struct private_entry, 1);
	entry->data = NULL;
	entry->destroy = destroy;

	return entry;
}

static void private_entry_free(struct private_entry *entry)
{
	if (entry->data != NULL && entry->destroy != NULL)
		entry->destroy(entry->data);
	g_free(entry);
}

static gpointer private_table_new(gpointer ptr G_GNUC_UNUSED)
{
	GHashTable *ht;

	ht = g_hash_table_new_full(
		g_direct_hash, g_direct_equal,
		NULL, (GDestroyNotify) private_entry_free
	);

	return ht;
}

static void private_table_free(gpointer ptr)
{
	GHashTable *ht = ptr;

	g_static_mutex_lock(&mutex);
	ht_list = g_slist_remove(ht_list, ht);
	g_static_mutex_unlock(&mutex);

	g_hash_table_destroy(ht);
}

static inline struct private_entry *private_get_entry(gpointer key, GHashTable **ptr)
{
	GHashTable *ht;

	ht = mbb_session_var_get_data(private_var);
	if (ptr != NULL)
		*ptr = ht;

	return g_hash_table_lookup(ht, key);
}

static void private_init(gpointer key, GDestroyNotify notify)
{
	GHashTable *ht;

	ht = mbb_session_var_get_data(private_var);
	g_hash_table_insert(ht, key, private_entry_new(notify));

	g_static_mutex_lock(&mutex);
	if (g_slist_find(ht_list, ht) == NULL)
		ht_list = g_slist_prepend(ht_list, ht);
	g_static_mutex_unlock(&mutex);
}

static gboolean private_set(gpointer key, gpointer data)
{
	struct private_entry *entry;

	entry = private_get_entry(key, NULL);
	if (entry == NULL)
		return FALSE;

	if (entry->data != NULL && entry->destroy != NULL)
		entry->destroy(entry->data);

	entry->data = data;
	return TRUE;
}

static gpointer private_get(gpointer key)
{
	struct private_entry *entry;

	entry = private_get_entry(key, NULL);
	if (entry != NULL)
		return entry->data;

	return NULL;
}

static void private_free(gpointer key)
{
	struct private_entry *entry;
	GHashTable *ht;

	entry = private_get_entry(key, &ht);
	if (entry != NULL) {
		g_hash_table_remove(ht, key);

		if (g_hash_table_size(ht) == 0) {
			g_static_mutex_lock(&mutex);
			ht_list = g_slist_remove(ht_list, ht);
			g_static_mutex_unlock(&mutex);
		}
	}
}

static void private_remove(gpointer key)
{
	GSList *new_list = NULL;
	GSList *list;

	g_static_mutex_lock(&mutex);
	for (list = ht_list; list != NULL; list = list->next) {
		GHashTable *ht = list->data;

		g_hash_table_remove(ht, key);
		if (g_hash_table_size(ht))
			new_list = g_slist_prepend(new_list, ht);
	}
	list = ht_list;
	ht_list = new_list;
	g_static_mutex_unlock(&mutex);

	g_slist_free(list);
}

MBB_VAR_DEF(null_def) {
	.op_read = NULL,
	.op_write = NULL
};

static void load_module(void)
{
	static struct private_interface interface = {
		.init = private_init,
		.set = private_set,
		.get = private_get,
		.free = private_free,
		.remove = private_remove
	};

	struct mbb_session_var var = {
		.op_new = private_table_new,
		.op_free = private_table_free
	};

	mbb_module_export(&interface);
	private_var = mbb_module_add_session_var(SS_("private.table"), &null_def, &var);
}

static void unload_module(void)
{
}

MBB_DEFINE_MODULE("private variables library")
