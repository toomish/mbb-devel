#include "mbbmodule.h"

#include "mbbxmlmsg.h"
#include "mbbplock.h"
#include "mbblock.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbbxtv.h"
#include "mbblog.h"
#include "mbbdb.h"

#include "varconv.h"
#include "xmltag.h"
#include "macros.h"
#include "query.h"
#include "debug.h"

#include "interface.h"
#include "dbattr.h"
#include "attr.h"

#include <stdarg.h>

static GHashTable *ht_group;

static void attr_group_free(struct attr_group *ag) {
	g_slist_free(ag->attr_list);
}

static inline void ht_group_init(void)
{
	struct attr_group *ag;

	ht_group = g_hash_table_new_full(
		g_str_hash, g_str_equal, NULL, (GDestroyNotify) attr_group_free
	);

	for (ag = attr_group_table; ag->name != NULL; ag++)
		g_hash_table_insert(ht_group, ag->name, ag);
}

static inline struct attr_group *attr_group_get(gchar *name)
{
	struct attr_group *ag;

	ag = g_hash_table_lookup(ht_group, name);

	if (ag == NULL || ag->id < 0)
		return NULL;

	return ag;
}

static gboolean db_attr_group_load(void)
{
	GError *error = NULL;
	MbbDbIter *iter;

	iter = mbb_db_select(&error, "attribute_group", 
		"group_id", "group_name", NULL, NULL
	);

	if (iter == NULL) {
		mbb_log_self("db_attr_group_load failed: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	ht_group_init();

	while (mbb_db_iter_next(iter)) {
		struct attr_group *ag;
		gchar *var;
		gint id;

		var = mbb_db_iter_value(iter, 0);
		if (! var_conv_int(var, &id)) {
			mbb_log_self("non integer group_id '%s'", var);
			mbb_db_iter_free(iter);

			return FALSE;
		}

		var = mbb_db_iter_value(iter, 1);
		ag = g_hash_table_lookup(ht_group, var);
		if (ag != NULL)
			ag->id = id;
		else
			mbb_log_self("unknown group_name '%s'", var);
	}

	mbb_db_iter_free(iter);
	return TRUE;
}

static gboolean db_attr_load(void)
{
	GError *error = NULL;
	MbbDbIter *iter;

	iter = mbb_db_select(&error, "attributes a, attribute_group g",
		"g.group_name", "a.attr_id", "a.attr_name", NULL,
		"a.group_id = g.group_id"
	);

	if (iter == NULL) {
		mbb_log_self("db_attr_load failed: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	while (mbb_db_iter_next(iter)) {
		struct attr_group *ag;
		gchar *var;
		gint id;

		var = mbb_db_iter_value(iter, 0);
		if ((ag = attr_group_get(var)) == NULL) {
			mbb_log_self("invalid attr group '%s'", var);
			goto fail;
		}

		var = mbb_db_iter_value(iter, 1);
		if (! var_conv_int(var, &id)) {
			mbb_log_self("non integer attr_id '%s'", var);
			goto fail;
		}

		var = mbb_db_iter_value(iter, 2);
		if (attr_get(ag->name, var) != NULL) {
			mbb_log_self("duplicate attr '%s' for group '%s'",
				var, ag->name);
			goto fail;
		}

		attr_add(ag, id, var);
	}

	mbb_db_iter_free(iter);
	return TRUE;

fail:
	attr_free_all();
	mbb_db_iter_free(iter);
	return FALSE;
}

static void mbb_attr_list(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_ATTR_GROUP);

	struct attr_group *ag;
	gchar *group;
	GSList *list;

	MBB_XTV_CALL(&group);

	if ((ag = attr_group_get(group)) == NULL) final
		*ans = mbb_xml_msg_error("unknown group");

	*ans = mbb_xml_msg_ok();

	mbb_plock_reader_lock();
	for (list = ag->attr_list; list != NULL; list = list->next) {
		struct attr *attr;

		attr = list->data;
		xml_tag_new_child(*ans, "attr",
			"name", variant_new_string(attr->name)
		);
	}
	mbb_plock_reader_unlock();
}

static void mbb_attr_add(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_ATTR_GROUP, XTV_ATTR_NAME);

	struct attr_group *ag;
	gchar *group;
	gchar *name;

	MBB_XTV_CALL(&group, &name);

	if ((ag = attr_group_get(group)) == NULL) final
		*ans = mbb_xml_msg_error("unknown group");

	mbb_plock_writer_lock();

	on_final { mbb_plock_writer_unlock(); }

	if (attr_get(ag->name, name) != NULL) final
		*ans = mbb_xml_msg_error("attr '%s' exists", name);
	else {
		GError *error = NULL;
		gint id;

		id = db_attr_add(ag->id, name, &error);
		if (id < 0) final
			*ans = mbb_xml_msg_from_error(error);

		attr_add(ag, id, name);
		mbb_log_debug("add attr %s with id %d to group %s",
			name, id, group
		);
	}

	mbb_plock_writer_unlock();
}

static void mbb_attr_del(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_ATTR_GROUP, XTV_ATTR_NAME);

	struct attr *attr;
	gchar *group;
	gchar *name;

	MBB_XTV_CALL(&group, &name);

	mbb_plock_writer_lock();

	on_final { mbb_plock_writer_unlock(); }

	if ((attr = attr_get(group, name)) == NULL) final
		*ans = mbb_xml_msg_error("unknown attr");
	else {
		GError *error = NULL;

		if (db_attr_del(attr->id, &error) == FALSE) final
			*ans = mbb_xml_msg_from_error(error);

		attr_del(attr);
		mbb_log_debug("del attr %s from group %s", name, group);
	}

	mbb_plock_writer_unlock();
}

