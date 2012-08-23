/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbdbunit.h"
#include "mbbxmlmsg.h"
#include "mbbthread.h"
#include "mbbuser.h"
#include "mbbunit.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbblock.h"
#include "mbbtime.h"
#include "mbblog.h"
#include "mbbxtv.h"

#include "variant.h"
#include "macros.h"
#include "xmltag.h"

struct ans_data {
	gboolean show_all;
	XmlTag *tag;
};

static void gather_unit(MbbUnit *unit, struct ans_data *ad)
{
	XmlTag *xt;

	if (! ad->show_all && mbb_unit_get_end(unit) != 0)
			return;

	xt = xml_tag_new_child(ad->tag, "unit",
		"id", variant_new_int(unit->id),
		"name", variant_new_string(unit->name),
		"start", variant_new_long(mbb_unit_get_start(unit)),
		"end", variant_new_long(mbb_unit_get_end(unit))
	);

	if (unit->con != NULL)
		xml_tag_set_attr(xt,
			"consumer", variant_new_string(unit->con->name)
		);

	if (unit->start < 0)
		xml_tag_set_attr(xt,
			"parent_start", variant_new_static_string("true")
		);

	if (unit->end < 0)
		xml_tag_set_attr(xt,
			"parent_end", variant_new_static_string("true")
		);
}

static void consumer_show_units(MbbConsumer *con, Regex re, struct ans_data *ad)
{
	GSList *list;

	for (list = con->units; list != NULL; list = list->next) {
		MbbUnit *unit = list->data;

		if (re == NULL || re_ismatch(re, unit->name))
			gather_unit(list->data, ad);
	}
}

static void user_show_units(MbbUser *user, Regex re, struct ans_data *ad)
{
	GSList *list;

	for (list = user->childs; list != NULL; list = list->next)
		consumer_show_units(list->data, re, ad);
}

static void show_units(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_USER_NAME_, XTV_CONSUMER_NAME_, XTV_REGEX_VALUE_);

	struct ans_data ad = { TRUE, NULL };

	gchar *user_name = NULL;
	gchar *con_name = NULL;
	Regex re = NULL;

	MBB_XTV_CALL(&user_name, &con_name, &re);

	if (user_name != NULL && con_name != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_AMBIGUOUS_REQUEST);

	mbb_lock_reader_lock();

	on_final { mbb_lock_reader_unlock(); re_free(re); }

	if (user_name != NULL) {
		MbbUser *user;

		user = mbb_user_get_by_name(user_name);
		if (user == NULL) final
			*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_USER);

		ad.tag = *ans = mbb_xml_msg_ok();
		user_show_units(user, re, &ad);
	} else if (con_name != NULL) {
		MbbConsumer *con;

		con = mbb_consumer_get_by_name(con_name);
		if (con == NULL) final
			*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_CONSUMER);

		ad.tag = *ans = mbb_xml_msg_ok();
		consumer_show_units(con, re, &ad);
	} else {
		ad.tag = *ans = mbb_xml_msg_ok();
		mbb_unit_forregex(re, (GFunc) gather_unit, &ad);
	}

	final {
		if (mbb_session_is_http())
			xml_tag_sort_by_attr(*ans, "unit", "name");
	}
}

static void self_show_units(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_CONSUMER_NAME_, XTV_REGEX_VALUE_);

	struct ans_data ad = { FALSE , NULL };

	gchar *name = NULL;
	MbbUser *user;
	Regex re = NULL;

	MBB_XTV_CALL(&name, &re);

	mbb_lock_reader_lock();

	on_final { mbb_lock_reader_unlock(); re_free(re); }

	user = mbb_user_get_by_id(mbb_thread_get_uid());
	if (user == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_SELF_NOT_EXISTS);

	if (name == NULL) {
		ad.tag = *ans = mbb_xml_msg_ok();
		user_show_units(user, re, &ad);
	} else {
		MbbConsumer *con;

		con = mbb_consumer_get_by_name(name);
		if (con == NULL || con->user != user) final
			*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_CONSUMER);

		ad.tag = *ans = mbb_xml_msg_ok();
		consumer_show_units(con, re, &ad);
	}

	final {
		if (mbb_session_is_http())
			xml_tag_sort_by_attr(*ans, "unit", "name");
	}
}

static void unit_show_self(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_UNIT_NAME);

	struct ans_data ad = { TRUE, NULL };

	MbbUnit *unit;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_reader_lock();

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final {
		mbb_lock_reader_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);
	}

	ad.tag = *ans = mbb_xml_msg_ok();
	gather_unit(unit, &ad);

	mbb_lock_reader_unlock();
}

