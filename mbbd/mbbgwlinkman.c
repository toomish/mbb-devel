#include "mbbdbgwlink.h"
#include "mbboperator.h"
#include "mbbsession.h"
#include "mbbgateway.h"
#include "mbbgwlink.h"
#include "mbbxmlmsg.h"
#include "mbbtime.h"
#include "mbblock.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbblog.h"
#include "mbbxtv.h"

#include "variant.h"
#include "macros.h"
#include "xmltag.h"

#include <time.h>

struct link_tag {
	guint link;
	XmlTag *xt;
	XmlTag *tag;
};

gboolean mbb_gwman_get_ipview(void);

static inline gchar *gateway_view(MbbGateway *gw)
{
	gboolean ipview;

	ipview = mbb_gwman_get_ipview();

	return mbb_gateway_get_ename(gw, ipview);
}

static void gather_gwlink(MbbGwLink *gl, XmlTag *tag)
{
	xml_tag_add_child(tag, xml_tag_newc(
		"gwlink",
		"id", variant_new_int(gl->id),
		"operator", variant_new_string(gl->op->name),
		"gateway", variant_new_alloc_string(gateway_view(gl->gw)),
		"link", variant_new_int(gl->link),
		"start", variant_new_long(gl->start),
		"end", variant_new_long(gl->end)
	));
}

static void show_gwlinks(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	*ans = mbb_xml_msg_ok();

	mbb_lock_reader_lock();
	mbb_gwlink_foreach((GFunc) gather_gwlink, *ans);
	mbb_lock_reader_unlock();
}

static void operator_show_gwlinks(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_OP_NAME);

	MbbOperator *op;
	MbbGwLink *gl;
	gchar *name;
	GSList *list;

	MBB_XTV_CALL(&name);

	mbb_lock_reader_lock();

	op = mbb_operator_get_by_name(name);
	if (op == NULL) final {
		mbb_lock_reader_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_OPERATOR);
	}

	*ans = mbb_xml_msg_ok();
	for (list = op->links; list != NULL; list = list->next) {
		gl = (MbbGwLink *) list->data;

		xml_tag_add_child(*ans, xml_tag_newc(
			"gwlink",
			"id", variant_new_int(gl->id),
			"gateway", variant_new_alloc_string(gateway_view(gl->gw)),
			"link", variant_new_int(gl->link),
			"start", variant_new_long(gl->start),
			"end", variant_new_long(gl->end)
		));
	}

	mbb_lock_reader_unlock();
}

static void gather_gwlink_gw(MbbGwLink *gl, struct link_tag *lt)
{
	if (lt->xt == NULL || lt->link != gl->link) {
		lt->link = gl->link;

		xml_tag_add_child(lt->tag, lt->xt = xml_tag_newc(
			"link", "no", variant_new_int(gl->link)
		));
	}

	xml_tag_add_child(lt->xt, xml_tag_newc(
		"gwlink",
		"id", variant_new_int(gl->id),
		"operator", variant_new_string(gl->op->name),
		"start", variant_new_long(gl->start),
		"end", variant_new_long(gl->end)
	));
}

static void gateway_show_gwlinks(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_GW_NAME);

	struct link_tag lt;
	MbbGateway *gw;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_lock_reader_lock();

	gw = mbb_gateway_get_by_ename(name);
	if (gw == NULL) final {
		mbb_lock_reader_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_GATEWAY);
	}

	lt.xt = NULL;
	lt.tag = *ans = mbb_xml_msg_ok();
	mbb_gateway_link_foreach(gw, (GFunc) gather_gwlink_gw, &lt);

	mbb_lock_reader_unlock();
}

