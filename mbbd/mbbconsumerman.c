#include "mbbdbconsumer.h"
#include "mbbconsumer.h"
#include "mbbxmlmsg.h"
#include "mbbthread.h"
#include "mbbuser.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbblock.h"
#include "mbbtime.h"
#include "mbblog.h"
#include "mbbxtv.h"

#include "variant.h"
#include "macros.h"
#include "xmltag.h"
#include "re.h"

struct ans_data {
	gboolean show_all;
	XmlTag *tag;
};

static void gather_consumer(MbbConsumer *con, struct ans_data *ad)
{
	XmlTag *xt;

	if (! ad->show_all && con->end != 0)
		return;

	xt = xml_tag_add_child(ad->tag, xml_tag_newc(
		"consumer",
		"id", variant_new_int(con->id),
		"name", variant_new_string(con->name),
		"start", variant_new_long(con->start),
		"end", variant_new_long(con->end)
	));

	if (con->user != NULL) {
		xml_tag_set_attr(xt,
			"user", variant_new_string(con->user->name)
		);
	}
}

static void user_show_consumers(MbbUser *user, Regex re, struct ans_data *ad)
{
	GSList *list;

	for (list = user->childs; list != NULL; list = list->next) {
		MbbConsumer *con = list->data;

		if (re == NULL || re_ismatch(re, con->name))
			gather_consumer(list->data, ad);
	}
}

static void show_consumers(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_USER_NAME_, XTV_REGEX_VALUE_);

	struct ans_data ad = { TRUE, NULL };

	gchar *name = NULL;
	Regex re = NULL;

	MBB_XTV_CALL(&name, &re);

	if (name == NULL) {
		ad.tag = *ans = mbb_xml_msg_ok();

		mbb_lock_reader_lock();
		mbb_consumer_forregex(re, (GFunc) gather_consumer, &ad);
		mbb_lock_reader_unlock();
	} else {
		MbbUser *user;

		mbb_lock_reader_lock();
		user = mbb_user_get_by_name(name);
		if (user == NULL) final {
			*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_USER);
			mbb_lock_reader_unlock();
			re_free(re);
		}

		ad.tag = *ans = mbb_xml_msg_ok();
		user_show_consumers(user, re, &ad);
		mbb_lock_reader_unlock();
	}

	re_free(re);

	if (mbb_session_is_http())
		xml_tag_sort_by_attr(*ans, "consumer", "name");
}

static void self_show_consumers(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	DEFINE_XTV(XTV_REGEX_VALUE_);

	struct ans_data ad = { FALSE, NULL };

	MbbUser *user;
	Regex re = NULL;

	MBB_XTV_CALL(&re);

	mbb_lock_reader_lock();

	on_final { mbb_lock_reader_unlock(); re_free(re); }

	user = mbb_user_get_by_id(mbb_thread_get_uid());
	if (user == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_SELF_NOT_EXISTS);

	ad.tag = *ans = mbb_xml_msg_ok();
	user_show_consumers(user, re, &ad);

	final {
		if (mbb_session_is_http())
			xml_tag_sort_by_attr(*ans, "consumer", "name");
	}
}

static void add_consumer(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	MbbConsumer *con;
	GError *error;
	gchar *name;
	time_t now;
	gint id;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	if (mbb_consumer_get_by_name(name) != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_NAME_EXISTS, name);

	error = NULL;
	time(&now);
	id = mbb_db_consumer_add(name, now, 0, &error);
	if (id < 0) final
		*ans = mbb_xml_msg_from_error(error);

	con = mbb_consumer_new(id, name, now, 0);
	mbb_consumer_join(con);
	mbb_lock_writer_unlock();

	mbb_log_debug("add consumer '%s'", name);
}

static void drop_consumer(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	MbbConsumer *con;
	GError *error;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	con = mbb_consumer_get_by_name(name);
	if (con == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_CONSUMER);

	if (con->units != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_HAS_CHILDS);

	error = NULL;
	if (mbb_db_consumer_drop(con->id, &error) == FALSE) final
		*ans = mbb_xml_msg_from_error(error);

	if (con->user != NULL)
		mbb_user_del_consumer(con->user, con);

	mbb_consumer_remove(con);
	mbb_lock_writer_unlock();

	mbb_log_debug("drop consumer '%s'", name);
}

