/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <stdarg.h>

#include "pathtree.h"
#include "flow.h"

#include "mbbinetmap.h"
#include "mbbgateway.h"
#include "mbbxmlmsg.h"
#include "mbbgwlink.h"
#include "mbbunit.h"
#include "mbbtask.h"
#include "mbblock.h"
#include "mbbxtv.h"
#include "mbblog.h"

#include "stat/interface.h"

#include "macros.h"

#define PLAIN_STAT_TASK (plain_stat_quark())
#define UPDATE_STAT_TASK (update_stat_quark())
#define FEED_STAT_TASK (feed_stat_quark())

extern struct stat_interface *stat_lib;

GString *nf_get_data_dir(void);

struct base_task_data {
	struct mbb_stat_pool *pool;
	struct path_tree pt;
	FlowStream *flow;
	MbbUMap *umap;

	gboolean (*op_init)(struct base_task_data *td);
	gboolean (*op_work)(struct base_task_data *td);
};

struct plain_task_data {
	struct base_task_data base;
};

struct update_task_data {
	struct base_task_data base;
	time_t start;
	time_t end;
};

static gboolean base_task_init(struct base_task_data *td);
static gboolean base_task_work(struct base_task_data *td);
static void base_task_free(struct base_task_data *td);

static struct mbb_task_hook base_task_hook = {
	.init = (mbb_task_work_t) base_task_init,
	.work = (mbb_task_work_t) base_task_work,
	.fini = (mbb_task_free_t) base_task_free
};

struct feed_task_data {
	struct mbb_stat_pool *pool;
	FlowStream *flow;
	MbbUMap *umap;
};

static gboolean feed_stat_init(struct feed_task_data *td);
static gboolean feed_stat_work(struct feed_task_data *td);
static void feed_stat_free(struct feed_task_data *td);

static struct mbb_task_hook feed_stat_hook = {
	.init = (mbb_task_work_t) feed_stat_init,
	.work = (mbb_task_work_t) feed_stat_work,
	.fini = (mbb_task_free_t) feed_stat_free
};

static void base_task_free(struct base_task_data *td)
{
	if (td->pool != NULL)
		stat_lib->pool_free(td->pool);

	if (td->flow != NULL)
		flow_stream_close(td->flow);

	path_tree_free(&td->pt);
	mbb_umap_free(td->umap);
	g_free(td);
}

static gboolean path_tree_open_next(struct base_task_data *td)
{
	GError *error = NULL;
	gchar *fname;

	if ((fname = path_tree_next(&td->pt)) == NULL)
		return FALSE;

	td->flow = flow_stream_new(fname, 0, &error);
	if (td->flow != NULL) {
		mbb_log("open %s", fname);
		td->pool = stat_lib->pool_new();
	} else {
		mbb_log("failed to open netflow file %s: %s",
			fname, error->message);
		g_error_free(error);
	}

	return TRUE;
}

static inline gint get_link_id(ipv4_t gw, guint link_no, time_t t)
{
	MbbGwLink *link;
	gint id = -1;

	mbb_lock_reader_lock();
	if ((link = mbb_gateway_find_link(gw, link_no, t)) != NULL)
		id = link->id;
	mbb_lock_reader_unlock();

	return id;
}

static inline gboolean islocal(MbbUnit *unit)
{
	if (unit == NULL)
		return FALSE;

	return unit->local;
}

static gboolean process_flow_data(struct flow_data *fd, MbbUMap *umap,
				  struct mbb_stat_pool *pool)
{
	struct mbb_stat_entry entry;
	MbbUnit *una, *unb;
	gint id;

	entry.point = fd->begin;

	una = mbb_umap_find(umap, fd->srcaddr, fd->begin);
	unb = mbb_umap_find(umap, fd->dstaddr, fd->begin);

	if (una != NULL && ! islocal(unb)) {
		if ((id = get_link_id(fd->exaddr, fd->output, fd->begin)) >= 0) {
			entry.unit_id = una->id;
			entry.link_id = id;

			entry.nbyte_in = 0;
			entry.nbyte_out = fd->nbytes;

			stat_lib->pool_feed(pool, &entry);
		}
	}

