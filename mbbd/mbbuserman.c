/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbxmlmsg.h"
#include "mbbthread.h"
#include "mbbdbuser.h"
#include "mbbgroup.h"
#include "mbbuser.h"
#include "mbbinit.h"
#include "mbblock.h"
#include "mbbfunc.h"
#include "mbblog.h"
#include "mbbxtv.h"

#include "variant.h"
#include "macros.h"
#include "xmltag.h"
#include "re.h"

static void gather_user(struct mbb_user *user, XmlTag *tag)
{
	xml_tag_new_child(tag, "user",
		"id", variant_new_int(user->id),
		"name", variant_new_string(user->name)
	);
}

static void show_users(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	DEFINE_XTV(XTV_REGEX_VALUE_);

	Regex re = NULL;

	MBB_XTV_CALL(&re);

	*ans = mbb_xml_msg_ok();

	mbb_lock_reader_lock();
	mbb_user_forregex(re, (GFunc) gather_user, *ans);
	mbb_lock_reader_unlock();

	re_free(re);
}

static void show_groups(MbbUser *user, XmlTag **ans)
{
	MbbGroup *group;
	GSList *group_list;
	GSList *list;

	*ans = mbb_xml_msg_ok();
	group_list = mbb_user_get_groups(user);
	for (list = group_list; list != NULL; list = list->next) {
		group = mbb_group_get_by_id(GPOINTER_TO_INT(list->data));
		xml_tag_add_child(*ans, xml_tag_newc(
			"group",
			"name", variant_new_string(group->name)
		));
	}

	g_slist_free(group_list);
}


static void user_show_groups(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_USER_NAME);

	MbbUser *user;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_reader_lock();
	user = mbb_user_get_by_name(name);
	if (user == NULL) final {
		mbb_lock_reader_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_USER);
	}

	show_groups(user, ans);
	mbb_lock_reader_unlock();
}

static void self_show_groups(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	MbbUser *user;

	mbb_lock_reader_lock();
	user = mbb_user_get_by_id(mbb_thread_get_uid());
	if (user == NULL) final {
		mbb_lock_reader_unlock();
		*ans = mbb_xml_msg(MBB_MSG_SELF_NOT_EXISTS);
	}

	show_groups(user, ans);
	mbb_lock_reader_unlock();
}

static void add_user(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE, XTV_SECRET_VALUE_);

	MbbUser *user;
	GError *error;
	gchar *secret;
	gchar *name;
	gint id;

	secret = NULL;
	MBB_XTV_CALL(&name, &secret);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	if (mbb_user_get_by_name(name) != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_NAME_EXISTS, name);

	error = NULL;
	id = mbb_db_user_add(name, secret, &error);
	if (id < 0) final
		*ans = mbb_xml_msg_from_error(error);

	user = mbb_user_new(id, name, secret);
	mbb_user_join(user);

	mbb_lock_writer_unlock();

	mbb_log_debug("add user '%s'", name);
}

static void drop_user(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	MbbUser *user;
	GError *error;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	user = mbb_user_get_by_name(name);
	if (user == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_USER);

	if (user->childs != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_HAS_CHILDS);

	error = NULL;
	if (mbb_db_user_drop(user->id, &error) == FALSE) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_user_remove(user);
	mbb_lock_writer_unlock();

	mbb_log_debug("drop user '%s'", name);
}

static void user_mod_name(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE, XTV_NEWNAME_VALUE);

	MbbUser *user;
	GError *error;
	gchar *newname;
	gchar *name;

	MBB_XTV_CALL(&name, &newname);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	user = mbb_user_get_by_name(name);
	if (user == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_USER);

	if (mbb_user_get_by_name(newname) != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_NAME_EXISTS, newname);

	error = NULL;
	if (! mbb_db_user_mod_name(user->id, newname, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	if (! mbb_user_mod_name(user, newname)) final
		*ans = mbb_xml_msg(MBB_MSG_UNPOSSIBLE);

	mbb_lock_writer_unlock();

	mbb_log_debug("user '%s' mod name '%s'", name, newname);
}

static inline gboolean change_pass(MbbUser *user, gchar *pass, GError **error)
{
	if (*pass == '\0')
		pass = NULL;

	if (mbb_db_user_mod_pass(user->id, pass, error) == FALSE)
		return FALSE;

	mbb_user_mod_pass(user, pass);

	return TRUE;
}

static void user_mod_pass(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE, XTV_NEWSECRET_VALUE);

	GError *error = NULL;
	MbbUser *user;
	gchar *newsecret;
	gchar *name;

	MBB_XTV_CALL(&name, &newsecret);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	user = mbb_user_get_by_name(name);
	if (user == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_USER);

	if (change_pass(user, newsecret, &error) == FALSE) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_lock_writer_unlock();

	mbb_log_debug("user '%s' mod pass", name);
}

static void self_mod_pass(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NEWSECRET_VALUE, XTV_SECRET_VALUE_);

	GError *error = NULL;
	MbbUser *user;
	gchar *newpass;
	gchar *pass;

	pass = NULL;
	MBB_XTV_CALL(&newpass, &pass);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	user = mbb_user_get_by_id(mbb_thread_get_uid());
	if (user == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_SELF_NOT_EXISTS);

	if (MBB_CAP_IS_ROOT(mbb_thread_get_cap()) == FALSE) {
		if (pass == NULL) final
			*ans = mbb_xml_msg(MBB_MSG_NEED_PASS);

		if (user->secret != NULL && g_strcmp0(pass, user->secret)) final
			*ans = mbb_xml_msg(MBB_MSG_INVALID_PASS);
	}

	if (change_pass(user, newpass, &error) == FALSE) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_lock_writer_unlock();

	mbb_log_debug("self mod pass");
}

MBB_FUNC_REGISTER_STRUCT("mbb-show-users", show_users, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-user-show-groups", user_show_groups, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-self-show-groups", self_show_groups, MBB_CAP_ALL_DUMMY);

MBB_FUNC_REGISTER_STRUCT("mbb-add-user", add_user, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-user-mod-name", user_mod_name, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-user-mod-pass", user_mod_pass, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-self-mod-pass", self_mod_pass, MBB_CAP_ALL);

MBB_FUNC_REGISTER_STRUCT("mbb-drop-user", drop_user, MBB_CAP_WHEEL);

static void __init init(void)
{
	static struct mbb_init_struct entries[] = {
		MBB_INIT_FUNC_STRUCT(show_users),
		MBB_INIT_FUNC_STRUCT(user_show_groups),
		MBB_INIT_FUNC_STRUCT(self_show_groups),
		MBB_INIT_FUNC_STRUCT(add_user),
		MBB_INIT_FUNC_STRUCT(drop_user),
		MBB_INIT_FUNC_STRUCT(user_mod_name),
		MBB_INIT_FUNC_STRUCT(user_mod_pass),
		MBB_INIT_FUNC_STRUCT(self_mod_pass)
	};

	mbb_init_pushv(entries, NELEM(entries));
}