static void consumer_mod_name(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE, XTV_NEWNAME_VALUE);

	MbbConsumer *con;
	GError *error;
	gchar *newname;
	gchar *name;

	MBB_XTV_CALL(&name, &newname);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	con = mbb_consumer_get_by_name(name);
	if (con == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_CONSUMER);

	if (mbb_consumer_get_by_name(newname) != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_NAME_EXISTS, newname);

	error = NULL;
	if (! mbb_db_consumer_mod_name(con->id, newname, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	if (mbb_consumer_mod_name(con, newname) == FALSE) final
		*ans = mbb_xml_msg(MBB_MSG_UNPOSSIBLE);

	mbb_lock_writer_unlock();

	mbb_log_debug("consumer '%s' mod name '%s'", name, newname);
}

static void consumer_mod_time(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE, XTV_ETIME_START_, XTV_ETIME_END_);

	MbbConsumer *con;
	time_t start, end;
	GError *error;
	gchar *name;

	start = end = VAR_CONV_TIME_UNSET;
	MBB_XTV_CALL(&name, &start, &end);

	if (start == VAR_CONV_TIME_UNSET && end == VAR_CONV_TIME_UNSET) final
		*ans = mbb_xml_msg(MBB_MSG_TIME_MISSED);

	if (start == MBB_TIME_PARENT || end == MBB_TIME_PARENT) final
		*ans = mbb_xml_msg(MBB_MSG_ORPHAN);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	con = mbb_consumer_get_by_name(name);
	if (con == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_CONSUMER);

	exec_if(start = con->start, start < 0);
	exec_if(end = con->end, end < 0);

	if  (mbb_time_becmp(start, end) > 0) final
		*ans = mbb_xml_msg(MBB_MSG_TIME_ORDER);

	if (mbb_consumer_mod_time(con, start, end) == FALSE) final
		*ans = mbb_xml_msg(MBB_MSG_TIME_COLLISION);

	error = NULL;
	if (! mbb_db_consumer_mod_time(con->id, start, end, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_lock_writer_unlock();

	mbb_log_debug("consumer '%s' mod time", name);
}

static void consumer_set_user(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_CONSUMER_NAME, XTV_USER_NAME);

	GError *error = NULL;
	gchar *con_name;
	gchar *user_name;
	MbbConsumer *con;
	MbbUser *user;

	MBB_XTV_CALL(&con_name, &user_name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	con = mbb_consumer_get_by_name(con_name);
	if (con == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_CONSUMER);

	user = mbb_user_get_by_name(user_name);
	if (user == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_USER);

	if (con->user != NULL) final
		*ans = mbb_xml_msg_error("consumer already has user");

	if (! mbb_db_consumer_set_user(con->id, user->id, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_user_add_consumer(user, con);
	con->user = user;

	mbb_lock_writer_unlock();

	mbb_log_debug("consumer '%s' set user '%s'", con_name, user_name);
}

static void consumer_unset_user(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	MbbConsumer *con;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	con = mbb_consumer_get_by_name(name);
	if (con == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_CONSUMER);

	if (con->user != NULL) {
		GError *error = NULL;

		if (! mbb_db_consumer_unset_user(con->id, &error)) final
			*ans = mbb_xml_msg_from_error(error);

		mbb_user_del_consumer(con->user, con);
		con->user = NULL;
	}

	mbb_lock_writer_unlock();

	mbb_log_debug("consumer '%s' unset user", name);
}

MBB_FUNC_REGISTER_STRUCT("mbb-show-consumers", show_consumers, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-self-show-consumers", self_show_consumers, MBB_CAP_CONS);

MBB_FUNC_REGISTER_STRUCT("mbb-add-consumer", add_consumer, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-drop-consumer", drop_consumer, MBB_CAP_WHEEL);

MBB_FUNC_REGISTER_STRUCT("mbb-consumer-mod-name", consumer_mod_name, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-consumer-mod-time", consumer_mod_time, MBB_CAP_ADMIN);

MBB_FUNC_REGISTER_STRUCT("mbb-consumer-set-user", consumer_set_user, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-consumer-unset-user", consumer_unset_user, MBB_CAP_ADMIN);

static void __init init(void)
{
	static struct mbb_init_struct entries[] = {
		MBB_INIT_FUNC_STRUCT(show_consumers),
		MBB_INIT_FUNC_STRUCT(add_consumer),
		MBB_INIT_FUNC_STRUCT(drop_consumer),
		MBB_INIT_FUNC_STRUCT(consumer_mod_name),
		MBB_INIT_FUNC_STRUCT(consumer_mod_time),
		MBB_INIT_FUNC_STRUCT(consumer_set_user),
		MBB_INIT_FUNC_STRUCT(consumer_unset_user),
		MBB_INIT_FUNC_STRUCT(self_show_consumers)
	};

	mbb_init_pushv(entries, NELEM(entries));
}

