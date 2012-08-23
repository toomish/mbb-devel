/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbdbgroup.h"
#include "mbbxmlmsg.h"
#include "mbbthread.h"
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

static void gather_group(struct mbb_group *group, XmlTag *tag)
{
	xml_tag_new_child(tag, "group",
		"id", variant_new_int(group->id),
		"name", variant_new_string(group->name)
	);
}

static void show_groups(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	*ans = mbb_xml_msg_ok();

	mbb_lock_reader_lock();
	mbb_group_foreach((GFunc) gather_group, *ans);
	mbb_lock_reader_unlock();
}

static void group_show_users(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_GROUP_NAME);

	struct mbb_group *group;
	struct mbb_user *user;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_reader_lock();
	group = mbb_group_get_by_name(name);
	if (group == NULL) final {
		mbb_lock_reader_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_GROUP);
	}

	*ans = mbb_xml_msg_ok();

	if (group->users != NULL) {
		GHashTableIter iter;

		g_hash_table_iter_init(&iter, group->users);
		while (g_hash_table_iter_next(&iter, NULL, (gpointer *) &user)) {
			xml_tag_new_child(*ans, "user",
				"name", variant_new_string(user->name)
			);
		}
	}

	mbb_lock_reader_unlock();
}

static void group_mod_name(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE, XTV_NEWNAME_VALUE);

	MbbGroup *group;
	gchar *newname;
	gchar *name;
	GError *error;

	MBB_XTV_CALL(&name, &newname);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	group = mbb_group_get_by_name(name);
	if (group == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_GROUP);

	if (mbb_group_get_by_name(newname) != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_NAME_EXISTS, newname);

	error = NULL;
	if (! mbb_db_group_mod_name(group->id, newname, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	if (! mbb_group_mod_name(group, newname)) final
		*ans = mbb_xml_msg(MBB_MSG_UNPOSSIBLE);

	mbb_lock_writer_unlock();

	mbb_log_debug("group '%s' mod name '%s'", name, newname);
}

static void group_do_user(XmlTag *tag, XmlTag **ans, gboolean add)
{
	DEFINE_XTV(XTV_GROUP_NAME, XTV_USER_NAME);

	gchar *group_name, *user_name;
	MbbGroup *group;
	MbbUser *user;
	GError *error;

	MBB_XTV_CALL(&group_name, &user_name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	group = mbb_group_get_by_name(group_name);
	if (group == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_GROUP);

	user = mbb_user_get_by_name(user_name);
	if (user == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_USER);

	if (group->id <= MBB_CAP_WHEEL_ID) {
		if (MBB_CAP_IS_WHEEL(mbb_thread_get_cap()) == FALSE) final
			*ans = mbb_xml_msg(MBB_MSG_ROOT_ONLY);
	}

	if (mbb_group_has_user(group, user->id) == add) final {
		if (add)
			*ans = mbb_xml_msg(MBB_MSG_GROUP_HAS_USER);
		else
			*ans = mbb_xml_msg(MBB_MSG_GROUP_HASNOT_USER);
	}

	error = NULL;
	if (add) {
		if (! mbb_db_group_add_user(group->id, user->id, &error)) final
			*ans = mbb_xml_msg_from_error(error);

		mbb_group_add_user(group, user);
	} else {
		if (! mbb_db_group_del_user(group->id, user->id, &error)) final
			*ans = mbb_xml_msg_from_error(error);

		mbb_group_del_user(group, user);
	}

	mbb_lock_writer_unlock();

	mbb_log_debug("group '%s' %s user '%s'",
		group_name, add ? "add" : "del", user_name
	);
}

static void group_add_user(XmlTag *tag, XmlTag **ans)
{
	group_do_user(tag, ans, TRUE);
}

static void group_del_user(XmlTag *tag, XmlTag **ans)
{
	group_do_user(tag, ans, FALSE);
}

MBB_INIT_FUNCTIONS_DO
	MBB_FUNC_STRUCT("mbb-show-groups", show_groups, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-group-show-users", group_show_users, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-group-mod-name", group_mod_name, MBB_CAP_WHEEL),
	MBB_FUNC_STRUCT("mbb-group-add-user", group_add_user, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-group-del-user", group_del_user, MBB_CAP_ADMIN),
MBB_INIT_FUNCTIONS_END

MBB_ON_INIT(MBB_INIT_FUNCTIONS)
