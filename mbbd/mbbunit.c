#include "mbbinetmap.h"
#include "mbbunit.h"
#include "mbbtime.h"
#include "range.h"

#include "datalink.h"
#include "macros.h"

enum {
	MBB_ACC_UNIT_SELF,
	MBB_ACC_UNIT_ID,
	MBB_ACC_UNIT_NAME
};

static struct data_link_entry dl_entries[] = {
	DATA_LINK_SELF_ENTRY,
	{ G_STRUCT_OFFSET(struct mbb_unit, id), g_int_hash, g_int_equal },
	{ G_STRUCT_OFFSET(struct mbb_unit, name), g_pstr_hash, g_pstr_equal }
};

static void inet_pool_entry_del(MbbInetPoolEntry *entry);

static struct mbb_object_interface self_interface = {
	.get_id = (get_int_func_t) mbb_unit_get_id,
	.get_start = (get_time_func_t) mbb_unit_get_start,
	.get_end = (get_time_func_t) mbb_unit_get_end,
	.get_pool = (get_ptr_func_t) mbb_unit_get_pool,
	.on_delete = (void_func_t) inet_pool_entry_del
};

static DataLink *dlink = NULL;
static MbbInetPool *inet_pool = NULL;
static GHashTable *ht_local = NULL;

static inline void dlink_init(void)
{
	dlink = data_link_new_full(
		dl_entries, NELEM(dl_entries), (GDestroyNotify) mbb_unit_free
	);
}

static inline void inet_pool_init(void)
{
	inet_pool = mbb_inet_pool_new();
}

static inline void ht_local_init(void)
{
	ht_local = g_hash_table_new(g_direct_hash, g_direct_equal);
}

static inline void mbb_unit_map_init(struct mbb_unit *unit)
{
	if (unit->map.slicer == NULL)
		time_map_init(&unit->map);
}

struct mbb_unit *mbb_unit_new(gint id, gchar *name, time_t start, time_t end)
{
	struct mbb_unit *unit;

	unit = g_new(struct mbb_unit, 1);
	unit->id = id;
	unit->name = g_strdup(name);

	MBB_OBJ_REF_INIT(unit);

	unit->con = NULL;

	unit->start = start;
	unit->end = end;

	unit->sep = (Separator) SEPARATOR_INIT;

	unit->self.ptr = unit;
	unit->self.methods = &self_interface;

	unit->map = (struct map) MAP_INIT;
	unit->local = FALSE;

	return unit;
}

void mbb_unit_free(struct mbb_unit *unit)
{
	mbb_unit_clear_inet(unit);

	g_free(unit->name);
	g_free(unit);
}

void mbb_unit_join(struct mbb_unit *unit)
{
	if (dlink == NULL)
		dlink_init();

	data_link_insert(dlink, unit);

	if (unit->local)
		mbb_unit_local_add(unit);
}

void mbb_unit_detach(struct mbb_unit *unit)
{
	if (dlink == NULL)
		return;

	data_link_steal(dlink, MBB_ACC_UNIT_SELF, unit);

	if (unit->local)
		mbb_unit_local_del(unit);
}

void mbb_unit_remove(struct mbb_unit *unit)
{
	if (unit->con != NULL) {
		mbb_consumer_del_unit(unit->con, unit);
		unit->con = NULL;
	}

	mbb_unit_detach(unit);
	mbb_unit_unref(unit);
}

void mbb_unit_local_add(struct mbb_unit *unit)
{
	if (ht_local == NULL)
		ht_local_init();

	g_hash_table_insert(ht_local, GINT_TO_POINTER(unit->id), unit);
	unit->local = TRUE;
}

void mbb_unit_local_del(struct mbb_unit *unit)
{
	if (ht_local != NULL) {
		g_hash_table_remove(ht_local, GINT_TO_POINTER(unit->id));
		unit->local = FALSE;
	}
}

gint mbb_unit_get_id(struct mbb_unit *unit)
{
	return unit->id;
}

