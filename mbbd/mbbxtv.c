/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbxmlmsg.h"
#include "mbbxtv.h"

#include <stdarg.h>

#define DEFINE_XTV_ENTRY(var, p, n, f) \
struct xml_tag_var xtv_##var = { \
	.path = p, \
	.name = n, \
	.conv = var_conv_##f \
}

XmlTag *mbb_xtv_call_z(XmlTag *tag, XmlTagVar **xtv, ...)
{
	XmlTagVar *p;
	va_list ap;
	gchar *arg = NULL;
	gint no;

	va_start(ap, xtv);
	no = xml_tag_get_valuesv(tag, xtv, &arg, ap);
	va_end(ap);

	if (no < 0)
		return NULL;

	p = xtv[no << 1];
	if (arg == NULL)
		return mbb_xml_msg(MBB_MSG_XTV_MISSED, p->path, p->name);

	return mbb_xml_msg(MBB_MSG_XTV_INVALID, p->path, p->name, arg);
}

DEFINE_XTV_ENTRY(regex_value, "regex", "value", regex);
DEFINE_XTV_ENTRY(name_value, "name", "value", null);
DEFINE_XTV_ENTRY(newname_value, "newname", "value", null);
DEFINE_XTV_ENTRY(inet_id, "inet_entry", "id", uint);
DEFINE_XTV_ENTRY(time_start, "time", "start", time);
DEFINE_XTV_ENTRY(time_end, "time", "end", time);
DEFINE_XTV_ENTRY(time_period, "time", "period", null);
DEFINE_XTV_ENTRY(time_step, "time", "step", null);
DEFINE_XTV_ENTRY(etime_start, "time", "start", etime);
DEFINE_XTV_ENTRY(etime_end, "time", "end", etime);
DEFINE_XTV_ENTRY(unit_name, "unit", "name", null);
DEFINE_XTV_ENTRY(inet_inet, "inet_entry", "inet", inet);
DEFINE_XTV_ENTRY(inet_exclusive, "inet_entry", "exclusive", bool);
DEFINE_XTV_ENTRY(inet_nice, "inet_entry", "nice", uint);
DEFINE_XTV_ENTRY(inet_inherit, "inet_entry", "inherit", bool);
DEFINE_XTV_ENTRY(group_name, "group", "name", null);
DEFINE_XTV_ENTRY(user_name, "user", "name", null);
DEFINE_XTV_ENTRY(consumer_name, "consumer", "name", null);
DEFINE_XTV_ENTRY(unit_inherit, "unit", "inherit", bool);
DEFINE_XTV_ENTRY(secret_value, "secret", "value", null);
DEFINE_XTV_ENTRY(newsecret_value, "newsecret", "value", null);
DEFINE_XTV_ENTRY(auth_login, NULL, "login", null);
DEFINE_XTV_ENTRY(auth_secret, NULL, "secret", null);
DEFINE_XTV_ENTRY(auth_type, NULL, "type", null);
DEFINE_XTV_ENTRY(time_value, "time", "value", time);
DEFINE_XTV_ENTRY(newnice_value, "newnice", "value", uint);
DEFINE_XTV_ENTRY(var_name, "var", "name", null);
DEFINE_XTV_ENTRY(var_value, "var", "value", null);
DEFINE_XTV_ENTRY(addr_value, "addr", "value", ipv4);
DEFINE_XTV_ENTRY(gwlink_id, "gwlink", "id", uint);
DEFINE_XTV_ENTRY(op_name, "operator", "name", null);
DEFINE_XTV_ENTRY(gw_name, "gateway", "name", null);
DEFINE_XTV_ENTRY(link_value, "link", "value", uint);
DEFINE_XTV_ENTRY(module_name, "module", "name", null);
DEFINE_XTV_ENTRY(task_id, "task", "id", uint);
DEFINE_XTV_ENTRY(net_value, "net", "value", inet);
DEFINE_XTV_ENTRY(attr_group, "attr", "group", null);
DEFINE_XTV_ENTRY(attr_name, "attr", "name", null);
DEFINE_XTV_ENTRY(attr_value, "attr", "value", null);
DEFINE_XTV_ENTRY(key_value, "key", "value", null);
DEFINE_XTV_ENTRY(obj_name, "obj", "name", null);
DEFINE_XTV_ENTRY(session_sid, "session", "sid", uint);

