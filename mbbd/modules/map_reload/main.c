/* Copyright (C) 2012 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbinetmap.h"
#include "mbbxmlmsg.h"
#include "mbbmodule.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbblock.h"
#include "mbbunit.h"
#include "mbblog.h"
#include "mbbvar.h"
#include "mbbxtv.h"

#include "varconv.h"
#include "xmltag.h"

static gboolean map_reload_oninit = FALSE;

static gboolean mbb_map_reload_unit(MbbUnit *unit, gpointer data)
{
	MbbInetPoolEntry *pp[2];
	struct map_cross cross;
	XmlTag **ans = data;

	if (mbb_unit_map_rebuild(unit, pp) == FALSE) {
		inet_buf_t buf1, buf2;
		gchar *msg;

		inettoa(buf1, &pp[0]->inet);
		inettoa(buf2, &pp[1]->inet);

		msg = g_strdup_printf("unit '%s' reload map failed: cross %s (%d) and %s (%d)",
			unit->name, buf1, pp[0]->id, buf2, pp[1]->id
		);

		if (ans == NULL)
			mbb_log_self("%s", msg);
		else
			*ans = mbb_xml_msg_error("%s", msg);

		g_free(msg);

		return FALSE;
	}

	mbb_map_del_unit(unit);
	if (mbb_map_add_unit(unit, &cross) == FALSE) {
		MbbInetPoolEntry *entry;
		MbbUnit *foe;
		gchar *msg;

		mbb_map_del_unit(unit);
		mbb_map_auto_glue();

		entry = (MbbInetPoolEntry *) cross.data;
		foe = entry->owner->ptr;

		msg = g_strdup_printf("map add unit '%s' failed: crosses with '%s'",
			unit->name, foe->name
		);

		if (ans == NULL)
			mbb_log_self("%s", msg);
		else
			*ans = mbb_xml_msg_error("%s", msg);

		g_free(msg);

		return FALSE;
	}

	return TRUE;
}

static inline void mbb_map_reload(void)
{
	mbb_lock_writer_lock();
	mbb_map_clear();
	mbb_unit_foreach((GFunc) mbb_map_reload_unit, NULL);
	mbb_lock_writer_unlock();
}

static void map_reload(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans G_GNUC_UNUSED)
{
	mbb_map_reload();
}

static void map_reload_unit(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_UNIT_NAME);

	MbbUnit *unit;
	char *name;

	MBB_XTV_CALL(&name);

	mbb_lock_writer_lock();

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final {
		mbb_lock_writer_unlock();
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);
	}

	mbb_map_reload_unit(unit, ans);

	mbb_lock_writer_unlock();
}

MBB_INIT_FUNCTIONS_DO
	MBB_FUNC_STRUCT("mbb-map-reload", map_reload, MBB_CAP_WHEEL),
	MBB_FUNC_STRUCT("mbb-map-reload-unit", map_reload_unit, MBB_CAP_ADMIN),
MBB_INIT_FUNCTIONS_END

MBB_VAR_DEF(var_def) {
	.op_read = var_str_bool,
	.op_write = var_conv_bool,

	.cap_read = MBB_CAP_ALL,
	.cap_write = MBB_CAP_ROOT
};

static void ready(void)
{
	if (map_reload_oninit) {
		mbb_log_self("do map reload");
		mbb_map_reload();
	}
}

static void load_module(void)
{
	mbb_module_add_base_var("map.reload.oninit", &var_def, &map_reload_oninit);
	mbb_module_add_functions(MBB_INIT_FUNCTIONS_TABLE);
	mbb_module_onready(ready);
}

static void unload_module(void)
{
}

MBB_DEFINE_MODULE("map reload methods")