static void mbb_attr_rename(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_ATTR_GROUP, XTV_ATTR_NAME, XTV_NEWNAME_VALUE);

	struct attr *attr;
	gchar *group;
	gchar *name;
	gchar *newname;

	MBB_XTV_CALL(&group, &name, &newname);

	mbb_plock_writer_lock();

	on_final { mbb_plock_writer_unlock(); }

	if ((attr = attr_get(group, name)) == NULL) final
		*ans = mbb_xml_msg_error("unknown attr");
	else if (attr_get(group, newname) != NULL) final
		*ans = mbb_xml_msg_error("name exists");
	else {
		GError *error = NULL;

		if (db_attr_rename(attr->id, newname, &error) == FALSE) final
			*ans = mbb_xml_msg_from_error(error);

		attr_rename(attr, newname);
		mbb_log_debug("rename attr %s to %s from group %s",
			name, newname, group
		);
	}

	mbb_plock_writer_unlock();
}

static void mbb_attr_set(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_ATTR_GROUP, XTV_OBJ_NAME, XTV_ATTR_NAME, XTV_ATTR_VALUE);

	struct attr *attr;
	gchar *group;
	gchar *name;
	gchar *value;
	gchar *obj;

	MBB_XTV_CALL(&group, &obj, &name, &value);

	mbb_plock_reader_lock();

	on_final { mbb_plock_reader_unlock(); }

	if ((attr = attr_get(group, name)) == NULL) final
		*ans = mbb_xml_msg_error("unknown attr");
	else {
		GError *error = NULL;
		gint id;

		mbb_lock_reader_lock();
		id = attr_group_get_obj_id(attr->group, obj);
		if (id < 0) {
			mbb_lock_reader_unlock();
			final *ans = mbb_xml_msg_error("unknown object");
		}

		if (db_attr_set(attr->id, value, id, &error) == FALSE) {
			mbb_lock_reader_unlock();
			final *ans = mbb_xml_msg_from_error(error);
		}

		mbb_lock_reader_unlock();
	}

	mbb_plock_reader_unlock();
}

static void mbb_attr_unset(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_ATTR_GROUP, XTV_OBJ_NAME, XTV_ATTR_NAME);

	struct attr *attr;
	gchar *group;
	gchar *name;
	gchar *obj;

	MBB_XTV_CALL(&group, &obj, &name);

	mbb_plock_reader_lock();

	on_final { mbb_plock_reader_unlock(); }

	if ((attr = attr_get(group, name)) == NULL) final
		*ans = mbb_xml_msg_error("unknown attr");
	else {
		GError *error = NULL;
		gint id;

		mbb_lock_reader_lock();
		id = attr_group_get_obj_id(attr->group, obj);
		if (id < 0) {
			mbb_lock_reader_unlock();
			final *ans = mbb_xml_msg_error("unknown object");
		}

		if (db_attr_unset(attr->id, id, &error) == FALSE) {
			mbb_lock_reader_unlock();
			final *ans = mbb_xml_msg_from_error(error);
		}

		mbb_lock_reader_unlock();
	}

	mbb_plock_reader_unlock();
}

