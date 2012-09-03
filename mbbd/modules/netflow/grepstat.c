/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "netflow.h"

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
#define ODD_STAT_TASK (odd_stat_quark())

struct grep_stat_opt {
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

	gboolean (*cond_func)(MbbUMap *, struct flow_data *, gchar **);

	struct grep_stat_opt opt;
};

static gboolean grep_stat_init(struct task_data *td);
static gboolean grep_stat_work(struct task_data *td);
static void task_data_free(struct task_data *td);

static struct mbb_task_hook grep_stat_hook = {
	.init = (gboolean (*)(gpointer)) grep_stat_init,
	.work = (gboolean (*)(gpointer)) grep_stat_work,
	.fini = (void (*)(gpointer)) task_data_free
};

static gboolean grep_stat_init(struct task_data *td)
{
	gboolean created = FALSE;
	mode_t mode = 0755;

	if (mkdir(td->dir, mode) == 0)
		created = TRUE;
	else {
		gchar *msg;

		if (errno != EEXIST) {
			msg = strerr(errno);
			mbb_log("mkdir %s failed: %s", td->dir, msg);
			g_free(msg);
		} else {
			gsize len = strlen(td->dir);
			gchar *buf = g_strndup(td->dir, len + 2);
			gchar *p = buf + len;
			gint n;

			g_free(td->dir);
			td->dir = buf;

			for (n = 1, *p++ = '.'; ! created && n < 10; n++) {
				g_snprintf(p, 2, "%d", n);

				if (mkdir(buf, mode) == 0)
					created = TRUE;
				else if (errno != EEXIST) {
					msg = strerr(errno);
					mbb_log("mkdir %s failed: %s", buf, msg);
					g_free(msg);
					goto out;
				}
			}

			if (! created)
				mbb_log("already 10 directories exists");
		}
	}

out:
	return created;
}

static gboolean grep_stat_open_next(struct task_data *td)
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

static void grep_stat_close(struct task_data *td)
{
	flow_stream_close(td->flow);
	fclose(td->fout);

	td->flow = NULL;
	td->fout = NULL;
}

static void grep_stat_write_down(struct task_data *td, gchar *prefix,
				 struct flow_data *fd)
{
	ipv4_buf_t src, dst;

	if (prefix != NULL)
		fprintf(td->fout, "%s ", prefix);

	ipv4toa(src, fd->srcaddr);
	ipv4toa(dst, fd->dstaddr);

	fprintf(td->fout, "%15s  %15s", src, dst);

	if (td->opt.with_proto)
		fprintf(td->fout, "  %2d", fd->proto);

	if (td->opt.with_port)
		fprintf(td->fout, "  %5d %5d", fd->srcport, fd->dstport);

	fprintf(td->fout, "  %8d", fd->nbytes);

	if (td->opt.with_time) {
		gchar buf[128];
		struct tm tm;

		localtime_r(&fd->begin, &tm);
		strftime(buf, sizeof(buf), "%Y-%m-%d/%H:%M:%S", &tm);

		fprintf(td->fout, "  %20s ", buf);
	}

	fprintf(td->fout, "\n");
}

