/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbmodule.h"
#include "mbbxmlmsg.h"
#include "mbbthread.h"
#include "mbbplock.h"
#include "mbbtask.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbblog.h"
#include "mbbxtv.h"

#include "xmltag.h"
#include "macros.h"

#include <time.h>

struct mbb_task {
	GQuark name;
	guint id;

	volatile gboolean cancel;
	volatile gboolean run;

	struct mbb_user *user;
	time_t start;
	gint sid;

	struct mbb_task_hook *hook;
	gpointer data;

	GMutex *mutex;
	GCond *cond;

	MbbModule *mod;
};

static guint task_id = 1;
static GHashTable *ht = NULL;
static GStaticPrivate task_key = G_STATIC_PRIVATE_INIT;

gint mbb_task_get_id(void)
{
	struct mbb_task *task;

	task = g_static_private_get(&task_key);
	if (task == NULL)
		return -1;

	return task->id;
}

gint mbb_task_get_tid(void)
{
	struct mbb_task *task;

	task = g_static_private_get(&task_key);
	if (task == NULL)
		return -1;

	return task->sid;
}

struct mbb_user *mbb_task_get_user(void)
{
	struct mbb_task *task;

	task = g_static_private_get(&task_key);
	if (task == NULL)
		return NULL;

	return task->user;
}

gchar *mbb_task_get_name(void)
{
	struct mbb_task *task;

	task = g_static_private_get(&task_key);
	if (task == NULL)
		return NULL;

	return (gchar *) g_quark_to_string(task->name);
}

static struct mbb_task *mbb_task_new(GQuark name, struct mbb_task_hook *hook, gpointer data)
{
	struct mbb_task *task;

	task = g_new(struct mbb_task, 1);
	task->name = name;
	time(&task->start);
	task->cancel = FALSE;
	task->run = TRUE;

	task->sid = mbb_thread_get_tid();
	if (task->sid >= 0)
		task->user = mbb_user_ref(mbb_thread_get_user());
	else {
		task->sid = mbb_task_get_tid();
		task->user = mbb_user_ref(mbb_task_get_user());
	}

	task->hook = hook;
	task->data = data;

	task->mutex = g_mutex_new();
	task->cond = g_cond_new();

	task->mod = mbb_module_last_used();
	mbb_module_use(task->mod);

	mbb_plock_writer_lock();

	if (ht == NULL)
		ht = g_hash_table_new(g_direct_hash, g_direct_equal);
	task->id = task_id++;
	g_hash_table_insert(ht, GINT_TO_POINTER(task->id), task);

	mbb_plock_writer_unlock();

	return task;
}

static void mbb_task_free(struct mbb_task *task)
{
	mbb_plock_writer_lock();

	if (ht != NULL)
		g_hash_table_remove(ht, GINT_TO_POINTER(task->id));

	mbb_plock_writer_unlock();

	mbb_module_unuse(task->mod);
	mbb_user_unref(task->user);

	g_mutex_free(task->mutex);
	g_cond_free(task->cond);
	g_free(task);
}

static inline struct mbb_task *mbb_task_get(guint id)
{
	if (ht == NULL)
		return NULL;

	return g_hash_table_lookup(ht, GINT_TO_POINTER(id));
}

static inline void mbb_task_pause(struct mbb_task *task)
{
	g_mutex_lock(task->mutex);
	while (task->run == FALSE && task->cancel == FALSE)
		g_cond_wait(task->cond, task->mutex);
	g_mutex_unlock(task->mutex);
}

static inline void mbb_task_resume(struct mbb_task *task)
{
	g_cond_signal(task->cond);
}

static gboolean task_poll(struct mbb_task *task)
{
	if (task->run == FALSE) {
		mbb_log("paused");
		mbb_task_pause(task);

		if (task->cancel == FALSE)
			mbb_log("resumed");
	}

	if (task->cancel) {
		mbb_log("canceled");
		return FALSE;
	}

	return TRUE;
}

static gpointer mbb_task_work(struct mbb_task *task)
{
	struct mbb_task_hook *hook = task->hook;

	g_static_private_set(&task_key, task, (GDestroyNotify) mbb_task_free);
	mbb_log("started");

	if (hook->init != NULL) {
		if (! hook->init(task->data)) {
			mbb_log("init failed");
			return NULL;
		}
	}

	while (hook->work(task->data)) {
		if (task_poll(task) == FALSE)
			break;
	}

	if (hook->fini != NULL)
		hook->fini(task->data);

	if (! task->cancel)
		mbb_log("complete");

	return NULL;
}

