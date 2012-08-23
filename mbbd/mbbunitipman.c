/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbdbinetpool.h"
#include "mbbinetpool.h"
#include "mbbunit.h"

#include "mbbxmlmsg.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbblock.h"
#include "mbbtime.h"
#include "mbblog.h"
#include "mbbxtv.h"

#include "variant.h"
#include "macros.h"
#include "xmltag.h"

static XmlTag *gather_entry(MbbInetPoolEntry *entry, XmlTag *tag)
{
	time_t start, end;
	XmlTag *xt;

	start = mbb_inet_pool_entry_get_start(entry);
	end = mbb_inet_pool_entry_get_end(entry);

	xml_tag_add_child(tag, xt = xml_tag_newc(
		"inet_entry",
		"id", variant_new_int(entry->id),
		"inet", variant_new_alloc_string(inettoa(NULL, &entry->inet)),
		"start", variant_new_long(start),
		"end", variant_new_long(end),
		"nice", variant_new_int(entry->nice)
	));

	if (entry->start < 0)
		xml_tag_set_attr(xt,
			"parent_start", variant_new_static_string("true")
		);

	if (entry->end < 0)
		xml_tag_set_attr(xt,
			"parent_end", variant_new_static_string("true")
		);

	if (entry->flag == FALSE)
		xml_tag_set_attr(xt,
			"exclusive", variant_new_static_string("true")
		);

	return xt;
}

static void gather_entry_with_owner(MbbInetPoolEntry *entry, XmlTag *tag)
{
	MbbUnit *unit;
	XmlTag *xt;

	xt = gather_entry(entry, tag);
	unit = (MbbUnit *) entry->owner->ptr;

	xml_tag_set_attr(xt, "unit", variant_new_string(unit->name));
}

static void unit_show_raw_inet(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTVN(xtvars, XTV_UNIT_NAME_);
	gchar *name = NULL;

	xml_tag_scan(tag, xtvars, NULL, &name);

	mbb_lock_reader_lock();

	if (name == NULL) {
		MbbInetPool *pool;

		*ans = mbb_xml_msg_ok();
		pool = mbb_unit_get_pool();

		mbb_inet_pool_foreach(
			pool, (GFunc) gather_entry_with_owner, *ans
		);
	} else {
		MbbUnit *unit;

		unit = mbb_unit_get_by_name(name);
		if (unit == NULL) final {
			mbb_lock_reader_unlock();
			*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);
		}

		*ans = mbb_xml_msg_ok();
		g_queue_foreach(&unit->sep.queue, (GFunc) gather_entry, *ans);
	}

	mbb_lock_reader_unlock();
}

static void unit_add_inet(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(
		XTV_UNIT_NAME, XTV_INET_INET,
		XTV_INET_EXCLUSIVE_, XTV_INET_NICE_, XTV_INET_INHERIT_
	);

	MbbInetPoolEntry entry;
	MbbUnit *unit;
	gchar *name;

	gboolean inherit = FALSE;
	GError *error = NULL;

	mbb_inet_pool_entry_init(&entry);

	entry.flag = FALSE;
	entry.nice = 0;
	MBB_XTV_CALL(&name, &entry.inet, &entry.flag, &entry.nice, &inherit);
	entry.flag = ! entry.flag;

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);

	if (inherit == FALSE && mbb_unit_get_end(unit) != 0) final
		*ans = mbb_xml_msg(MBB_MSG_UNIT_FREEZED, name);

	entry.owner = &unit->self;
	entry.end = -1;
	if (inherit)
		entry.start = -1;
	else
		time(&entry.start);

	entry.id = mbb_db_inet_pool_add(&entry, unit->id, &error);
	if (entry.id < 0) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_unit_add_inet(unit, g_memdup(&entry, sizeof(entry)));

	mbb_lock_writer_unlock();
}

static void drop_inet(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_INET_ID);

	MbbInetPoolEntry *entry;
	MbbInetPool *pool;
	GError *error;
	gint id;

	MBB_XTV_CALL(&id);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	pool = mbb_unit_get_pool();
	entry = mbb_inet_pool_get(pool, id);

	if (entry == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_INET);

	error = NULL;
	if (! mbb_db_inet_pool_drop(entry->id, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_unit_del_inet(entry->owner->ptr, entry);

	mbb_lock_writer_unlock();
}

static void unit_clear_inet(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_UNIT_NAME);

	MbbUnit *unit;
	gchar *name;
	GError *error;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);

	error = NULL;
	if (! mbb_db_inet_pool_clear(unit->id, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_unit_clear_inet(unit);

	mbb_lock_writer_unlock();

	mbb_log_debug("unit '%s' clear inet", name);
}

static void unit_mod_inet_time(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_INET_ID, XTV_ETIME_START_, XTV_ETIME_END_);

	MbbInetPoolEntry *entry;
	time_t start, end;
	gint id;

	start = end = MBB_TIME_UNSET;
	MBB_XTV_CALL(&id, &start, &end);

	if (start == MBB_TIME_UNSET && end == MBB_TIME_UNSET) final
		*ans = mbb_xml_msg(MBB_MSG_TIME_MISSED);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	entry = mbb_inet_pool_get(mbb_unit_get_pool(), id);

	if (entry == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_INET);

	exec_if(start = entry->start, start == MBB_TIME_UNSET);
	exec_if(end = entry->end, end == MBB_TIME_UNSET);

	*ans = mbb_time_test_order(start, end,
		mbb_unit_get_start(entry->owner->ptr),
		mbb_unit_get_end(entry->owner->ptr));

	if (*ans == NULL) {
		GError *error = NULL;

		if (! mbb_db_inet_pool_mod_time(entry->id, start, end, &error))
			final *ans = mbb_xml_msg_from_error(error);
	}

	entry->start = start;
	entry->end = end;

	mbb_lock_writer_unlock();
}