time_t mbb_unit_get_start(struct mbb_unit *unit)
{
	if (unit->start >= 0 || unit->con == NULL)
		return unit->start;

	return unit->con->start;
}

time_t mbb_unit_get_end(struct mbb_unit *unit)
{
	if (unit->end >= 0 || unit->con == NULL)
		return unit->end;

	return unit->con->end;
}

MbbInetPool *mbb_unit_get_pool(void)
{
	return inet_pool;
}

static void inet_pool_entry_del(MbbInetPoolEntry *entry)
{
	mbb_unit_del_inet(entry->owner->ptr, entry);
}

gboolean mbb_unit_mod_name(struct mbb_unit *unit, gchar *newname)
{
	if (dlink == NULL || mbb_unit_get_by_name(newname))
		return FALSE;

	data_link_steal(dlink, MBB_ACC_UNIT_SELF, unit);
	g_free(unit->name);
	unit->name = g_strdup(newname);
	data_link_insert(dlink, unit);

	return TRUE;
}

static gboolean unit_test_inet_time(GList *childs, time_t start, time_t end)
{
	GList *list;

	for (list = childs; list != NULL; list = list->next)
		if (! mbb_inet_pool_entry_test_time(list->data, start, end))
			return FALSE;

	return TRUE;
}

gboolean mbb_unit_test_time(struct mbb_unit *unit, time_t start, time_t end)
{
	if (unit->start >= 0)
		start = unit->start;
	if (unit->end >= 0)
		end = unit->end;

	if (mbb_time_becmp(start, end) > 0)
		return FALSE;

	return unit_test_inet_time(unit->sep.queue.head, start, end);
}

gboolean mbb_unit_mod_time(struct mbb_unit *unit, time_t start, time_t end)
{
	time_t ts, te;

	ts = start < 0 ? unit->con->start : start;
	te = end < 0 ? unit->con->end : end;

	if (! unit_test_inet_time(unit->sep.queue.head, ts, te))
		return FALSE;

	unit->start = start;
	unit->end = end;

	return TRUE;
}

struct mbb_unit *mbb_unit_get_by_id(gint id)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_ACC_UNIT_ID, &id);
}

struct mbb_unit *mbb_unit_get_by_name(gchar *name)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_ACC_UNIT_NAME, &name);
}

void mbb_unit_foreach(GFunc func, gpointer user_data)
{
	data_link_foreach(dlink, func, user_data);
}

void mbb_unit_forregex(Regex re, GFunc func, gpointer user_data)
{
	data_link_forregex(dlink, re, MBB_ACC_UNIT_NAME, func, user_data);
}

void mbb_unit_local_forregex(Regex re, GFunc func, gpointer user_data)
{
	GHashTableIter iter;
	MbbUnit *unit;

	if (ht_local == NULL)
		return;

	g_hash_table_iter_init(&iter, ht_local);

	if (re == NULL) {
		while (g_hash_table_iter_next(&iter, NULL, (gpointer *) &unit))
			func(unit, user_data);
	} else {
		while (g_hash_table_iter_next(&iter, NULL, (gpointer *) &unit)) {
			if (re_ismatch(re, unit->name))
				func(unit, user_data);
		}
	}
}

void mbb_unit_inherit_time(struct mbb_unit *unit)
{
	if (unit->con != NULL) {
		if (unit->start < 0)
			unit->start = unit->con->start;
		if (unit->end < 0)
			unit->end = unit->con->end;
	}
}

gboolean mbb_unit_unset_consumer(struct mbb_unit *unit)
{
	if (unit->start < 0 || unit->end < 0)
		return FALSE;

	unit->con = NULL;

	return TRUE;
}

void mbb_unit_set_consumer(struct mbb_unit *unit, struct mbb_consumer *con)
{
	unit->con = con;
}

