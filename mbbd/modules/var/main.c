/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbvar.h"
#include "debug.h"

static gchar *msg;

static gpointer var_new(gpointer ptr)
{
	gpointer tmp = g_ptr_strdup(ptr);

	msg_warn("new %p (%s)", tmp, *(gchar **) tmp);

	return tmp;
}

static void var_free(gpointer ptr)
{
	msg_warn("free %p (%s)", ptr, *(gchar **) ptr);
	g_ptr_strfree(ptr);
}

MBB_VAR_DEF(var_def) {
	.op_read = var_str_str,
	.op_write = var_conv_dup,
	.cap_read = MBB_CAP_ALL,
	.cap_write = MBB_CAP_ALL
};

MBB_SESSION_VAR_DEF(ss_var) {
	.op_new = var_new,
	.op_free = var_free,
	.data = &msg
};

static void load_module(void)
{
	msg_warn("load var module");

	msg = g_strdup("value");
	mbb_module_add_session_var(SS_("var.test"), &var_def, &ss_var);
}

static void unload_module(void)
{
	msg_warn("unload var module");
	g_free(msg);
}

MBB_DEFINE_MODULE("var test module")
