/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbxmlmsg.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbblock.h"
#include "mbblog.h"
#include "mbbxtv.h"
#include "mbbvar.h"

#include "mbbinetmap.h"
#include "range.h"
#include "inet.h"
#include "vmap.h"

#include "variant.h"
#include "xmltag.h"
#include "macros.h"
#include "debug.h"

struct umap_data_entry {
	struct umap_data_entry *next;
	MbbUnit *unit;
	time_t min;
	time_t max;
};

struct umap_entry {
	struct umap_data_entry *data;
	ipv4_t min;
	ipv4_t max;
};

static gboolean map_glue_auto = FALSE;

static struct map global_map = {
	.node_add = mbb_inet_pool_map_add_handler,
	.node_del = mbb_inet_pool_map_del_handler
};

static inline void mbb_map_init(void)
{
	if (global_map.slicer == NULL)
		inet_map_init(&global_map);
}

static gboolean map_add_map(struct map *map, struct map *unit_map, struct map_cross *cross)
{
	MbbInetPoolEntry *entry;
	struct slice key, dkey;
	MapDataIter data_iter;
	MapIter iter;

	map_iter_init(&iter, unit_map);
	while (map_iter_next(&iter, &data_iter, &dkey))
		while (map_data_iter_next(&data_iter, (gpointer *) &entry, &key)) {
			map_add(map, &key, &dkey, entry, cross);

			if (cross->found)
				return FALSE;
		}

	return TRUE;
}

static inline gboolean map_add_map_try(struct map *map, struct map *unit_map)
{
	struct map_cross cross;

	return map_add_map(map, unit_map, &cross);
}

gboolean mbb_map_add_unit(MbbUnit *unit, struct map_cross *cross)
{
	mbb_map_init();

	if (! map_add_map(&global_map, &unit->map, cross)) {
		mbb_map_del_unit(unit);
		return FALSE;
	}

	return TRUE;
}

void mbb_map_del_inet(MbbInetPoolEntry *entry)
{
	GList *list;

	for (list = entry->node_list; list != NULL; list = list->next) {
		MSG_WARN("entry %d: del %p", entry->id, (void *) list->data);

		map_node_remove(&global_map, list->data);
		g_free(list->data);
	}

	entry->node_list = NULL;
}

void mbb_map_del_unit(MbbUnit *unit)
{
	GList *list;

	for (list = unit->sep.queue.head; list != NULL; list = list->next)
		mbb_map_del_inet(list->data);
}

static inline struct umap_data_entry *umap_data_entry_new(struct umap_data_entry *next)
{
	struct umap_data_entry *entry;

	entry = g_new(struct umap_data_entry, 1);
	entry->next = next;

	return entry;
}

static struct umap_data_entry *umap_get_data(MapDataIter *data_iter)
{
	struct umap_data_entry *data = NULL;
	MbbInetPoolEntry *inet_entry;
	struct slice time_slice;

	while (map_data_iter_next(data_iter, (gpointer *) &inet_entry, &time_slice)) {
		data = umap_data_entry_new(data);
		data->min = GPOINTER_TO_TIME(time_slice.begin);
		data->max = GPOINTER_TO_TIME(time_slice.end);
		data->unit = mbb_unit_ref(inet_entry->owner->ptr);
	}

	return data;
}

static void umap_init(GArray *array, struct map *map)
{
	struct umap_entry entry;
	struct slice inet_slice;
	MapDataIter data_iter;
	MapIter iter;

	map_iter_init(&iter, map);
	while (map_iter_next(&iter, &data_iter, &inet_slice)) {
		if (map_data_iter_is_null(&data_iter))
			continue;

		entry.min = GPOINTER_TO_IPV4(inet_slice.begin);
		entry.max = GPOINTER_TO_IPV4(inet_slice.end);
		entry.data = umap_get_data(&data_iter);

		if (entry.data != NULL)
			g_array_append_val(array, entry);
	}
}