static XmlTag *db_fetch_attrs(gchar *query)
{
	GError *error = NULL;
	MbbDbIter *iter;
	XmlTag *tag;

	iter = mbb_db_query_iter(query, &error);
	if (iter == NULL)
		return mbb_xml_msg_from_error(error);

	tag = mbb_xml_msg_ok();
	while (mbb_db_iter_next(iter)) {
		struct attr *attr;
		gchar *value;
		gchar *var;
		gint id;

		var = mbb_db_iter_value(iter, 0);
		if (! var_conv_int(var, &id))
			continue;

		if ((attr = attr_get_by_id(id)) == NULL)
			continue;

		value = mbb_db_iter_value(iter, 1);

		xml_tag_new_child(tag, "attr",
			"name", variant_new_string(attr->name),
			"value", variant_new_string(value)
		);
	}

	mbb_db_iter_free(iter);

	return tag;
}

static GSList *get_attr_list(gchar *group, GSList *var_list)
{
	GSList *list = NULL;

	for (; var_list != NULL; var_list = var_list->next) {
		struct attr *attr;
		gchar *s;

		s = variant_get_string(var_list->data);
		attr = attr_get(group, s);
		if (attr != NULL)
			list = g_slist_prepend(list, attr);
	}

	return list;
}

static XmlTag *db_attr_get(GSList *list, gint obj_id)
{
	struct attr *attr;
	gchar *query;

	attr = list->data;
	query_format("select * from attr_get(array[%d", attr->id);

	for (list = list->next; list != NULL; list = list->next) {
		attr = list->data;
		query_append_format(",%d", attr->id);
	}

	query = query_append_format("], %d);", obj_id);

	return db_fetch_attrs(query);
}

static gint db_read_attrs(gchar *query, GHashTable *ht, GError **error)
{
	MbbDbIter *iter;
	gsize nelem = 0;

	iter = mbb_db_query_iter(query, error);
	if (iter == NULL)
		return -1;

	while (mbb_db_iter_next(iter)) {
		struct attrvec *av;
		gchar *var;
		gint id;

		var = mbb_db_iter_value(iter, 0);
		if (! var_conv_int(var, &id))
			continue;

		var = mbb_db_iter_value(iter, 1);
		av = g_hash_table_lookup(ht, GINT_TO_POINTER(id));

		if (av != NULL && av->value == NULL) {
			av->value = g_strdup(var);
			nelem++;
		}
	}

	return nelem;
}

static gint mbb_attr_read(gchar *group, gint obj, struct attrvec *av, gsize cnt,
			  GError **error)
{
	GHashTable *ht = NULL;
	gsize nelem;

	mbb_plock_reader_lock();

	for (; cnt > 0; cnt--, av++) {
		struct attr *attr;

		attr = attr_get(group, av->name);
		if (attr != NULL) {
			if (ht != NULL)
				query_append_format(",%d", attr->id);
			else {
				ht = g_hash_table_new(
					g_direct_hash, g_direct_equal);
				query_format("select * from attr_get(array[%d",
					attr->id);
			}

			g_hash_table_insert(ht, GINT_TO_POINTER(attr->id), av);
		}
	}

	if (ht == NULL) {
		mbb_plock_reader_unlock();
		return 0;
	}

	nelem = db_read_attrs(
		query_append_format("], %d);", obj), ht, error
	);

	mbb_plock_reader_unlock();
	g_hash_table_destroy(ht);

	return nelem;
}

