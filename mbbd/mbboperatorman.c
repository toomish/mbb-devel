/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbdboperator.h"
#include "mbboperator.h"
#include "mbbxmlmsg.h"
#include "mbblock.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbblog.h"
#include "mbbxtv.h"

#include "variant.h"
#include "xmltag.h"
#include "macros.h"

static void gather_operator(MbbOperator *op, XmlTag *tag)
{
	xml_tag_add_child(tag, xml_tag_newc(
		"operator",
		"name", variant_new_string(op->name)
	));
}

static void show_operators(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	*ans = mbb_xml_msg_ok();

	mbb_lock_reader_lock();
	mbb_operator_foreach((GFunc) gather_operator, *ans);
	mbb_lock_reader_unlock();
}

static void add_operator(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	MbbOperator *op;
	GError *error;
	gchar *name;
	gint id;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	if (mbb_operator_get_by_name(name) != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_NAME_EXISTS, name);

	error = NULL;
	id = mbb_db_operator_add(name, &error);
	if (id < 0) final
		*ans = mbb_xml_msg_from_error(error);

	op = mbb_operator_new(id, name);
	mbb_operator_join(op);

	mbb_lock_writer_unlock();

	mbb_log_debug("add operator '%s'", name);
}

static void drop_operator(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	MbbOperator *op;
	GError *error;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	op = mbb_operator_get_by_name(name);
	if (op == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_OPERATOR);

	if (op->links != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_HAS_CHILDS);

	error = NULL;
	if (! mbb_db_operator_drop(op->id, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_operator_remove(op);
	mbb_lock_writer_unlock();

	mbb_log_debug("drop operator '%s'", name);
}

static void operator_mod_name(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE, XTV_NEWNAME_VALUE);

	MbbOperator *op;
	GError *error;
	gchar *newname;
	gchar *name;

	MBB_XTV_CALL(&name, &newname);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	op = mbb_operator_get_by_name(name);
	if (op == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_OPERATOR);

	if (mbb_operator_get_by_name(newname) != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_NAME_EXISTS, newname);

	error = NULL;
	if (! mbb_db_operator_mod_name(op->id, newname, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	if (mbb_operator_mod_name(op, newname) == FALSE) final
		*ans = mbb_xml_msg(MBB_MSG_UNPOSSIBLE);

	mbb_lock_writer_unlock();

	mbb_log_debug("operator '%s' mod name '%s'", name, newname);
}

MBB_INIT_FUNCTIONS_DO
	MBB_FUNC_STRUCT("mbb-show-operators", show_operators, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-add-operator", add_operator, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-drop-operator", drop_operator, MBB_CAP_WHEEL),
	MBB_FUNC_STRUCT("mbb-operator-mod-name", operator_mod_name, MBB_CAP_ADMIN),
MBB_INIT_FUNCTIONS_END

MBB_ON_INIT(MBB_INIT_FUNCTIONS)
