/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_XML_TAG_VAR_H
#define MBB_XML_TAG_VAR_H

#include "varconv.h"
#include "xmltag.h"

#define XTVAR_NAME xtvar__

XmlTag *mbb_xtv_call_z(XmlTag *tag, XmlTagVar **xtv, ...);

#define MBB_XTV_CALL(...) \
	*ans = mbb_xtv_call(tag, XTVAR_NAME, __VA_ARGS__); \
	if (*ans != NULL) return;

#define XTV_TRUE GINT_TO_POINTER(TRUE)
#define XTV_FALSE GINT_TO_POINTER(FALSE)

#define mbb_xtv_call(tag, xtv, ...) mbb_xtv_call_z(tag, xtv, __VA_ARGS__, NULL)

#define DEFINE_XTVN(var, ...) XmlTagVar *var[] = { __VA_ARGS__ }
#define DEFINE_XTV(...) DEFINE_XTVN(XTVAR_NAME, __VA_ARGS__)

#define DEFINE_EXTERN(var) extern XmlTagVar xtv_##var

#define XTV_NAME_VALUE &xtv_name_value, XTV_FALSE
#define XTV_NAME_VALUE_ &xtv_name_value, XTV_TRUE

#define XTV_REGEX_VALUE_ &xtv_regex_value, XTV_TRUE
#define XTV_INET_ID &xtv_inet_id, XTV_FALSE
#define XTV_INET_ID_ &xtv_inet_id, XTV_TRUE
#define XTV_TIME_START &xtv_time_start, XTV_FALSE
#define XTV_TIME_START_ &xtv_time_start, XTV_TRUE
#define XTV_TIME_END &xtv_time_end, XTV_FALSE
#define XTV_TIME_END_ &xtv_time_end, XTV_TRUE
#define XTV_TIME_PERIOD_ &xtv_time_period, XTV_TRUE
#define XTV_TIME_STEP_ &xtv_time_step, XTV_TRUE
#define XTV_ETIME_START_ &xtv_etime_start, XTV_TRUE
#define XTV_ETIME_END_ &xtv_etime_end, XTV_TRUE
#define XTV_UNIT_NAME &xtv_unit_name, XTV_FALSE
#define XTV_UNIT_NAME_ &xtv_unit_name, XTV_TRUE
#define XTV_INET_INET &xtv_inet_inet, XTV_FALSE
#define XTV_INET_EXCLUSIVE_ &xtv_inet_exclusive, XTV_TRUE
#define XTV_INET_NICE_ &xtv_inet_nice, XTV_TRUE
#define XTV_INET_INHERIT_ &xtv_inet_inherit, XTV_TRUE
#define XTV_NEWNAME_VALUE &xtv_newname_value, XTV_FALSE
#define XTV_GROUP_NAME &xtv_group_name, XTV_FALSE
#define XTV_USER_NAME &xtv_user_name, XTV_FALSE
#define XTV_USER_NAME_ &xtv_user_name, XTV_TRUE
#define XTV_CONSUMER_NAME &xtv_consumer_name, XTV_FALSE
#define XTV_CONSUMER_NAME_ &xtv_consumer_name, XTV_TRUE
#define XTV_UNIT_INHERIT_ &xtv_unit_inherit, XTV_TRUE
#define XTV_SECRET_VALUE_ &xtv_secret_value, XTV_TRUE
#define XTV_NEWSECRET_VALUE &xtv_newsecret_value, XTV_FALSE
#define XTV_AUTH_LOGIN_ &xtv_auth_login, XTV_TRUE
#define XTV_AUTH_SECRET_ &xtv_auth_secret, XTV_TRUE
#define XTV_AUTH_TYPE_ &xtv_auth_type, XTV_TRUE
#define XTV_TIME_VALUE_ &xtv_time_value, XTV_TRUE
#define XTV_NEWNICE_VALUE &xtv_newnice_value, XTV_FALSE
#define XTV_VAR_NAME &xtv_var_name, XTV_FALSE
#define XTV_VAR_VALUE &xtv_var_value, XTV_FALSE
#define XTV_VAR_VALUE_ &xtv_var_value, XTV_TRUE
#define XTV_ADDR_VALUE &xtv_addr_value, XTV_FALSE
#define XTV_GWLINK_ID &xtv_gwlink_id, XTV_FALSE
#define XTV_OP_NAME &xtv_op_name, XTV_FALSE
#define XTV_GW_NAME &xtv_gw_name, XTV_FALSE
#define XTV_LINK_VALUE &xtv_link_value, XTV_FALSE
#define XTV_MODULE_NAME &xtv_module_name, XTV_FALSE
#define XTV_TASK_ID &xtv_task_id, XTV_FALSE
#define XTV_NET_VALUE &xtv_net_value, XTV_FALSE
#define XTV_ATTR_GROUP &xtv_attr_group, XTV_FALSE
#define XTV_ATTR_NAME &xtv_attr_name, XTV_FALSE
#define XTV_ATTR_VALUE &xtv_attr_value, XTV_FALSE
#define XTV_KEY_VALUE &xtv_key_value, XTV_FALSE
#define XTV_OBJ_NAME &xtv_obj_name, XTV_FALSE
#define XTV_SESSION_SID_ &xtv_session_sid, XTV_TRUE

DEFINE_EXTERN(regex_value);
DEFINE_EXTERN(name_value);
DEFINE_EXTERN(newname_value);
DEFINE_EXTERN(inet_id);
DEFINE_EXTERN(time_start);
DEFINE_EXTERN(time_end);
DEFINE_EXTERN(time_period);
DEFINE_EXTERN(time_step);
DEFINE_EXTERN(etime_start);
DEFINE_EXTERN(etime_end);
DEFINE_EXTERN(unit_name);
DEFINE_EXTERN(inet_inet);
DEFINE_EXTERN(inet_exclusive);
DEFINE_EXTERN(inet_nice);
DEFINE_EXTERN(inet_inherit);
DEFINE_EXTERN(group_name);
DEFINE_EXTERN(user_name);
DEFINE_EXTERN(consumer_name);
DEFINE_EXTERN(unit_inherit);
DEFINE_EXTERN(secret_value);
DEFINE_EXTERN(newsecret_value);
DEFINE_EXTERN(auth_login);
DEFINE_EXTERN(auth_secret);
DEFINE_EXTERN(auth_type);
DEFINE_EXTERN(time_value);
DEFINE_EXTERN(newnice_value);
DEFINE_EXTERN(var_name);
DEFINE_EXTERN(var_value);
DEFINE_EXTERN(addr_value);
DEFINE_EXTERN(gwlink_id);
DEFINE_EXTERN(op_name);
DEFINE_EXTERN(gw_name);
DEFINE_EXTERN(link_value);
DEFINE_EXTERN(module_name);
DEFINE_EXTERN(task_id);
DEFINE_EXTERN(net_value);
DEFINE_EXTERN(attr_group);
DEFINE_EXTERN(attr_name);
DEFINE_EXTERN(attr_value);
DEFINE_EXTERN(key_value);
DEFINE_EXTERN(obj_name);
DEFINE_EXTERN(session_sid);

#endif