static gboolean unit_create(gchar *name, MbbConsumer *con, gboolean inherit,
			    GError **error)
{
	time_t start, end;
	MbbUnit *unit;
	gint con_id;
	gint id;

	if (inherit && con != NULL)
		start = -1;
	else
		time(&start);

	if (con != 0)
		end = -1;
	else
		end = 0;

	con_id = con == NULL ? -1 : con->id;
	if ((id = mbb_db_unit_add(name, con_id, start, end, error)) < 0)
		return FALSE;

	unit = mbb_unit_new(id, name, start, end);

	if (con != NULL) {
		unit->con = con;
		mbb_consumer_add_unit(con, unit);
	}

	mbb_unit_join(unit);

	return TRUE;
}
	
static void add_unit(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	GError *error = NULL;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	if (mbb_unit_get_by_name(name) != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_NAME_EXISTS, name);

	if (unit_create(name, NULL, FALSE, &error) == FALSE) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_lock_writer_unlock();

	mbb_log_debug("add unit '%s'", name);
}

static void drop_unit(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	MbbUnit *unit;
	GError *error;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);

	if (separator_count(&unit->sep)) final
		*ans = mbb_xml_msg(MBB_MSG_HAS_CHILDS);

	error = NULL;
	if (! mbb_db_unit_drop(unit->id, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_unit_remove(unit);
	mbb_lock_writer_unlock();

	mbb_log_debug("drop unit '%s'", name);
}

static void unit_mod_name(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE, XTV_NEWNAME_VALUE);

	MbbUnit *unit;
	GError *error;
	gchar *newname;
	gchar *name;

	MBB_XTV_CALL(&name, &newname);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);

	if (mbb_unit_get_by_name(newname) != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_NAME_EXISTS, newname);

	error = NULL;
	if (! mbb_db_unit_mod_name(unit->id, newname, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	if (! mbb_unit_mod_name(unit, newname)) final
		*ans = mbb_xml_msg(MBB_MSG_UNPOSSIBLE);

	mbb_lock_writer_unlock();

	mbb_log_debug("unit '%s' mod name '%s'", name, newname);
}

static void unit_mod_time(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE, XTV_ETIME_START_, XTV_ETIME_END_);

	MbbUnit *unit;
	GError *error;
	time_t start, end;
	gchar *name;

	start = end = VAR_CONV_TIME_UNSET;
	MBB_XTV_CALL(&name, &start, &end);

	if (start == VAR_CONV_TIME_UNSET && end == VAR_CONV_TIME_UNSET) final
		*ans = mbb_xml_msg(MBB_MSG_TIME_MISSED);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);

	if (unit->con == NULL && (start == MBB_TIME_PARENT || end == MBB_TIME_PARENT))
		final *ans = mbb_xml_msg(MBB_MSG_ORPHAN);

	exec_if(start = unit->start, start == MBB_TIME_UNSET);
	exec_if(end = unit->end, end == MBB_TIME_UNSET);

	if (unit->con != NULL)
		*ans = mbb_time_test_order(
			start, end, unit->con->start, unit->con->end
		);
	else if (mbb_time_becmp(start, end) > 0)
		*ans = mbb_xml_msg(MBB_MSG_INVALID_TIME_ORDER);

	if (*ans != NULL)
		final;

	if (! mbb_unit_mod_time(unit, start, end)) final
		*ans = mbb_xml_msg(MBB_MSG_TIME_COLLISION);

	error = NULL;
	if (! mbb_db_unit_mod_time(unit->id, start, end, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_lock_writer_unlock();

	mbb_log_debug("unit '%s' mod time", name);
}

static void unit_set_consumer(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_UNIT_NAME, XTV_CONSUMER_NAME);

	gchar *unit_name, *con_name;
	MbbConsumer *con;
	MbbUnit *unit;
	GError *error;

	MBB_XTV_CALL(&unit_name, &con_name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	unit = mbb_unit_get_by_name(unit_name);
	if (unit == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);

	con = mbb_consumer_get_by_name(con_name);
	if (con == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_CONSUMER);

	if (unit->con != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNIT_HAS_CONSUMER);

	error = NULL;
	if (! mbb_db_unit_set_consumer(unit->id, con->id, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_unit_set_consumer(unit, con);
	mbb_consumer_add_unit(con, unit);

	mbb_lock_writer_unlock();

	mbb_log_debug("unit '%s' set consumer '%s'", unit_name, con_name);
}

static void unit_unset_consumer(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	MbbUnit *unit;
	GError *error;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);

	error = NULL;
	if (unit->con != NULL) {
		mbb_consumer_del_unit(unit->con, unit);

		if (mbb_unit_unset_consumer(unit) == FALSE) {
			mbb_unit_inherit_time(unit);
			mbb_unit_unset_consumer(unit);

			mbb_db_unit_mod_time(unit->id, unit->start, unit->end, &error);
		}

		if (error == NULL)
			mbb_db_unit_unset_consumer(unit->id, &error);

		if (error != NULL) final
			*ans = mbb_xml_msg_from_error(error);
	}

	mbb_lock_writer_unlock();

	mbb_log_debug("unit '%s' unset consumer", name);
}

static void consumer_add_unit(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_CONSUMER_NAME, XTV_UNIT_NAME, XTV_UNIT_INHERIT_);

	MbbConsumer *con;
	gboolean inherit;
	gchar *unit_name;
	gchar *con_name;
	GError *error;

	inherit = FALSE;
	MBB_XTV_CALL(&con_name, &unit_name, &inherit);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	con = mbb_consumer_get_by_name(con_name);
	if (con == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_CONSUMER);

	if (inherit == FALSE && con->end != 0) final
		*ans = mbb_xml_msg(MBB_MSG_CONSUMER_FREEZED, con_name);

	if (mbb_unit_get_by_name(unit_name) != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_NAME_EXISTS, unit_name);

	error = NULL;
	if (unit_create(unit_name, con, inherit, &error) == FALSE) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_lock_writer_unlock();

	mbb_log_debug("consumer '%s' add unit '%s'", con_name, unit_name);
}

static void gather_mapped_unit(MbbUnit *unit, struct ans_data *ad)
{
	if (mbb_unit_mapped(unit))
		gather_unit(unit, ad);
}

static void show_mapped_units(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	struct ans_data ad = {
		TRUE, *ans = mbb_xml_msg_ok()
	};

	mbb_lock_reader_lock();
	mbb_unit_foreach((GFunc) gather_mapped_unit, &ad);
	mbb_lock_reader_unlock();
}

static void gather_nomapped_units(MbbUnit *unit, struct ans_data *ad)
{
	if (! mbb_unit_mapped(unit))
		gather_unit(unit, ad);
}

static void show_nomapped_units(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	struct ans_data ad = {
		TRUE, *ans = mbb_xml_msg_ok()
	};

	mbb_lock_reader_lock();
	mbb_unit_foreach((GFunc) gather_nomapped_units, &ad);
	mbb_lock_reader_unlock();
}

static void show_local_units(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_REGEX_VALUE_);

	Regex re = NULL;

	MBB_XTV_CALL(&re);

	*ans = mbb_xml_msg_ok();

	mbb_lock_reader_lock();
	mbb_unit_local_forregex(re, (GFunc) gather_unit, *ans);
	mbb_lock_reader_unlock();
}