static gboolean grep_stat_work(struct task_data *td)
{
	struct flow_data fd;
	gchar *prefix = NULL;

	if (td->flow == NULL)
		return grep_stat_open_next(td);

	if (flow_stream_read(td->flow, &fd) == FALSE) {
		grep_stat_close(td);
		return TRUE;
	}

	if (! td->cond_func(td->umap, &fd, &prefix))
		return TRUE;

	grep_stat_write_down(td, prefix, &fd);

	if (ferror(td->fout)) {
		grep_stat_close(td);
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

static GQuark odd_stat_quark(void)
{
	return g_quark_from_string("netflow-odd-stat");
}

static inline struct task_data *task_data_new(gchar *suffix, MbbUMap *umap,
					      struct path_tree *pt,
					      struct grep_stat_opt *opt)
{
	struct task_data *td;

	td = g_new(struct task_data, 1);
	td->dir = netflow_get_store_dir(suffix);
	td->umap = umap;
	td->pt = *pt;

	td->cond_func = NULL;
	td->fout = NULL;
	td->flow = NULL;
	td->opt = *opt;

	return td;
}

static gboolean unit_stat_cond(MbbUMap *umap, struct flow_data *fd, gchar **prefix)
{
	if (mbb_umap_find(umap, fd->srcaddr, fd->begin) != NULL)
		*prefix = "<";
	else if (mbb_umap_find(umap, fd->dstaddr, fd->begin) != NULL)
		*prefix = ">";
	else
		return FALSE;

	return TRUE;
}

static XmlTag *unit_stat_do(MbbUnit *unit, GSList *list,
			    struct grep_stat_opt *opt)
{
	struct task_data *td;
	struct path_tree pt;
	MbbUMap *umap;
	gint id;

	if (! path_tree_walk(&pt, list))
		return mbb_xml_msg_error("no such files");

	umap = mbb_umap_from_unit(unit);
	if (umap == NULL) {
		path_tree_free(&pt);
		return mbb_xml_msg_error("empty inet map");
	}

	td = task_data_new(unit->name, umap, &pt, opt);
	td->cond_func = unit_stat_cond;

	if ((id = mbb_task_create(UNIT_STAT_TASK, &grep_stat_hook, td)) < 0)
		return mbb_xml_msg_error("mbb_task_create failed");

	return mbb_xml_msg_task_id(id);
}

static gboolean odd_stat_cond(MbbUMap *umap, struct flow_data *fd, gchar **prefix G_GNUC_UNUSED)
{
	if (mbb_umap_find(umap, fd->srcaddr, fd->begin) != NULL)
		return FALSE;

	if (mbb_umap_find(umap, fd->dstaddr, fd->begin) != NULL)
		return FALSE;

	return TRUE;
}

static XmlTag *odd_stat_do(gchar *name, GSList *list, struct grep_stat_opt *opt)
{
	struct task_data *td;
	struct path_tree pt;
	MbbUMap *umap;
	gint id;

	if (! path_tree_walk(&pt, list))
		return mbb_xml_msg_error("no such files");

	mbb_lock_reader_lock();
	umap = mbb_umap_create();
	mbb_lock_reader_unlock();

	if (umap == NULL) {
		path_tree_free(&pt);
		return mbb_xml_msg_error("empty inet map");
	}

	td = task_data_new(name, umap, &pt, opt);
	td->cond_func = odd_stat_cond;

	if ((id = mbb_task_create(ODD_STAT_TASK, &grep_stat_hook, td)) < 0)
		return mbb_xml_msg_error("mbb_task_create failed");

	return mbb_xml_msg_task_id(id);
}

static gboolean opt_parse(XmlTag *tag, XmlTag **ans, struct grep_stat_opt *opt)
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

	struct grep_stat_opt opt;
	MbbUnit *unit;
	GSList *list;
	gchar *name;

	MBB_XTV_CALL(&name);

	memset(&opt, 0, sizeof(opt));
	if (opt_parse(tag, ans, &opt) == FALSE)
		return;

	if ((list = netflow_get_glob_list(tag, ans)) == NULL)
		return;

	mbb_lock_reader_lock();

	on_final { mbb_lock_reader_unlock(); }

	unit = mbb_unit_get_by_name(name);
	if (unit == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);

	*ans = unit_stat_do(unit, list, &opt);

	mbb_lock_reader_unlock();

	g_slist_free(list);
}

void odd_stat(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_NAME_VALUE);

	struct grep_stat_opt opt;
	GSList *list;
	gchar *name;

	MBB_XTV_CALL(&name);

	memset(&opt, 0, sizeof(opt));
	if (opt_parse(tag, ans, &opt) == FALSE)
		return;

	if ((list = netflow_get_glob_list(tag, ans)) == NULL)
		return;

	*ans = odd_stat_do(name, list, &opt);

	g_slist_free(list);
}

