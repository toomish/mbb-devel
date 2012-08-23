/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbxmlmsg.h"
#include "mbbthread.h"
#include "mbblock.h"
#include "mbbuser.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbbxtv.h"

#include "variant.h"
#include "xmltag.h"
#include "macros.h"

static void show_methods_common(mbb_cap_t mask, XmlTag **ans)
{
	GSList *meth_list;
	GSList *list;

	meth_list = mbb_func_get_methods(mask);

	*ans = mbb_xml_msg_ok();

	for (list = meth_list; list != NULL; list = list->next) {
		xml_tag_new_child(*ans, "method",
			"name", variant_new_static_string(list->data)
		);
	}

	g_slist_free(meth_list);
}

static void user_show_methods(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_USER_NAME);

	MbbUser *user;
	mbb_cap_t mask;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_reader_lock();
	user = mbb_user_get_by_name(name);
	if (user == NULL) {
		mbb_lock_reader_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_USER);
	} else {
		mask = user->cap_mask;
		mbb_lock_reader_unlock();
		show_methods_common(mask, ans);
	}
}

static void self_show_methods(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	show_methods_common(mbb_thread_get_cap(), ans);
}

MBB_INIT_FUNCTIONS_DO
	MBB_FUNC_STRUCT("mbb-self-show-methods", self_show_methods, MBB_CAP_ALL_DUMMY),
	MBB_FUNC_STRUCT("mbb-user-show-methods", user_show_methods, MBB_CAP_ADMIN),
MBB_INIT_FUNCTIONS_END

MBB_ON_INIT(MBB_INIT_FUNCTIONS)
