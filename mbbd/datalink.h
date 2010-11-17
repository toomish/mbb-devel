#ifndef DATA_LINK_H
#define DATA_LINK_H

#include <glib.h>

#include "re.h"

typedef struct data_link DataLink;

struct data_link_entry {
	guint off;
	GHashFunc hash_func;
	GEqualFunc key_equal_func;
};

/*
struct data_link_iter {
	gboolean init;
	struct data_link *dlink;
	GHashTableIter ht_iter;
};

#define DATA_LINK_ITER_INIT(dl) { .init = FALSE, .dlink = dl }

#define data_link_for_each(ptr, dlink) \
	for (struct data_link_iter iter = DATA_LINK_ITER_INIT(dlink); \
		data_link_iter_next(&iter, (gpointer *) &ptr);)

gboolean data_link_iter_next(struct data_link_iter *iter, gpointer *data);
*/

#define DATA_LINK_SELF_ENTRY { 0, g_direct_hash, g_direct_equal }


struct data_link *data_link_new(struct data_link_entry *entries, guint nentries);
struct data_link *data_link_new_full(struct data_link_entry *entries,
				     guint nentries,
				     GDestroyNotify func);

void data_link_insert(struct data_link *dlink, gpointer data);
gpointer data_link_lookup(struct data_link *dlink, guint entry_no, gpointer key);
void data_link_steal(struct data_link *dlink, guint entry_no, gpointer key);
void data_link_remove(struct data_link *dlink, guint entry_no, gpointer key);
void data_link_free(struct data_link *dlink);

void data_link_foreach(struct data_link *dlink, GFunc func, gpointer user_data);
void data_link_forregex(struct data_link *dlink, Regex re, guint entry_no,
			GFunc func, gpointer user_data);

gboolean g_pstr_equal(gconstpointer pa, gconstpointer pb);
guint g_pstr_hash(gconstpointer p);

gboolean g_ptr_equal(gconstpointer pa, gconstpointer pb);
guint g_ptr_hash(gconstpointer p);

#endif