MbbUMap *mbb_umap_create(void)
{
	GArray *array;
	gint count;

	if (global_map.slicer == NULL)
		return NULL;

	count = slicer_count(global_map.slicer);
	if (count == 0)
		return NULL;

	array = g_array_sized_new(
		FALSE, FALSE, sizeof(struct umap_entry), count
	);

	umap_init(array, &global_map);

	if (array->len == 0) {
		g_array_free(array, TRUE);
		array = NULL;
	}

	return (MbbUMap *) array;
}

MbbUMap *mbb_umap_from_unit(MbbUnit *unit)
{
	struct map *unit_map = &unit->map;
	struct map imap = MAP_INIT;
	GArray *array;

	if (unit_map->slicer == NULL)
		return NULL;

	if (slicer_count(unit_map->slicer) == 0)
		return NULL;

	inet_map_init(&imap);

	if (map_add_map_try(&imap, unit_map) == FALSE) {
		map_clear(&imap);
		return NULL;
	}

	array = g_array_new(FALSE, FALSE, sizeof(struct umap_entry));
	umap_init(array, &imap);
	map_clear(&imap);

	if (array->len == 0) {
		g_array_free(array, TRUE);
		array = NULL;
	}

	return (MbbUMap *) array;
}

static struct umap_entry *umap_bsearch(struct umap_entry *base, guint len, ipv4_t ip)
{
	struct umap_entry *entry;
	gint max = len - 1;
	gint min = 0;
	gint n;

	while (min <= max) {
		n = (min + max) >> 1;

		entry = base + n;

		if (ip < entry->min)
			max = n - 1;
		else if (ip > entry->max)
			min = n + 1;
		else
			return entry;
	}

	return NULL;
}

static MbbUnit *umap_data_search(struct umap_data_entry *data, time_t t)
{
	if (data->max == 0) {
		if (t >= data->min)
			return data->unit;

		data = data->next;

		if (data == NULL)
			return NULL;
	}

	do {
		if (t > data->max)
			return NULL;
		if (t >= data->min)
			return data->unit;
	} while ((data = data->next) != NULL);

	return NULL;
}

MbbUnit *mbb_umap_find(MbbUMap *umap, ipv4_t ip, time_t t)
{
	struct umap_entry *entry;
	GArray *array;

	array = (GArray *) umap;
	entry = umap_bsearch((struct umap_entry *) array->data, array->len, ip);

	if (entry == NULL)
		return NULL;

	return umap_data_search(entry->data, t);
}

static inline void umap_data_free(struct umap_data_entry *data)
{
	struct umap_data_entry *next;

	for (; data != NULL; data = next) {
		mbb_unit_unref(data->unit);
		next = data->next;
		g_free(data);
	}
}

void mbb_umap_free(MbbUMap *umap)
{
	struct umap_entry *entry;
	GArray *array;
	guint len;

	array = (GArray *) umap;
	entry = (struct umap_entry *) array->data;
	len = array->len;

	/* do umap_data_free((entry++)->data) while (--len); */

	for (; len; len--, entry++)
		umap_data_free(entry->data);

	g_array_free(array, TRUE);
}

static void map_add_unit(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_UNIT_NAME);

	struct map_cross cross;
	MbbUnit *unit;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final {
		mbb_lock_writer_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);
	}

	mbb_map_del_unit(unit);
	if (mbb_map_add_unit(unit, &cross) == FALSE) {
		MbbInetPoolEntry *entry;

		mbb_map_del_unit(unit);
		if (map_glue_auto)
			map_glue_null(&global_map);

		entry = (MbbInetPoolEntry *) cross.data;
		unit = entry->owner->ptr;

		*ans = mbb_xml_msg(MBB_MSG_INET_CROSS, entry->id, unit->name);
	}

	mbb_lock_writer_unlock();
}

static void map_del_unit(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_UNIT_NAME);

	MbbUnit *unit;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final {
		mbb_lock_writer_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);
	}

	mbb_map_del_unit(unit);
	if (map_glue_auto)
		map_glue_null(&global_map);

	mbb_lock_writer_unlock();
}