void mbb_unit_add_inet(struct mbb_unit *unit, MbbInetPoolEntry *entry)
{
	if (separator_has(&unit->sep, entry) == FALSE) {
		separator_add(&unit->sep, entry);

		if (inet_pool == NULL)
			inet_pool_init();

		mbb_inet_pool_add(inet_pool, entry);
	}
}

void mbb_unit_del_inet(struct mbb_unit *unit, MbbInetPoolEntry *entry)
{
	if (separator_has(&unit->sep, entry)) {
		separator_del(&unit->sep, entry);
		map_remove_custom(&unit->map, entry, g_direct_equal);
		mbb_map_del_inet(entry);
		mbb_inet_pool_del(inet_pool, entry->id);
	}
}

void mbb_unit_clear_inet(struct mbb_unit *unit)
{
	MbbInetPoolEntry *entry;
	GList *list;

	for (list = unit->sep.queue.head; list != NULL; list = list->next) {
		entry = (MbbInetPoolEntry *) list->data;

		mbb_map_del_inet(entry);
		mbb_inet_pool_del(inet_pool, entry->id);
	}

	separator_clear(&unit->sep);
	map_clear(&unit->map);
}

/*
gboolean mbb_unit_apply_inet(struct mbb_unit *unit, MbbInetPoolEntry *entry, gboolean reorder)
{
	Separator *sep;
	Separator tmp_sep;

	sep = &unit->sep;
	if (separator_has(sep, entry) == FALSE)
		return FALSE;

	if (reorder) {
		separator_init(&tmp_sep);
		sep = separator_copy(&tmp_sep, sep, entry);
		separator_add(sep, entry);
	}

	here some tests will be

	if (reorder)
		separator_move(&unit->sep, sep);

	return TRUE;
}
*/

void mbb_unit_sep_reorder(MbbInetPoolEntry *entry)
{
	Separator *sep;
	MbbUnit *unit;

	unit = entry->owner->ptr;
	sep = &unit->sep;

	separator_del(sep, entry);
	separator_add(sep, entry);
}

static gboolean unit_map_add_inet(struct map *map, MbbInetPoolEntry *entry,
				  struct map_cross *cross)
{
	struct slice key, dkey;
	struct range r;

	inet2range(&entry->inet, &r);

	key.begin = TIME_TO_POINTER(mbb_inet_pool_entry_get_start(entry));
	key.end = TIME_TO_POINTER(mbb_inet_pool_entry_get_end(entry));

	dkey.begin = IPV4_TO_POINTER(r.r_min);
	dkey.end = IPV4_TO_POINTER(r.r_max);

	if (entry->flag)
		map_add(map, &key, &dkey, entry, cross);
	else {
		map_del(map, &key, &dkey, NULL);
		cross->found = FALSE;
	}

	return ! cross->found;
}

gboolean mbb_unit_map_rebuild(struct mbb_unit *unit, MbbInetPoolEntry *pp[2])
{
	MbbInetPoolEntry *entry;
	struct map_cross cross;
	struct map map;
	GList *list;

	if (separator_count(&unit->sep) == 0) {
		map_clear(&unit->map);
		return TRUE;
	}

	map_init(&map, NULL);
	time_map_init(&map);
	for (list = unit->sep.queue.head; list != NULL; list = list->next) {
		entry = (MbbInetPoolEntry *) list->data;

		if (unit_map_add_inet(&map, entry, &cross) == FALSE) {
			if (pp != NULL) {
				pp[0] = cross.data;
				pp[1] = entry;
			}

			map_clear(&map);
			return FALSE;
		}
	}

	map_move(&unit->map, &map);
	return TRUE;
}

gboolean mbb_unit_mapped(struct mbb_unit *unit)
{
	MbbInetPoolEntry *entry;
	GList *list;

	for (list = unit->sep.queue.head; list != NULL; list = list->next) {
		entry = (MbbInetPoolEntry *) list->data;

		if (entry->node_list != NULL)
			return TRUE;
	}

	return FALSE;
}

