/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbxmlmsg.h"

#include <stdarg.h>

#include "variant.h"
#include "macros.h"

static gchar *msg_table[] = {
	[MBB_MSG_OK] = NULL,
	[MBB_MSG_EMPTY] = NULL,
	[MBB_MSG_UNKNOWN_METHOD] = "method %s not found",
	[MBB_MSG_TIME_MISSED] = "missed time values",
	[MBB_MSG_UNKNOWN_INET] = "unknown inet",
	[MBB_MSG_UNKNOWN_UNIT] = "unknown unit",
	[MBB_MSG_PSTART_MORE_END] = "parent start time is larger then new end time",
	[MBB_MSG_PEND_LESS_START] = "parent end time is smaller then new start time",
	[MBB_MSG_INVALID_TIME_ORDER] = "invalid time order",
	[MBB_MSG_XTV_MISSED] = "xml entry %s:%s missed",
	[MBB_MSG_XTV_INVALID] = "xml entry %s:%s has invalid value %s",
	[MBB_MSG_UNIT_FREEZED] = "unit %s is freezed",
	[MBB_MSG_NAME_EXISTS] = "name %s exists",
	[MBB_MSG_UNKNOWN_CONSUMER] = "unknown consumer",
	[MBB_MSG_HAS_CHILDS] = "has childs",
	[MBB_MSG_UNPOSSIBLE] = "unpossible",
	[MBB_MSG_ORPHAN] = "sorry, i am orphan",
	[MBB_MSG_TIME_ORDER] = "invalid time order",
	[MBB_MSG_TIME_COLLISION] = "time collision",
	[MBB_MSG_UNKNOWN_GROUP] = "unknown group",
	[MBB_MSG_UNKNOWN_USER] = "unknown user",
	[MBB_MSG_GROUP_HAS_USER] = "group already has this user",
	[MBB_MSG_GROUP_HASNOT_USER] = "group hasnot this user",
	[MBB_MSG_UNIT_HAS_CONSUMER] = "unit already has consumer",
	[MBB_MSG_CONSUMER_FREEZED] = "consumer %s is freezed",
	[MBB_MSG_SELF_NOT_EXISTS] = "self user not exists",
	[MBB_MSG_INVALID_PASS] = "invalid password",
	[MBB_MSG_NEED_PASS] = "password expected",
	[MBB_MSG_INET_COLLISION] = "inet collision",
	[MBB_MSG_INET_CROSS] = "cross with inet %d (%s)",
	[MBB_MSG_AMBIGUOUS_REQUEST] = "ambiguous request: too many data",
	[MBB_MSG_UNKNOWN_VAR] = "unknown var",
	[MBB_MSG_NOT_CAPABLE] = "capability not permit",
	[MBB_MSG_INVALID_VALUE] = "invalid value",
	[MBB_MSG_VAR_READONLY] = "var is only for read",
	[MBB_MSG_ADDR_EXISTS] = "address %s exists",
	[MBB_MSG_UNKNOWN_GATEWAY] = "unknown gateway",
	[MBB_MSG_UNKNOWN_OPERATOR] = "unknown operator",
	[MBB_MSG_GW_HAS_LINK] = "gateway already has this link",
	[MBB_MSG_UNKNOWN_GWLINK] = "unknown gateway link",
	[MBB_MSG_UNAUTHORIZED] = "unauthorized",
	[MBB_MSG_HEADER_MISSED] = "header %s missed",
	[MBB_MSG_INVALID_HEADER] = "invalid header '%s'",
	[MBB_MSG_VAR_NOT_INIT] = "var '%s' not initialized",
	[MBB_MSG_UNKNOWN_MODULE] = "unknown module",
	[MBB_MSG_UNKNOWN_TASK] = "unknown task",
	[MBB_MSG_TASK_CREATE_FAILED] = "failed to create task",
	[MBB_MSG_TIME_NOPARENT] = "parent time not allowed",
	[MBB_MSG_ROOT_ONLY] = "only root can do this",
	[MBB_MSG_UNKNOWN_SESSION] = "unknown session"
};

static XmlTag *responsev(gchar *result, gchar *fmt, va_list ap)
{
	XmlTag *tag;
	gchar *msg;

	/* msg = g_markup_vprintf_escaped(fmt, ap); */
	msg = g_strdup_vprintf(fmt, ap);

	tag = xml_tag_newc(
		"response",
		"result", variant_new_static_string(result),
		"desc", variant_new_alloc_string(msg)
	);

	return tag;
}

static XmlTag *mbb_xml_msg_response(gchar *result, gchar *fmt, ...)
{
	XmlTag *tag;

	if (fmt == NULL)
		tag = xml_tag_newc(
			"response",
			"result", variant_new_static_string(result)
		);
	else {
		va_list ap;

		va_start(ap, fmt);
		tag = responsev(result, fmt, ap);
		va_end(ap);
	}

	return tag;
}

XmlTag *mbb_xml_msg_ok(void)
{
	return mbb_xml_msg_response("ok", NULL);
}

gboolean mbb_xml_msg_is_ok(XmlTag *tag, gchar **msg)
{
	Variant *var;

	var = xml_tag_get_attr(tag, "result");
	if (var == NULL)
		return TRUE;

	if (! g_strcmp0("ok", variant_get_string(var)))
		return TRUE;

	if (msg != NULL) {
		var = xml_tag_get_attr(tag, "desc");
		if (var != NULL)
			*msg = variant_get_string(var);
	}

	return FALSE;
}

XmlTag *mbb_xml_msg_error(gchar *fmt, ...)
{
	XmlTag *tag;

	if (fmt == NULL)
		tag = mbb_xml_msg_response("error", NULL);
	else {
		va_list ap;

		va_start(ap, fmt);
		tag = responsev("error", fmt, ap);
		va_end(ap);
	}

	return tag;
}

XmlTag *mbb_xml_msg_from_error(GError *error)
{
	XmlTag *tag;

	if (error == NULL)
		return mbb_xml_msg_error("unknown error");

	tag = mbb_xml_msg_error("%s [%d]: %s",
		g_quark_to_string(error->domain), error->code, error->message
	);

	g_error_free(error);

	return tag;
}

XmlTag *mbb_xml_msg(mbb_msg_t msg_no, ...)
{
	XmlTag *tag;
	va_list ap;
	gchar *fmt;

	if (msg_no == MBB_MSG_OK)
		return mbb_xml_msg_ok();

	if (msg_no > NELEM(msg_table))
		return mbb_xml_msg_error(NULL);

	fmt = msg_table[msg_no];
	if (fmt == NULL)
		return mbb_xml_msg_error(NULL);

	va_start(ap, msg_no);
	tag = responsev("error", fmt, ap);
	va_end(ap);

	return tag;
}