static inline XmlTag *xml_tag_add_range_tag(XmlTag *tag, struct slice *slice)
{
	ipv4_buf_t min, max;
	XmlTag *xt;

	ipv4toa(min, GPOINTER_TO_IPV4(slice->begin));
	ipv4toa(max, GPOINTER_TO_IPV4(slice->end));

	xt = xml_tag_new_child(tag, "inet_range",
		"min", variant_new_string(min),
		"max", variant_new_string(max)
	);

	return xt;
}

static inline void xml_tag_add_time_tag(XmlTag *tag, MbbUnit *unit,
					struct slice *slice)
{
	time_t begin, end;

	begin = GPOINTER_TO_TIME(slice->begin);
	end = GPOINTER_TO_TIME(slice->end);

	xml_tag_new_child(tag, "time_range",
		"min", variant_new_long(begin),
		"max", variant_new_long(end),
		"unit", variant_new_string(unit->name)
	);
}

static void map_show_common(XmlTag *tag, GCompareFunc cmp_func, gpointer ptr)
{
	struct slice inet_slice, time_slice;
	MbbInetPoolEntry *entry;
	MapDataIter data_iter;
	MapIter iter;
	XmlTag *xt;

	map_iter_init(&iter, &global_map);
	while (map_iter_next(&iter, &data_iter, &inet_slice)) {
		if (cmp_func != NULL)
			xt = NULL;
		else
			xt = xml_tag_add_range_tag(tag, &inet_slice);

		while (map_data_iter_next(&data_iter, (gpointer *) &entry, &time_slice)) {
			if (cmp_func == NULL || cmp_func(entry, ptr)) {
				if (xt == NULL) {
					xt = xml_tag_add_range_tag(
						tag, &inet_slice
					);
				}

				xml_tag_add_time_tag(
					xt, entry->owner->ptr, &time_slice
				);
			}
		}
	}
}

static void map_show_all(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	*ans = mbb_xml_msg_ok();

	mbb_lock_reader_lock();
	map_show_common(*ans, NULL, NULL);
	mbb_lock_reader_unlock();
}

static void map_show_inet(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_INET_ID);

	MbbInetPoolEntry *entry;
	MbbInetPool *pool;
	guint id;

	MBB_XTV_CALL(&id);

	mbb_lock_reader_lock();

	pool = mbb_unit_get_pool();
	entry = mbb_inet_pool_get(pool, id);

	if (entry == NULL) final {
		mbb_lock_reader_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_INET);
	}

	if (entry->node_list != NULL) {
		*ans = mbb_xml_msg_ok();
		map_show_common(*ans, g_direct_equal, entry);
	}

	mbb_lock_reader_unlock();
}

static gint inet_entry_cmp_unit(MbbInetPoolEntry *entry, MbbUnit *unit)
{
	return entry->owner->ptr == unit;
}

static void map_show_unit(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_UNIT_NAME);

	MbbUnit *unit;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_reader_lock();

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final {
		mbb_lock_reader_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);
	}

	if (mbb_unit_mapped(unit)) {
		*ans = mbb_xml_msg_ok();
		map_show_common(*ans, (GCompareFunc) inet_entry_cmp_unit, unit);
	}

	mbb_lock_reader_unlock();
}

static void inet_map_clear(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans G_GNUC_UNUSED)
{
	mbb_lock_writer_lock();
	map_clear(&global_map);
	mbb_lock_writer_unlock();
}

static void inet_map_glue_null(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans G_GNUC_UNUSED)
{
	mbb_lock_writer_lock();
	map_glue_null(&global_map);
	mbb_lock_writer_unlock();
}

static GSList *map_search_net(struct map *map, struct inet *inet,
			      time_t start, time_t end)
{
	struct slice dkey;
	struct slice key;
	struct range r;

	dkey.begin = TIME_TO_POINTER(start);
	dkey.end = TIME_TO_POINTER(end);
	inet2range(inet, &r);
	key.begin = IPV4_TO_POINTER(r.r_min);
	key.end = IPV4_TO_POINTER(r.r_max);

	return map_search(map, &key, &dkey);
}