static void unit_local_do(XmlTag *tag, XmlTag **ans, gboolean local)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	GError *error = NULL;
	MbbUnit *unit;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);

	if (unit->local == local)
		final;

	if (! mbb_db_unit_mod_local(unit->id, local, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	if (local)
		mbb_unit_local_add(unit);
	else
		mbb_unit_local_del(unit);

	mbb_lock_writer_unlock();
}

static void unit_local_add(XmlTag *tag, XmlTag **ans)
{
	unit_local_do(tag, ans, TRUE);
}

static void unit_local_del(XmlTag *tag, XmlTag **ans)
{
	unit_local_do(tag, ans, FALSE);
}

MBB_INIT_FUNCTIONS_DO
	MBB_FUNC_STRUCT("mbb-show-units", show_units, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-unit-show-self", unit_show_self, MBB_CAP_ADMIN),

	MBB_FUNC_STRUCT("mbb-add-unit", add_unit, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-drop-unit", drop_unit, MBB_CAP_WHEEL),

	MBB_FUNC_STRUCT("mbb-unit-mod-name", unit_mod_name, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-unit-mod-time", unit_mod_time, MBB_CAP_ADMIN),

	MBB_FUNC_STRUCT("mbb-unit-set-consumer", unit_set_consumer, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-unit-unset-consumer", unit_unset_consumer, MBB_CAP_ADMIN),

	MBB_FUNC_STRUCT("mbb-consumer-add-unit", consumer_add_unit, MBB_CAP_ADMIN),

	MBB_FUNC_STRUCT("mbb-show-mapped-units", show_mapped_units, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-show-nomapped-units", show_nomapped_units, MBB_CAP_ADMIN),

	MBB_FUNC_STRUCT("mbb-show-local-units", show_local_units, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-unit-local-add", unit_local_add, MBB_CAP_WHEEL),
	MBB_FUNC_STRUCT("mbb-unit-local-del", unit_local_del, MBB_CAP_WHEEL),

	MBB_FUNC_STRUCT("mbb-self-show-units", self_show_units, MBB_CAP_CONS),
MBB_INIT_FUNCTIONS_END

MBB_ON_INIT(MBB_INIT_FUNCTIONS)
