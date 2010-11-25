/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "pathtree.h"
#include "unitstat.h"
#include "flow.h"

#include "mbbinetmap.h"
#include "mbbxmlmsg.h"
#include "mbbtask.h"
#include "mbblock.h"
#include "mbbunit.h"
#include "mbbxtv.h"
#include "mbblog.h"

#include "varconv.h"

#include "strerr.h"
#include "macros.h"
#include "trash.h"

#define UNIT_STAT_TASK (unit_stat_quark())

gchar *nf_get_store_dir(gchar *name);

struct unit_stat_opt {
	gboolean with_time;
	gboolean with_proto;
	gboolean with_port;
};

struct task_data {
	struct path_tree pt;
	MbbUMap *umap;
	gchar *dir;

	FlowStream *flow;
	FILE *fout;

	struct unit_stat_opt opt;
};

static gboolean unit_stat_init(struct task_data *td);
static gboolean unit_stat_work(struct task_data *td);
static void task_data_free(struct task_data *td);

static struct mbb_task_hook unit_stat_hook = {
	.init = (gboolean (*)(gpointer)) unit_stat_init,
	.work = (gboolean (*)(gpointer)) unit_stat_work,
	.fini = (void (*)(gpointer)) task_data_free
};

static gboolean unit_stat_init(struct task_data *td)
{
	if (mkdir(td->dir, 0755) < 0) {
		gchar *msg = strerr(errno);

		mbb_log("mkdir %s failed: %s", td->dir, msg);
		g_free(msg);

		return FALSE;
	}

	return TRUE;
}

static gboolean unit_stat_open_next(struct task_data *td)
{
	GError *error = NULL;
	guint mask = 0;

	gchar *fname;
	gchar *tmp;

	if ((fname = path_tree_next(&td->pt)) == NULL)
		return FALSE;

	if (td->opt.with_proto)
		mask |= FLOW_FIELD_PROTO;
	if (td->opt.with_port)
		mask |= FLOW_FIELD_PORT;

	td->flow = flow_stream_new(fname, mask, &error);
	if (td->flow != NULL)
		mbb_log("open %s", fname);
	else {
		mbb_log("failed to open netflow file %s: %s",
			fname, error->message);
		g_error_free(error);
		return TRUE;
	}

	tmp = g_path_get_basename(fname);
	fname = g_strdup_printf("%s/%s", td->dir, tmp);
	g_free(tmp);

	td->fout = fopen(fname, "w");
	if (td->fout == NULL) {
		gchar *msg = strerr(errno);

		flow_stream_close(td->flow);
		td->flow = NULL;

		mbb_log("failed to open store file %s: %s", fname, msg);
		g_free(fname);
		g_free(msg);

		return FALSE;
	}

	g_free(fname);

	return TRUE;
}

static void unit_stat_close(struct task_data *td)
{
	flow_stream_close(td->flow);
	fclose(td->fout);

	td->flow = NULL;
	td->fout = NULL;
}
	
static gboolean unit_stat_work(struct task_data *td)
{
	struct flow_data fd;
	ipv4_buf_t src, dst;
	gchar *dir;

	if (td->flow == NULL)
		return unit_stat_open_next(td);

	if (flow_stream_read(td->flow, &fd) == FALSE) {
		unit_stat_close(td);
		return TRUE;
	}

	if (mbb_umap_find(td->umap, fd.srcaddr, fd.begin) != NULL)
		dir = "<";
	else if (mbb_umap_find(td->umap, fd.dstaddr, fd.begin) != NULL)
		dir = ">";
	else
		return TRUE;

	ipv4toa(src, fd.srcaddr);
	ipv4toa(dst, fd.dstaddr);

	fprintf(td->fout, "%s %15s  %15s", dir, src, dst);

	if (td->opt.with_proto)
		fprintf(td->fout, "  %2d", fd.proto);

	if (td->opt.with_port)
		fprintf(td->fout, "  %5d %5d", fd.srcport, fd.dstport);

	fprintf(td->fout, "  %8d", fd.nbytes);

	if (td->opt.with_time) {
		gchar buf[128];
		struct tm tm;

		localtime_r(&fd.begin, &tm);
		strftime(buf, sizeof(buf), "%Y-%m-%d/%H:%M:%S", &tm);

		fprintf(td->fout, "  %20s ", buf);
	}

	fprintf(td->fout, "\n");

	if (ferror(td->fout)) {
		unit_stat_close(td);
		mbb_log("write failed");
	}

	return TRUE;
}

static void task_data_free(struct task_data *td)
{
	if (td->flow != NULL)
		flow_stream_close(td->flow);

	if (td->fout != NULL)
		fclose(td->fout);

	path_tree_free(&td->pt);
	mbb_umap_free(td->umap);
	g_free(td->dir);
	g_free(td);
}

static GQuark unit_stat_quark(void)
{
	return g_quark_from_string("netflow-unit-stat");
}

static XmlTag *unit_stat_do(MbbUnit *unit, GSList *list,
			    struct unit_stat_opt *opt)
{
	struct task_data *td;
	struct path_tree pt;
	MbbUMap *umap;
	XmlTag *tag;
	gint id;

	if (! path_tree_walk(&pt, list))
		return mbb_xml_msg_error("no such files");

	umap = mbb_umap_from_unit(unit);
	if (umap == NULL) {
		path_tree_free(&pt);
		return mbb_xml_msg_error("empty inet map");
	}

	td = g_new(struct task_data, 1);
	td->dir = nf_get_store_dir(unit->name);
	td->umap = umap;
	td->pt = pt;

	td->fout = NULL;
	td->flow = NULL;
	td->opt = *opt;

	if ((id = mbb_task_create(UNIT_STAT_TASK, &unit_stat_hook, td)) < 0)
		return mbb_xml_msg_error("mbb_task_create failed");

	tag = mbb_xml_msg_ok();
	xml_tag_new_child(tag, "task", "id", variant_new_int(id));

	return tag;
}

static gboolean opt_parse(XmlTag *tag, XmlTag **ans, struct unit_stat_opt *opt)
{
	GSList *opt_list;
	GSList *list;

	opt_list = xml_tag_path_attr_list(tag, "opt", "value");

	for (list = opt_list; list != NULL; list = list->next) {
		gchar *str;

		str = variant_get_string(list->data);
		if (! strcmp(str, "time"))
			opt->with_time = TRUE;
		else if (! strcmp(str, "proto"))
			opt->with_proto = TRUE;
		else if (! strcmp(str, "port"))
			opt->with_port = TRUE;
		else {
			*ans = mbb_xml_msg_error("invalid option '%s'", str);
			break;
		}
	}

	g_slist_free(opt_list);

	return *ans == NULL;
}

void unit_stat(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_UNIT_NAME);

	struct unit_stat_opt opt;
	MbbUnit *unit;
	GSList *list;
	gchar *name;

	MBB_XTV_CALL(&name);

	memset(&opt, 0, sizeof(opt));
	if (opt_parse(tag, ans, &opt) == FALSE)
		return;

	list = xml_tag_path_attr_list(tag, "glob", "value");

	if (list == NULL) final
		*ans = mbb_xml_msg_error("nothing to do");

	mbb_lock_reader_lock();

	on_final { mbb_lock_reader_unlock(); }

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);

	*ans = unit_stat_do(unit, list, &opt);

	mbb_lock_reader_unlock();

	g_slist_free(list);
}