static void mbb_attr_get(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_ATTR_GROUP, XTV_OBJ_NAME);

	struct attr_group *ag;
	GSList *attrs;
	gchar *group;
	gchar *obj;
	gint id;

	MBB_XTV_CALL(&group, &obj);

	if ((ag = attr_group_get(group)) == NULL) final 
		*ans = mbb_xml_msg_error("unknown group");

	mbb_plock_reader_lock();
	mbb_lock_reader_lock();

	id = attr_group_get_obj_id(ag, obj);
	if (id < 0) final {
		mbb_lock_reader_unlock();
		mbb_plock_reader_unlock();

		*ans = mbb_xml_msg_error("unknown object");
	}

	attrs = xml_tag_path_attr_list(tag, "attr", "name");
	if (attrs == NULL)
		*ans = db_attr_get(ag->attr_list, id);
	else {
		GSList *list;

		list = get_attr_list(group, attrs);

		if (list != NULL) {
			*ans = db_attr_get(list, id);
			g_slist_free(list);
		}

		g_slist_free(attrs);
	}

	mbb_lock_reader_unlock();
	mbb_plock_reader_unlock();
}

static XmlTag *db_attr_find(struct attr *attr, gchar *value)
{
	GError *error = NULL;
	MbbDbIter *iter;
	gchar *query;
	XmlTag *tag;

	query = query_format("select * from attr_find(%d, %s);", attr->id, value);

	mbb_lock_reader_lock();

	iter = mbb_db_query_iter(query, &error);
	if (iter == NULL) {
		mbb_lock_reader_unlock();
		return mbb_xml_msg_from_error(error);
	}

	tag = mbb_xml_msg_ok();
	while (mbb_db_iter_next(iter)) {
		gchar *value;
		gchar *name;
		gint id;

		if (! var_conv_int(mbb_db_iter_value(iter, 0), &id))
			continue;

		name = attr_group_get_obj_name(attr->group, id);
		if (name == NULL)
			continue;

		value = mbb_db_iter_value(iter, 1);
		if (value == NULL)
			continue;

		xml_tag_new_child(tag, "obj",
			"name", variant_new_string(name),
			"value", variant_new_string(value)
		);
	}

	mbb_lock_reader_unlock();

	mbb_db_iter_free(iter);

	return tag;
}

static void mbb_attr_find(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_ATTR_GROUP, XTV_ATTR_NAME, XTV_ATTR_VALUE);

	struct attr *attr;
	gchar *group;
	gchar *name;
	gchar *value;

	MBB_XTV_CALL(&group, &name, &value);

	mbb_plock_reader_lock();

	if ((attr = attr_get(group, name)) == NULL) final {
		mbb_plock_reader_unlock();
		*ans = mbb_xml_msg_error("unknown attr");
	}

	*ans = db_attr_find(attr, value);

	mbb_plock_reader_unlock();
}

MBB_FUNC_REGISTER_STRUCT("mbb-attr-list", mbb_attr_list, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-attr-add", mbb_attr_add, MBB_CAP_WHEEL);
MBB_FUNC_REGISTER_STRUCT("mbb-attr-del", mbb_attr_del, MBB_CAP_WHEEL);
MBB_FUNC_REGISTER_STRUCT("mbb-attr-rename", mbb_attr_rename, MBB_CAP_WHEEL);
MBB_FUNC_REGISTER_STRUCT("mbb-attr-set", mbb_attr_set, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-attr-unset", mbb_attr_unset, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-attr-get", mbb_attr_get, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-attr-find", mbb_attr_find, MBB_CAP_ADMIN);

static void load_module(void)
{
	static struct attr_interface ai = {
		.attr_readv = mbb_attr_read
	};

	if (db_attr_group_load() == FALSE)
		return;

	if (db_attr_load() == FALSE)
		return;

	mbb_module_export(&ai);

	mbb_module_add_func(&MBB_FUNC(mbb_attr_list));
	mbb_module_add_func(&MBB_FUNC(mbb_attr_add));
	mbb_module_add_func(&MBB_FUNC(mbb_attr_del));
	mbb_module_add_func(&MBB_FUNC(mbb_attr_rename));
	mbb_module_add_func(&MBB_FUNC(mbb_attr_set));
	mbb_module_add_func(&MBB_FUNC(mbb_attr_unset));
	mbb_module_add_func(&MBB_FUNC(mbb_attr_get));
	mbb_module_add_func(&MBB_FUNC(mbb_attr_find));
}

static void unload_module(void)
{
	attr_free_all();

	if (ht_group != NULL)
		g_hash_table_destroy(ht_group);
}

MBB_DEFINE_MODULE("attributes for mbb objects")
