/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <time.h>

#include "mbbxmlmsg.h"
#include "mbbtime.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbbvar.h"
#include "mbblog.h"

#include "varconv.h"
#include "macros.h"
#include "xmltag.h"

#include "pathtree.h"
#include "unitstat.h"
#include "calcstat.h"

gpointer *stat_lib = NULL;

static struct mbb_var *nf_data_var = NULL;
static struct mbb_var *nf_store_var = NULL;

gchar *nf_get_store_dir(gchar *name)
{
	time_buf_t tbuf;
	gchar **dir;
	gchar *str;

	timetoa(tbuf, time(NULL));

	dir = mbb_session_var_get_data(nf_store_var);
	str = g_strdup_printf("%s%s.%s", *dir, name, tbuf);

	return str;
}

GString *nf_get_data_dir(void)
{
	GString *string = NULL;
	gchar **dir;

	dir = mbb_session_var_get_data(nf_data_var);

	if (*dir != NULL)
		string = g_string_new(*dir);

	return string;
}

static void netflow_file_list(XmlTag *tag, XmlTag **ans)
{
	struct path_tree pt;
	GSList *list;

	*ans = mbb_xml_msg_ok();

	list = xml_tag_path_attr_list(tag, "glob", "value");
	if (list == NULL)
		return;

	list = g_slist_reverse(list);
	if (path_tree_walk(&pt, list)) {
		gchar *str;

		while ((str = path_tree_next(&pt)) != NULL)
			xml_tag_new_child(*ans, "file", "path",
				variant_new_string(str)
			);

		path_tree_free(&pt);
	}

	g_slist_free(list);
}

MBB_INIT_FUNCTIONS_DO
	MBB_FUNC_STRUCT("mbb-netflow-file-list", netflow_file_list, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-netflow-grep-unit", unit_stat, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-netflow-stat-update", update_stat, MBB_CAP_WHEEL),
	MBB_FUNC_STRUCT("mbb-netflow-stat-plain", plain_stat, MBB_CAP_WHEEL),
	MBB_FUNC_STRUCT("mbb-netflow-stat-feed", feed_stat, MBB_CAP_WHEEL),
MBB_INIT_FUNCTIONS_END

static gchar *nf_data_dir__ = NULL;
static gchar *nf_store_dir__ = NULL;

MBB_VAR_DEF(nfd_def) {
	.op_read = var_str_str,
	.op_write = var_conv_dir,

	.cap_read = MBB_CAP_ALL,
	.cap_write = MBB_CAP_ROOT
};

MBB_VAR_DEF(ss_nfd_def) {
	.op_read = var_str_str,
	.op_write = var_conv_dir,
	.cap_read = MBB_CAP_ALL,
	.cap_write = MBB_CAP_ADMIN
};

static void load_module(void)
{
	struct mbb_session_var ss_nfd = {
		.op_new = g_ptr_vardup,
		.op_free = g_ptr_strfree
	};

	if ((stat_lib = mbb_module_import("stat.so")) == NULL) final
		mbb_log_self("import module stat.so failed");

	nf_store_dir__ = g_strdup("/tmp/");

	ss_nfd.data = mbb_module_add_base_var("netflow.data.dir", &nfd_def, &nf_data_dir__);
	nf_data_var = mbb_module_add_session_var(SS_("netflow.data.dir"), &ss_nfd_def, &ss_nfd);

	ss_nfd.data = mbb_module_add_base_var("netflow.store.dir", &nfd_def, &nf_store_dir__);
	nf_store_var = mbb_module_add_session_var(SS_("netflow.store.dir"), &ss_nfd_def, &ss_nfd);

	mbb_module_add_functions(MBB_INIT_FUNCTIONS_TABLE);
}

static void unload_module(void)
{
	g_free(nf_data_dir__);
	g_free(nf_store_dir__);
}

MBB_DEFINE_MODULE("netflow data source")
