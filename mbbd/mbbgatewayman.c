#include "mbbdbgateway.h"
#include "mbbgateway.h"
#include "mbbxmlmsg.h"
#include "mbblock.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbblog.h"
#include "mbbxtv.h"
#include "mbbvar.h"

#include "variant.h"
#include "macros.h"
#include "xmltag.h"

static struct mbb_var *gwiv_var = NULL;

gboolean mbb_gwman_get_ipview(void)
{
	return *(gboolean *) mbb_session_var_get_data(gwiv_var);
}

static void gather_gateway(MbbGateway *gw, XmlTag *tag)
{
	xml_tag_add_child(tag, xml_tag_newc(
		"gateway",
		"name", variant_new_string(gw->name),
		"addr", variant_new_alloc_string(ipv4toa(NULL, gw->addr))
	));
}

static void show_gateways(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	*ans = mbb_xml_msg_ok();

	mbb_lock_reader_lock();
	mbb_gateway_foreach((GFunc) gather_gateway, *ans);
	mbb_lock_reader_unlock();
}

static void add_gateway(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE, XTV_ADDR_VALUE);

	MbbGateway *gw;
	GError *error;
	gchar *name;
	ipv4_t addr;
	gint id;

	MBB_XTV_CALL(&name, &addr);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	if (mbb_gateway_get_by_name(name) != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_NAME_EXISTS, name);

	if (mbb_gateway_get_by_addr(addr) != NULL) final {
		ipv4_buf_t buf;
		*ans = mbb_xml_msg(MBB_MSG_ADDR_EXISTS, ipv4toa(buf, addr));
	}

	error = NULL;
	id = mbb_db_gateway_add(name, addr, &error);
	if (id < 0) final
		*ans = mbb_xml_msg_from_error(error);

	gw = mbb_gateway_new(id, name, addr);
	mbb_gateway_join(gw);

	mbb_lock_writer_unlock();

	/* debug log */
	ipv4_buf_t buf;
	mbb_log_debug("add gateway '%s' %s", name, ipv4toa(buf, addr));
}

static void drop_gateway(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	MbbGateway *gw;
	GError *error;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	gw = mbb_gateway_get_by_name(name);
	if (gw == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_GATEWAY);

	if (mbb_gateway_nlink(gw)) final
		*ans = mbb_xml_msg(MBB_MSG_HAS_CHILDS);

	error = NULL;
	if (! mbb_db_gateway_drop(gw->id, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_gateway_remove(gw);
	mbb_lock_writer_unlock();

	mbb_log_debug("drop gateway '%s'", name);
}

static void gateway_mod_name(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE, XTV_NEWNAME_VALUE);

	MbbGateway *gw;
	GError *error;
	gchar *newname;
	gchar *name;

	MBB_XTV_CALL(&name, &newname);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	gw = mbb_gateway_get_by_name(name);
	if (gw == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_GATEWAY);

	if (mbb_gateway_get_by_name(newname) != NULL) final
		*ans = mbb_xml_msg(MBB_MSG_NAME_EXISTS, newname);

	error = NULL;
	if (! mbb_db_gateway_mod_name(gw->id, newname, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	if (! mbb_gateway_mod_name(gw, newname)) final
		*ans = mbb_xml_msg(MBB_MSG_UNPOSSIBLE);

	mbb_lock_writer_unlock();

	mbb_log_debug("gateway '%s' mod name '%s'", name, newname);
}

static gboolean gateway_ipview__ = FALSE;

MBB_VAR_DEF(gwiv_def) {
	.op_read = var_str_bool,
	.op_write = var_conv_bool,
	.cap_read = MBB_CAP_ALL,
	.cap_write = MBB_CAP_ROOT
};

MBB_VAR_DEF(ss_gwiv_def) {
	.op_read = var_str_bool,
	.op_write = var_conv_bool,
	.cap_read = MBB_CAP_ALL,
	.cap_write = MBB_CAP_ALL
};

MBB_SESSION_VAR_DEF(ss_gwiv) {
	.op_new = g_ptr_booldup,
	.op_free = g_free,
	.data = &gateway_ipview__
};

MBB_FUNC_REGISTER_STRUCT("mbb-show-gateways", show_gateways, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-add-gateway", add_gateway, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-drop-gateway", drop_gateway, MBB_CAP_WHEEL);
MBB_FUNC_REGISTER_STRUCT("mbb-gateway-mod-name", gateway_mod_name, MBB_CAP_ADMIN);

static void init_vars(void)
{
	mbb_base_var_register("gateway.ipview", &gwiv_def, &gateway_ipview__);

	gwiv_var = mbb_session_var_register(
		SS_("gateway.ipview"), &ss_gwiv_def, &ss_gwiv
	);
}

static void __init init(void)
{
	static struct mbb_init_struct entries[] = {
		MBB_INIT_VARS,

		MBB_INIT_FUNC_STRUCT(show_gateways),
		MBB_INIT_FUNC_STRUCT(add_gateway),
		MBB_INIT_FUNC_STRUCT(drop_gateway),
		MBB_INIT_FUNC_STRUCT(gateway_mod_name)
	};

	mbb_init_pushv(entries, NELEM(entries));
}