static void unit_mod_inet_nice(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_INET_ID, XTV_NEWNICE_VALUE);

	MbbInetPoolEntry *entry;
	guint id, nice;
	GError *error;

	MBB_XTV_CALL(&id, &nice);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	entry = mbb_inet_pool_get(mbb_unit_get_pool(), id);

	if (entry == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_INET);

	if (entry->nice == nice)
		final;

	error = NULL;
	if (! mbb_db_inet_pool_mod_nice(id, nice, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	entry->nice = nice;
	mbb_unit_sep_reorder(entry);

	mbb_lock_writer_unlock();
}

static inline XmlTag *xml_tag_add_time_range(XmlTag *tag, struct slice *slice)
{
	time_t min, max;
	XmlTag *xt;

	min = GPOINTER_TO_TIME(slice->begin);
	max = GPOINTER_TO_TIME(slice->end);

	xml_tag_add_child(tag, xt = xml_tag_newc(
		"time_range",
		"min", variant_new_long(min),
		"max", variant_new_long(max)
	));

	return xt;
}

static void map_show(struct map *map, XmlTag *tag, gpointer ptr, gboolean all)
{
	MbbInetPoolEntry *entry;
	struct slice time_slice;
	struct slice inet_slice;
	MapDataIter data_iter;
	MapIter iter;
	XmlTag *xt;

	map_iter_init(&iter, map);

	if (all == FALSE)
		map_iter_last(&iter);

	while (map_iter_next(&iter, &data_iter, &time_slice)) {
		if (all == FALSE && time_slice.end != NULL)
			break;

		if (ptr != NULL)
			xt = NULL;
		else
			xt = xml_tag_add_time_range(tag, &time_slice);

		while (map_data_iter_next(&data_iter, (gpointer *) &entry, &inet_slice)) {
			ipv4_buf_t min, max;

			if (ptr != NULL && ptr != entry)
				continue;

			if (xt == NULL)
				xt = xml_tag_add_time_range(tag, &time_slice);

			ipv4toa(min, GPOINTER_TO_IPV4(inet_slice.begin));
			ipv4toa(max, GPOINTER_TO_IPV4(inet_slice.end));

			xml_tag_add_child(xt, xml_tag_newc(
				"inet_range",
				"min", variant_new_string(min),
				"max", variant_new_string(max),
				"id", variant_new_int(entry->id)
			));
		}
	}
}

static void unit_map_show_common(XmlTag *tag, XmlTag **ans, gboolean all)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	MbbUnit *unit;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_reader_lock();

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final {
		mbb_lock_reader_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);
	}

	*ans = mbb_xml_msg_ok();
	map_show(&unit->map, *ans, NULL, all);

	mbb_lock_reader_unlock();
}

static void unit_map_showall(XmlTag *tag, XmlTag **ans)
{
	unit_map_show_common(tag, ans, TRUE);
}

static void unit_map_show(XmlTag *tag, XmlTag **ans)
{
	unit_map_show_common(tag, ans, FALSE);
}

static void unit_map_rebuild(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	MbbInetPoolEntry *pp[2];
	MbbUnit *unit;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();
	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final {
		mbb_lock_writer_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);
	}

	if (mbb_unit_map_rebuild(unit, pp) == FALSE) {
		*ans = mbb_xml_msg(MBB_MSG_INET_COLLISION);
		gather_entry(pp[0], *ans);
		gather_entry(pp[1], *ans);
	}

	mbb_lock_writer_unlock();
}

static void unit_map_inet_showall(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_INET_ID);

	MbbInetPoolEntry *entry;
	MbbInetPool *pool;
	MbbUnit *unit;
	guint id;

	MBB_XTV_CALL(&id);

	mbb_lock_reader_lock();

	pool = mbb_unit_get_pool();
	entry = mbb_inet_pool_get(pool, id);
	if (entry == NULL) final {
		mbb_lock_reader_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_INET);
	}

	*ans = mbb_xml_msg_ok();
	unit = entry->owner->ptr;
	map_show(&unit->map, *ans, entry, TRUE);

	mbb_lock_reader_unlock();
}

static void unit_map_clear(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	MbbUnit *unit;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final {
		mbb_lock_writer_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);
	}

	map_clear(&unit->map);

	mbb_lock_writer_unlock();
}

MBB_INIT_FUNCTIONS_DO
	MBB_FUNC_STRUCT("mbb-unit-show-raw-inet", unit_show_raw_inet, MBB_CAP_ADMIN),

	MBB_FUNC_STRUCT("mbb-unit-add-inet", unit_add_inet, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-unit-drop-inet", drop_inet, MBB_CAP_WHEEL),
	MBB_FUNC_STRUCT("mbb-unit-clear-inet", unit_clear_inet, MBB_CAP_WHEEL),

	MBB_FUNC_STRUCT("mbb-unit-mod-inet-time", unit_mod_inet_time, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-unit-mod-inet-nice", unit_mod_inet_nice, MBB_CAP_ADMIN),

	MBB_FUNC_STRUCT("mbb-unit-map-showall", unit_map_showall, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-unit-map-show", unit_map_show, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-unit-map-inet-showall", unit_map_inet_showall, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-unit-map-rebuild", unit_map_rebuild, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-unit-map-clear", unit_map_clear, MBB_CAP_WHEEL),
MBB_INIT_FUNCTIONS_END

MBB_ON_INIT(MBB_INIT_FUNCTIONS)