	if (unb != NULL && ! islocal(una)) {
		if ((id = get_link_id(fd->exaddr, fd->input, fd->begin)) >= 0) {
			entry.unit_id = unb->id;
			entry.link_id = id;

			entry.nbyte_in = fd->nbytes;
			entry.nbyte_out = 0;

			stat_lib->pool_feed(pool, &entry);
		}
	}

	return TRUE;
}

static gboolean update_task_init(struct base_task_data *td)
{
	struct update_task_data *utd = (gpointer) td;
	GError *error = NULL;

	if (! stat_lib->db_wipe(utd->start, utd->end, &error)) {
		mbb_log("mbb_stat_db_wipe failed: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	return TRUE;
}

static gboolean base_task_init(struct base_task_data *td)
{
	if (! stat_lib->pool_init())
		return FALSE;

	if (td->op_init != NULL)
		return td->op_init(td);

	return TRUE;
}

static gboolean update_task_work(struct base_task_data *td)
{
	struct update_task_data *utd = (gpointer) td;
	struct flow_data fd;

	if (flow_stream_read(td->flow, &fd) == FALSE)
		return FALSE;

	if (utd->start <= fd.begin && fd.begin < utd->end)
		process_flow_data(&fd, td->umap, td->pool);

	return TRUE;
}

static gboolean plain_task_work(struct base_task_data *td)
{
	struct flow_data fd;

	if (flow_stream_read(td->flow, &fd) == FALSE)
		return FALSE;

	process_flow_data(&fd, td->umap, td->pool);

	return TRUE;
}

static gboolean base_task_work(struct base_task_data *td)
{
	if (td->flow == NULL)
		return path_tree_open_next(td);

	if (td->op_work(td) == FALSE) {
		flow_stream_close(td->flow);
		td->flow = NULL;

		stat_lib->pool_save(td->pool);
		td->pool = NULL;
	}

	return TRUE;
}

static GQuark plain_stat_quark(void)
{
	return g_quark_from_string("netflow-stat-plain");
}

static GQuark update_stat_quark(void)
{
	return g_quark_from_string("netflow-stat-update");
}

static inline struct base_task_data *
base_task_data_new(gsize size, MbbUMap *umap, struct path_tree *pt)
{
	struct base_task_data *td;

	td = g_malloc0(size);

	td->umap = umap;
	td->pt = *pt;

	return td;
}

static inline struct update_task_data *
update_task_data_new(MbbUMap *umap, struct path_tree *pt)
{
	struct base_task_data *td;

	td = base_task_data_new(sizeof(struct update_task_data), umap, pt);
	td->op_init = update_task_init;
	td->op_work = update_task_work;

	return (gpointer) td;
}

static inline struct plain_task_data *
plain_task_data_new(MbbUMap *umap, struct path_tree *pt)
{
	struct base_task_data *td;

	td = base_task_data_new(sizeof(struct plain_task_data), umap, pt);
	td->op_work = plain_task_work;

	return (gpointer) td;
}

static XmlTag *base_task_do(GSList *list, gboolean update, ...)
{
	struct base_task_data *td;
	struct path_tree pt;
	GQuark task_name;
	MbbUMap *umap;
	XmlTag *tag;
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

	if (update == FALSE) {
		td = (gpointer) plain_task_data_new(umap, &pt);
		task_name = PLAIN_STAT_TASK;
	} else {
		struct update_task_data *utd;
		va_list ap;

		utd = update_task_data_new(umap, &pt);

		va_start(ap, update);
		utd->start = va_arg(ap, time_t);
		utd->end = va_arg(ap, time_t);
		va_end(ap);

		task_name = UPDATE_STAT_TASK;
		td = (gpointer) utd;
	}

	id = mbb_task_create(task_name, &base_task_hook, td);

	if (id < 0)
		return mbb_xml_msg(MBB_MSG_TASK_CREATE_FAILED);

	tag = mbb_xml_msg_ok();
	xml_tag_new_child(tag, "task", "id", variant_new_int(id));

	return tag;
}

static gboolean get_glob_list(XmlTag *tag, GSList **ptr, XmlTag **ans)
{
	GSList *list;

	list = xml_tag_path_attr_list(tag, "glob", "value");

	if (list == NULL) {
		*ans = mbb_xml_msg_error("nothing to do");
		return FALSE;
	}

	*ptr = list;

	return TRUE;
}

void update_stat(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_TIME_START, XTV_TIME_END_, XTV_TIME_PERIOD_);

	time_t start, end;
	gchar *period;
	GSList *list;

	period = NULL;
	end = VAR_CONV_TIME_UNSET;
	MBB_XTV_CALL(&start, &end, &period);

	if (! stat_lib->parse_tag(&start, &end, period, NULL, ans))
		return;

	if (get_glob_list(tag, &list, ans) == FALSE)
		return;

	*ans = base_task_do(list, TRUE, start, end);

	g_slist_free(list);
}

void plain_stat(XmlTag *tag, XmlTag **ans)
{
	GSList *list;

	if (get_glob_list(tag, &list, ans)) {
		*ans = base_task_do(list, FALSE);
		g_slist_free(list);
	}
}

static GQuark feed_stat_quark(void)
{
	return g_quark_from_string("netflow-stat-feed");
}

static gboolean feed_stat_init(struct feed_task_data *td)
{
	if (! stat_lib->pool_init())
		return FALSE;

	td->pool = stat_lib->pool_new();

	return TRUE;
}

static gboolean feed_stat_work(struct feed_task_data *td)
{
	struct flow_data fd;

	if (flow_stream_read(td->flow, &fd))
		process_flow_data(&fd, td->umap, td->pool);
	else {
		stat_lib->pool_save(td->pool);
		td->pool = NULL;

		return FALSE;
	}

	return TRUE;
}

static void feed_stat_free(struct feed_task_data *td)
{
	if (td->pool != NULL)
		stat_lib->pool_free(td->pool);

	flow_stream_close(td->flow);
	mbb_umap_free(td->umap);
	g_free(td);
}

static FlowStream *flow_from_tag(XmlTag *tag, XmlTag **ans)
{
	GError *error = NULL;
	FlowStream *flow;
	Variant *var;
	gchar *name;
	gchar *tmp;

	var = xml_tag_path_attr(tag, "file", "name");
	if (var == NULL) {
		*ans = mbb_xml_msg_error("file argument missed");
		return NULL;
	}

	tmp = variant_get_string(var);

	if (*tmp == '/')
		name = tmp;
	else {
		GString *string = nf_get_data_dir();

		if (string == NULL) {
			*ans = mbb_xml_msg_error("netflow data dir not set");
			return NULL;
		}

		g_string_append(string, tmp);
		name = g_string_free(string, FALSE);
	}

	flow = flow_stream_new(name, 0, &error);

	if (flow == NULL) {
		*ans = mbb_xml_msg_error("invalid netflow file: %s", error->message);
		g_error_free(error);
	}

	if (name != tmp)
		g_free(name);

	return flow;
}

void feed_stat(XmlTag *tag, XmlTag **ans)
{
	struct feed_task_data *td;
	FlowStream *flow;
	MbbUMap *umap;
	gint id;

	if ((flow = flow_from_tag(tag, ans)) == NULL)
		final;

	mbb_lock_reader_lock();
	umap = mbb_umap_create();
	mbb_lock_reader_unlock();

	if (umap == NULL) final {
		flow_stream_close(flow);
		*ans = mbb_xml_msg_error("empty inet map");
	}

	td = g_new(struct feed_task_data, 1);
	td->flow = flow;
	td->umap = umap;
	td->pool = NULL;

	id = mbb_task_create(FEED_STAT_TASK, &feed_stat_hook, td);
	if (id < 0) final {
		feed_stat_free(td);
		*ans = mbb_xml_msg(MBB_MSG_TASK_CREATE_FAILED);
	}

	*ans = mbb_xml_msg_ok();
	xml_tag_new_child(*ans, "task", "id", variant_new_int(id));
}