gboolean mbb_task_poll_state(void)
{
	struct mbb_task *task;

	task = g_static_private_get(&task_key);
	if (task == NULL)
		return TRUE;

	return task_poll(task);
}

gint mbb_task_create(GQuark name, struct mbb_task_hook *hook, gpointer data)
{
	struct mbb_task *task;
	GThread *thread;

	if (hook->work == NULL)
		return -1;

	task = mbb_task_new(name, hook, data);
	thread = g_thread_create((GThreadFunc) mbb_task_work, task, FALSE, NULL);

	if (thread == NULL) {
		if (hook->fini != NULL)
			hook->fini(data);
		mbb_task_free(task);

		mbb_log("g_thread_create failed");
		return -1;
	}

	return task->id;
}

static void task_do_run(XmlTag *tag, XmlTag **ans, gboolean run)
{
	DEFINE_XTV(XTV_TASK_ID);

	struct mbb_task *task;
	guint id;

	MBB_XTV_CALL(&id);

	mbb_plock_reader_lock();

	on_final { mbb_plock_reader_unlock(); }

	task = mbb_task_get(id);
	if (task == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_TASK);

	g_mutex_lock(task->mutex);
	if (task->run == run) final {
		g_mutex_unlock(task->mutex);
		*ans = mbb_xml_msg_error(
			"task is already %s", run ? "run" : "stop"
		);
	}

	task->run = run;
	if (run) mbb_task_resume(task);
	g_mutex_unlock(task->mutex);

	mbb_plock_reader_unlock();
}

static void task_run(XmlTag *tag, XmlTag **ans)
{
	task_do_run(tag, ans, TRUE);
}

static void task_stop(XmlTag *tag, XmlTag **ans)
{
	task_do_run(tag, ans, FALSE);
}

static void task_cancel(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_TASK_ID);

	struct mbb_task *task;
	guint id;

	MBB_XTV_CALL(&id);

	mbb_plock_reader_lock();

	on_final { mbb_plock_reader_unlock(); }

	task = mbb_task_get(id);
	if (task == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_TASK);

	task->cancel = TRUE;
	g_mutex_lock(task->mutex);
	if (task->run == FALSE) {
		task->run = TRUE;
		mbb_task_resume(task);
	}
	g_mutex_unlock(task->mutex);

	mbb_plock_reader_unlock();
}

static void gather_task(gpointer key G_GNUC_UNUSED, gpointer value, gpointer data)
{
	struct mbb_task *task = value;
	XmlTag *tag = data;

	gchar *username;
	gchar *state;
	gchar *name;

	mbb_user_lock(task->user);
	username = g_strdup(task->user->name);
	mbb_user_unlock(task->user);

	name = (gchar *) g_quark_to_string(task->name);
	state = task->run ? "run" : "stop";

	xml_tag_new_child(tag, "task",
		"id", variant_new_int(task->id),
		"name", variant_new_static_string(name),
		"sid", variant_new_int(task->sid),
		"state", variant_new_static_string(state),
		"start", variant_new_long(task->start),
		"user", variant_new_alloc_string(username)
	);
}

static void show_tasks(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	*ans = mbb_xml_msg_ok();

	mbb_plock_reader_lock();

	if (ht != NULL)
		g_hash_table_foreach(ht, gather_task, *ans);

	mbb_plock_reader_unlock();
}

MBB_INIT_FUNCTIONS_DO
	MBB_FUNC_STRUCT("mbb-task-run", task_run, MBB_CAP_WHEEL),
	MBB_FUNC_STRUCT("mbb-task-stop", task_stop, MBB_CAP_WHEEL),
	MBB_FUNC_STRUCT("mbb-task-cancel", task_cancel, MBB_CAP_WHEEL),
	MBB_FUNC_STRUCT("mbb-show-tasks", show_tasks, MBB_CAP_ADMIN),
MBB_INIT_FUNCTIONS_END

MBB_ON_INIT(MBB_INIT_FUNCTIONS)