static void xml_tag_add_inet_list(XmlTag *tag, GSList *list, MbbUnit *main)
{
	MbbInetPoolEntry *entry;
	MbbUnit *unit;

	for (; list != NULL; list = list->next) {
		entry = list->data;
		unit = entry->owner->ptr;

		if (unit == main)
			continue;

		xml_tag_new_child(tag, "inet_entry",
			"id", variant_new_int(entry->id),
			"owner", variant_new_string(unit->name)
		);
	}
}

static void map_find_net(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NET_VALUE, XTV_TIME_START_, XTV_TIME_END_);

	time_t start, end;
	struct inet inet;
	GSList *list;

	start = end = VAR_CONV_TIME_UNSET;
	MBB_XTV_CALL(&inet, &start, &end);

	if (start == VAR_CONV_TIME_UNSET) {
		if (end == VAR_CONV_TIME_UNSET)
			start = end = 0;
		else
			start = end;
	} else if (end == VAR_CONV_TIME_UNSET)
		end = start;
	else if (start > end) final
		*ans = mbb_xml_msg(MBB_MSG_INVALID_TIME_ORDER);

	mbb_lock_reader_lock();

	if (global_map.slicer == NULL) final
		mbb_lock_reader_unlock();

	list = map_search_net(&global_map, &inet, start, end);
	*ans = mbb_xml_msg_ok();
	xml_tag_add_inet_list(*ans, list, NULL);

	mbb_lock_reader_unlock();

	g_slist_free(list);
}

static void map_search_unit(struct map *map, MbbUnit *unit, XmlTag *tag)
{
	MbbInetPoolEntry *entry;
	struct slice key, dkey;
	MapDataIter data_iter;
	MapIter iter;
	GSList *list;

	map_iter_init(&iter, &unit->map);
	while (map_iter_next(&iter, &data_iter, &dkey))
		while (map_data_iter_next(&data_iter, (gpointer *) &entry, &key)) {
			list = map_search(map, &key, &dkey);
			xml_tag_add_inet_list(tag, list, unit);
			g_slist_free(list);
		}
}

static void map_find_unit(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	MbbUnit *unit;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_reader_lock();

	on_final { mbb_lock_reader_unlock(); }

	if (global_map.slicer == NULL)
		final;

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);

	*ans = mbb_xml_msg_ok();
	map_search_unit(&global_map, unit, *ans);

	mbb_lock_reader_unlock();
}

MBB_INIT_FUNCTIONS_DO
	MBB_FUNC_STRUCT("mbb-map-add-unit", map_add_unit, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-map-del-unit", map_del_unit, MBB_CAP_WHEEL),

	MBB_FUNC_STRUCT("mbb-map-show-all", map_show_all, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-map-show-inet", map_show_inet, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-map-show-unit", map_show_unit, MBB_CAP_ADMIN),

	MBB_FUNC_STRUCT("mbb-map-clear", inet_map_clear, MBB_CAP_WHEEL),
	MBB_FUNC_STRUCT("mbb-map-glue-null", inet_map_glue_null, MBB_CAP_WHEEL),

	MBB_FUNC_STRUCT("mbb-map-find-net", map_find_net, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-map-find-unit", map_find_unit, MBB_CAP_ADMIN),
MBB_INIT_FUNCTIONS_END

MBB_VAR_DEF(mga_def) {
	.op_read = var_str_bool,
	.op_write = var_conv_bool,
	.cap_read = MBB_CAP_ALL,
	.cap_write = MBB_CAP_ROOT
};

static void init_vars(void)
{
	mbb_base_var_register("map.glue.auto", &mga_def, &map_glue_auto);
}

MBB_ON_INIT(MBB_INIT_VARS, MBB_INIT_FUNCTIONS)