static void add_gwlink(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_GW_NAME, XTV_LINK_VALUE, XTV_OP_NAME);

	MbbOperator *op;
	MbbGateway *gw;
	MbbGwLink glink, *gl;
	gchar *opname;
	gchar *gwname;
	GError *error;

	MBB_XTV_CALL(&gwname, &glink.link, &opname);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	gw = mbb_gateway_get_by_ename(gwname);
	if (gw == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_GATEWAY);

	op = mbb_operator_get_by_name(opname);
	if (op == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_OPERATOR);

	time(&glink.start);
	glink.end = 0;

	if (! mbb_gateway_can_add(gw, &glink, TRUE)) final
		*ans = mbb_xml_msg(MBB_MSG_GW_HAS_LINK);

	glink.gw = gw;
	glink.op = op;

	error = NULL;
	glink.id = mbb_db_gwlink_add(&glink, &error);
	if (glink.id < 0) final
		*ans = mbb_xml_msg_from_error(error);

	gl = g_memdup(&glink, sizeof(glink));
	if (! mbb_gateway_add_link(gw, gl, NULL)) final {
		g_free(gl);
		*ans = mbb_xml_msg(MBB_MSG_UNPOSSIBLE);
	}

	mbb_gwlink_join(gl);
	mbb_operator_add_link(op, gl);

	mbb_lock_writer_unlock();

	mbb_log_debug("add link %d for %s/%s", glink.link, gwname, opname);
}

static void drop_gwlink(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_GWLINK_ID);

	MbbGwLink *gl;
	GError *error;
	guint id;

	MBB_XTV_CALL(&id);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	gl = mbb_gwlink_get_by_id(id);
	if (gl == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_GWLINK);

	error = NULL;
	if (! mbb_db_gwlink_drop(gl->id, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_log_debug(
		"drop link %d for %s/%s", gl->link, gl->gw->name, gl->op->name
	);

	mbb_gwlink_remove(gl);

	mbb_lock_writer_unlock();
}

static void gwlink_mod_time(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_GWLINK_ID, XTV_ETIME_START_, XTV_ETIME_END_);

	MbbGwLink *gl;
	GError *error;
	time_t start, end;
	guint id;

	start = end = VAR_CONV_TIME_UNSET;
	MBB_XTV_CALL(&id, &start, &end);

	if (start == VAR_CONV_TIME_UNSET && end == VAR_CONV_TIME_UNSET) final
		*ans = mbb_xml_msg(MBB_MSG_TIME_MISSED);

	if (start == VAR_CONV_TIME_PARENT || end == VAR_CONV_TIME_PARENT) final
		*ans = mbb_xml_msg(MBB_MSG_ORPHAN);

	mbb_lock_writer_lock();

	on_final { mbb_lock_writer_unlock(); }

	gl = mbb_gwlink_get_by_id(id);
	if (gl == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_GWLINK);

	exec_if(start = gl->start, start < 0);
	exec_if(end = gl->end, end < 0);

	if (mbb_time_becmp(start, end) > 0) final
		*ans = mbb_xml_msg(MBB_MSG_INVALID_TIME_ORDER);

	SWAP(start, gl->start);
	SWAP(end, gl->end);

	if (! mbb_gateway_reorder_link(gl->gw, gl)) {
		SWAP(start, gl->start);
		SWAP(end, gl->end);

		final *ans = mbb_xml_msg(MBB_MSG_TIME_COLLISION);
	}

	error = NULL;
	if (! mbb_db_gwlink_mod_time(gl->id, gl->start, gl->end, &error)) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_log_debug("link %d for %s/%s mod time",
		gl->link, gl->gw->name, gl->op->name
	);

	mbb_lock_writer_unlock();
}

MBB_FUNC_REGISTER_STRUCT("mbb-show-gwlinks", show_gwlinks, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-operator-show-gwlinks", operator_show_gwlinks, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-gateway-show-gwlinks", gateway_show_gwlinks, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-add-gwlink", add_gwlink, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-drop-gwlink", drop_gwlink, MBB_CAP_WHEEL);
MBB_FUNC_REGISTER_STRUCT("mbb-gwlink-mod-time", gwlink_mod_time, MBB_CAP_ADMIN);

static void __init init(void)
{
	static struct mbb_init_struct entries[] = {
		MBB_INIT_FUNC_STRUCT(show_gwlinks),
		MBB_INIT_FUNC_STRUCT(operator_show_gwlinks),
		MBB_INIT_FUNC_STRUCT(gateway_show_gwlinks),
		MBB_INIT_FUNC_STRUCT(add_gwlink),
		MBB_INIT_FUNC_STRUCT(drop_gwlink),
		MBB_INIT_FUNC_STRUCT(gwlink_mod_time)
	};

	mbb_init_pushv(entries, NELEM(entries));
}

