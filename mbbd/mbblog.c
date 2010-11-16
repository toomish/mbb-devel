#include "mbbmsgqueue.h"
#include "mbbsession.h"
#include "signaller.h"
#include "mbbthread.h"
#include "mbbxmlmsg.h"
#include "mbbuser.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbbtask.h"
#include "mbblog.h"
#include "mbbcap.h"
#include "mbbxtv.h"

#include "strconv.h"
#include "macros.h"
#include "shared.h"
#include "debug.h"

#include <string.h>

#define LOG_MASK_FULL ((mbb_log_lvl_t) -1)

static struct {
	mbb_log_lvl_t lvl;
	gchar *name;
} log_levels[] = {
	{ MBB_LOG_MSG,		"msg" },
	{ MBB_LOG_XML,		"xml" },
	{ MBB_LOG_HTTP,		"http" },
	{ MBB_LOG_USER,		"user" },
	{ MBB_LOG_SELF,		"self" },
	{ MBB_LOG_TASK,		"task" },
	{ MBB_LOG_QUERY,	"query" },
	{ MBB_LOG_DEBUG,	"debug" },
	{ MBB_LOG_DEBUGV,	"debugv" }
};

enum log_status { LOG_NONE, LOG_ON, LOG_OFF };

struct thread_log_data {
	MbbMsgQueue *msg_queue;
	Signaller *signaller;
	mbb_log_lvl_t lvl_mask;
	GSList *traced;
};

struct log_msg {
	gchar *domain;

	gchar *fmt;
	va_list ap;

	Shared *shared;
	gsize len;

	mbb_log_lvl_t lvl;
	gint tid;

	gboolean task;
};

static GHashTable *ht = NULL;
static GStaticRWLock rwlock = G_STATIC_RW_LOCK_INIT;
static GStaticPrivate log_mask_key = G_STATIC_PRIVATE_INIT;

static inline gboolean mask_has_lvl(mbb_log_lvl_t mask, mbb_log_lvl_t lvl)
{
	return (mask & lvl) == lvl;
}

static void tld_free(struct thread_log_data *tld)
{
	msg_warn("tld_free %p", (void *) tld);
	g_slist_free(tld->traced);
	g_free(tld);
}

static inline void ht_init(void)
{
	ht = g_hash_table_new_full(
		g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) tld_free
	);
}

static struct thread_log_data *tld_new(mbb_log_lvl_t mask)
{
	struct thread_log_data *tld;
	MbbMsgQueue *msg_queue;

	msg_queue = mbb_thread_get_msg_queue();

	if (msg_queue == NULL)
		return NULL;

	tld = g_new(struct thread_log_data, 1);
	tld->msg_queue = msg_queue;
	tld->signaller = mbb_thread_get_signaller();
	tld->lvl_mask = mask;
	tld->traced = NULL;

	msg_warn("tld_new %p", (void *) tld);

	return tld;
}

static inline gchar *log_msg_new(gchar *domain, gchar *msg)
{
	struct mbb_user *user;
	gchar *name;
	gchar *peer;
	gchar *str;
	gint tid;

	user = mbb_thread_get_user();
	tid = mbb_thread_get_tid();
	peer = mbb_thread_get_peer();

	if (user == NULL)
		name = "*";
	else {
		mbb_user_lock(user);
		name = g_strdup(user->name);
		mbb_user_unlock(user);
	}

	str = g_markup_printf_escaped(
		"<log sid='%d' user='%s' peer='%s' domain='%s'>" \
			"<message value='%s'/>" \
		"</log>",
		tid, name, peer, domain, msg
	);

	if (user != NULL)
		g_free(name);

	return str;
}

static inline gchar *log_task_msg_new(gchar *domain, gchar *msg)
{
	gchar *name;
	gchar *str;
	gint tid;
	gint id;

	name = mbb_task_get_name();
	tid = mbb_task_get_tid();
	id = mbb_task_get_id();

	str = g_markup_printf_escaped(
		"<log sid='%d' task_id='%d' peer='%s' domain='%s'>" \
			"<message value='%s'/>" \
		"</log>",
		tid, id, name, domain, msg
	);

	return str;
}

static inline Shared *msg_get_shared_ref(struct log_msg *msg)
{
	if (msg->shared == NULL) {
		gchar *text;
		gchar *str;

		text = g_strdup_vprintf(msg->fmt, msg->ap);
		if (msg->task)
			str = log_task_msg_new(msg->domain, text);
		else
			str = log_msg_new(msg->domain, text);
		g_free(text);

		msg->shared = shared_new(str, g_free);
		msg->len = strlen(str);
	}

	return shared_ref(msg->shared);
}

static inline void tld_push_msg(struct thread_log_data *tld, struct log_msg *msg)
{
	(void) msg_get_shared_ref(msg);
	mbb_msg_queue_push_shared(tld->msg_queue, msg->shared, msg->len);

	if (tld->signaller != NULL)
		signaller_raise(tld->signaller);
}

static void mbb_log_push(gpointer key, gpointer value, gpointer data)
{
	struct thread_log_data *tld;
	struct log_msg *msg;

	tld = (struct thread_log_data *) value;
	msg = (struct log_msg *) data;

	/* if self tid in tld->traced then recieve logs without log level self */

	if (tld->traced != NULL) {
		if (! g_slist_find(tld->traced, GINT_TO_POINTER(msg->tid)))
			return;
	} else {
		if (GPOINTER_TO_INT(key) == msg->tid) {
			if (! mask_has_lvl(tld->lvl_mask, MBB_LOG_SELF))
				return;
		} else if (msg->lvl == MBB_LOG_SELF)
			return;
	}

	if (mask_has_lvl(tld->lvl_mask, msg->lvl))
		tld_push_msg(tld, msg);
}

static inline gchar *log_lvl_name(mbb_log_lvl_t lvl)
{
	guint n;

	for (n = 0; n < NELEM(log_levels); n++)
		if (lvl == log_levels[n].lvl)
			return log_levels[n].name;

	return NULL;
}

static gboolean log_lvl_ispermit(mbb_log_lvl_t lvl)
{
	mbb_log_lvl_t log_mask;

	log_mask = GPOINTER_TO_INT(g_static_private_get(&log_mask_key));
	if (log_mask == 0)
		return TRUE;

	return mask_has_lvl(log_mask, lvl);
}

static void mbb_log_push_all(mbb_log_lvl_t lvl, gchar *fmt, va_list ap)
{
	gboolean task = FALSE;
	gchar *domain;

	if (log_lvl_ispermit(lvl) == FALSE)
		return;

	if (mbb_thread_get_peer() == NULL)  {
		task = TRUE;
		lvl = MBB_LOG_TASK;
	}

	domain = log_lvl_name(lvl);
	if (domain == NULL)
		return;

	g_static_rw_lock_reader_lock(&rwlock);

	if (ht != NULL && g_hash_table_size(ht) != 0) {
		struct log_msg msg;

		msg.domain = domain;
		msg.fmt = fmt;
		va_copy(msg.ap, ap);

		msg.shared = NULL;
		msg.len = 0;
		msg.lvl = lvl;
		msg.task = task;

		if (task)
			msg.tid = mbb_task_get_tid();
		else
			msg.tid = mbb_thread_get_tid();

		g_hash_table_foreach(ht, mbb_log_push, &msg);

		if (msg.shared != NULL)
			shared_unref(msg.shared);
		va_end(msg.ap);
	}

	g_static_rw_lock_reader_unlock(&rwlock);
}

void mbb_log_lvl(mbb_log_lvl_t lvl, gchar *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	mbb_log_push_all(lvl, fmt, ap);
	va_end(ap);
}

void mbb_log(gchar *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	mbb_log_push_all(MBB_LOG_MSG, fmt, ap);
	va_end(ap);
}

void mbb_log_mask(gint how, mbb_log_lvl_t mask, mbb_log_lvl_t *omask)
{
	mbb_log_lvl_t log_mask;

	log_mask = GPOINTER_TO_INT(g_static_private_get(&log_mask_key));
	if (log_mask == 0)
		log_mask = LOG_MASK_FULL;

	if (omask != NULL)
		*omask = log_mask;

	switch (how) {
	case LOG_MASK_ADD:
		log_mask |= mask;
		break;
	case LOG_MASK_DEL:
		log_mask &= ~mask;
		break;
	case LOG_MASK_SET:
		log_mask = mask;
		break;
	default:
		msg_warn("invalid how value %d", how);
	}

	if (log_mask == LOG_MASK_FULL)
		log_mask = 0;

	g_static_private_set(&log_mask_key, GINT_TO_POINTER(log_mask), NULL);
}

static mbb_log_lvl_t log_lvl_mask(GSList *list)
{
	mbb_log_lvl_t mask;
	gchar *lvl_name;
	guint n;

	mask = 0;
	for (; list != NULL; list = list->next) {
		lvl_name = variant_get_string(list->data);

		for (n = 0; n < NELEM(log_levels); n++)
			if (! g_strcmp0(lvl_name, log_levels[n].name)) {
				mask |= log_levels[n].lvl;
				break;
			}
	}

	return mask;
}

static GSList *log_lvl_names(mbb_log_lvl_t mask)
{
	GSList *list;
	guint n;

	list = NULL;
	for (n = 0; n < NELEM(log_levels); n++)
		if (mask_has_lvl(mask, log_levels[n].lvl))
			list = g_slist_prepend(list, log_levels[n].name);

	return list;
}

static inline struct thread_log_data *tld_get(gint tid)
{
	if (ht != NULL)
		return g_hash_table_lookup(ht, GINT_TO_POINTER(tid));

	return NULL;
}

static struct thread_log_data *tld_get_or_creat(gint tid)
{
	struct thread_log_data *tld = NULL;

	if (ht == NULL)
		ht_init();
	else
		tld = g_hash_table_lookup(ht, GINT_TO_POINTER(tid));

	if (tld == NULL) {
		tld = tld_new(0);

		if (tld != NULL)
			g_hash_table_insert(ht, GINT_TO_POINTER(tid), tld);
	}

	return tld;
}

static inline void tld_del(gint tid)
{
	g_hash_table_remove(ht, GINT_TO_POINTER(tid));
}

static void mbb_log_set_lvl_mask(mbb_log_lvl_t mask)
{
	enum log_status ls;
	gint tid;

	ls = LOG_NONE;
	tid = mbb_thread_get_tid();

	g_static_rw_lock_writer_lock(&rwlock);

	if (mask == 0) {
		if (ht != NULL) {
			tld_del(tid);
			ls = LOG_OFF;
		}
	} else {
		struct thread_log_data *tld;

		tld = tld_get_or_creat(tid);
		if (tld != NULL) {
			if (tld->lvl_mask == 0)
				ls = LOG_ON;
			tld->lvl_mask = mask;
		}
	}

	g_static_rw_lock_writer_unlock(&rwlock);

	if (ls == LOG_ON)
		mbb_log_debugv("log on");
	else if (ls == LOG_OFF)
		mbb_log_debugv("log off");
}

static void mbb_log_add_lvl_mask(mbb_log_lvl_t mask)
{
	struct thread_log_data *tld;
	enum log_status ls;
	gint tid;

	if (mask == 0)
		return;

	ls = LOG_NONE;
	tid = mbb_thread_get_tid();

	g_static_rw_lock_writer_lock(&rwlock);

	tld = tld_get_or_creat(tid);
	if (tld != NULL) {
		if (tld->lvl_mask == 0)
			ls = LOG_ON;
		tld->lvl_mask |= mask;
	}

	g_static_rw_lock_writer_unlock(&rwlock);

	if (ls == LOG_ON)
		mbb_log_debugv("log on");
}

static void mbb_log_del_lvl_mask(mbb_log_lvl_t mask)
{
	struct thread_log_data *tld;
	enum log_status ls;
	gint tid;

	if (mask == 0)
		return;

	ls = LOG_NONE;
	tid = mbb_thread_get_tid();

	g_static_rw_lock_writer_lock(&rwlock);

	tld = tld_get(tid);
	if (tld != NULL) {
		tld->lvl_mask &= ~mask;
		if (tld->lvl_mask == 0) {
			tld_del(tid);
			ls = LOG_OFF;
		}
	}

	g_static_rw_lock_writer_unlock(&rwlock);

	if (ls == LOG_OFF)
		mbb_log_debugv("log off");
}

static void log_register(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans G_GNUC_UNUSED)
{
	struct thread_log_data *tld;
	enum log_status ls;
	gint tid;

	ls = LOG_NONE;
	tid = mbb_thread_get_tid();

	g_static_rw_lock_writer_lock(&rwlock);

	tld = tld_get_or_creat(tid);
	if (tld != NULL && tld->lvl_mask == 0) {
		tld->lvl_mask = MBB_LOG_MSG;
		ls = LOG_ON;
	}

	g_static_rw_lock_writer_unlock(&rwlock);

	if (ls == LOG_ON)
		mbb_log_debugv("log on");
}

void mbb_log_unregister(void)
{
	enum log_status ls;
	gint tid;

	ls = LOG_NONE;
	tid = mbb_thread_get_tid();

	g_static_rw_lock_writer_lock(&rwlock);

	if (ht != NULL) {
		tld_del(tid);
		ls = LOG_OFF;
	}

	g_static_rw_lock_writer_unlock(&rwlock);

	if (ls == LOG_OFF)
		mbb_log_debugv("log off");
}

static void log_unregister(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans G_GNUC_UNUSED)
{
	mbb_log_unregister();
}

static void log_do_levels(XmlTag *tag, void (*func)(mbb_log_lvl_t))
{
	mbb_log_lvl_t mask;
	GSList *lvl_list;

	lvl_list = xml_tag_path_attr_list(tag, "level", "value");
	mask = log_lvl_mask(lvl_list);
	g_slist_free(lvl_list);

	func(mask);
}

static void log_set_levels(XmlTag *tag, XmlTag **ans G_GNUC_UNUSED)
{
	log_do_levels(tag, mbb_log_set_lvl_mask);
}

static void log_add_levels(XmlTag *tag, XmlTag **ans G_GNUC_UNUSED)
{
	log_do_levels(tag, mbb_log_add_lvl_mask);
}

static void log_del_levels(XmlTag *tag, XmlTag **ans G_GNUC_UNUSED)
{
	log_do_levels(tag, mbb_log_del_lvl_mask);
}

static void log_show_levels(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	DEFINE_XTV(XTV_SESSION_SID_);

	struct thread_log_data *tld;
	gint tid = -1;

	MBB_XTV_CALL(&tid);

	if (tid < 0)
		tid = mbb_thread_get_tid();
	else {
		if (MBB_CAP_IS_WHEEL(mbb_thread_get_cap()) == FALSE) final
			*ans = mbb_xml_msg(MBB_MSG_ROOT_ONLY);
	}

	*ans = mbb_xml_msg_ok();

	g_static_rw_lock_reader_lock(&rwlock);

	tld = tld_get(tid);
	if (tld != NULL) {
		GSList *lvl_list;
		GSList *list;

		lvl_list = log_lvl_names(tld->lvl_mask);
		for (list = lvl_list; list != NULL; list = list->next)
			xml_tag_new_child(*ans, "level",
				"value", variant_new_static_string(list->data)
			);

		g_slist_free(lvl_list);
	}

	g_static_rw_lock_reader_unlock(&rwlock);
}

static void log_push(XmlTag *tag, XmlTag **ans G_GNUC_UNUSED)
{
	Variant *var;

	var = xml_tag_path_attr(tag, "message", "value");
	if (var != NULL)
		mbb_log_lvl(MBB_LOG_USER, "%s", variant_get_string(var));
}

static void log_showall(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans G_GNUC_UNUSED)
{
	guint n;

	*ans = mbb_xml_msg_ok();

	for (n = 0; n < NELEM(log_levels); n++)
		xml_tag_new_child(*ans, "level",
			"value", variant_new_static_string(log_levels[n].name)
		);
}

static void log_trace_show(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	struct thread_log_data *tld;
	gint tid;

	tid = mbb_thread_get_tid();
	*ans = mbb_xml_msg_ok();

	g_static_rw_lock_reader_lock(&rwlock);

	tld = tld_get(tid);
	if (tld != NULL) {
		GSList *list;
		XmlTag *xt;
		gint sid;

		for (list = tld->traced; list != NULL; list = list->next) {
			sid = GPOINTER_TO_INT(list->data);
			xt = xml_tag_new_child(*ans, "session",
				"sid", variant_new_int(sid)
			);

			if (mbb_session_has(sid) == FALSE)
				xml_tag_set_attr(xt,
					"zombie", variant_new_static_string("true")
				);
		}
	}

	g_static_rw_lock_reader_unlock(&rwlock);
}

static GSList *get_sid_list(GSList *var_list)
{
	GSList *sid_list;
	gpointer data;
	GSList *list;
	gchar *str;
	gint sid;

	sid_list = NULL;
	for (list = var_list; list != NULL; list = list->next) {
		str = variant_get_string(list->data);
		if (! strcmp(str, "self"))
			sid = mbb_thread_get_tid();
		else {
			sid = str_conv_to_uint64(str, NULL);
			if (sid == 0)
				continue;

			if (mbb_session_has(sid) == FALSE)
				continue;
		}

		data = GINT_TO_POINTER(sid);
		if (g_slist_find(sid_list, data) == FALSE)
			sid_list = g_slist_prepend(sid_list, data);
	}

	return sid_list;
}

static void log_trace_add(XmlTag *tag, XmlTag **ans G_GNUC_UNUSED)
{
	struct thread_log_data *tld;
	GSList *var_list;
	GSList *sid_list;
	GSList *list;
	gint tid;

	var_list = xml_tag_path_attr_list(tag, "sid", "value");
	if (var_list == NULL)
		return;

	tid = mbb_thread_get_tid();
	sid_list = get_sid_list(var_list);
	g_slist_free(var_list);

	if (sid_list == NULL)
		return;

	g_static_rw_lock_writer_lock(&rwlock);

	tld = tld_get_or_creat(tid);
	if (tld != NULL) {
		if (tld->lvl_mask == 0)
			tld->lvl_mask = MBB_LOG_MSG;

		for (list = sid_list; list != NULL; list = list->next) {
			if (g_slist_find(tld->traced, list->data))
				continue;

			tld->traced = g_slist_prepend(tld->traced, list->data);
		}
	}

	g_static_rw_lock_writer_unlock(&rwlock);

	g_slist_free(sid_list);
}

static void log_trace_del(XmlTag *tag, XmlTag **ans G_GNUC_UNUSED)
{
	struct thread_log_data *tld;
	GSList *var_list;
	gint tid;

	var_list = xml_tag_path_attr_list(tag, "sid", "value");
	if (var_list == NULL)
		return;

	tid = mbb_thread_get_tid();

	g_static_rw_lock_writer_lock(&rwlock);

	tld = tld_get(tid);
	if (tld != NULL) {
		GSList *sid_list;

		sid_list = get_sid_list(var_list);
		if (sid_list != NULL) {
			GSList *list;

			for (list = sid_list; list != NULL; list = list->next)
				tld->traced = g_slist_remove(
					tld->traced, list->data
				);
		}

		g_slist_free(sid_list);
	}

	g_static_rw_lock_writer_unlock(&rwlock);

	g_slist_free(var_list);
}

static void log_trace_zclean(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans G_GNUC_UNUSED)
{
	struct thread_log_data *tld;
	gint tid;

	tid = mbb_thread_get_tid();

	g_static_rw_lock_writer_lock(&rwlock);

	tld = tld_get(tid);
	if (tld != NULL && tld->traced != NULL) {
		GSList *list;
		GSList *next;
		gint sid;

		list = tld->traced;
		while (list != NULL) {
			next = list->next;
			sid = GPOINTER_TO_INT(list->data);

			if (mbb_session_has(sid) == FALSE)
				tld->traced = g_slist_delete_link(
					tld->traced, list
				);

			list = next;
		}
	}

	g_static_rw_lock_writer_unlock(&rwlock);
}

static void log_trace_clean(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans G_GNUC_UNUSED)
{
	struct thread_log_data *tld;
	gint tid;

	tid = mbb_thread_get_tid();

	g_static_rw_lock_writer_lock(&rwlock);

	tld = tld_get(tid);
	if (tld != NULL) {
		g_slist_free(tld->traced);
		tld->traced = NULL;
	}

	g_static_rw_lock_writer_unlock(&rwlock);
}

MBB_FUNC_REGISTER_STRUCT("mbb-log-on", log_register, MBB_CAP_LOG);
MBB_FUNC_REGISTER_STRUCT("mbb-log-off", log_unregister, MBB_CAP_LOG);

MBB_FUNC_REGISTER_STRUCT("mbb-log-set-levels", log_set_levels, MBB_CAP_LOG);
MBB_FUNC_REGISTER_STRUCT("mbb-log-add-levels", log_add_levels, MBB_CAP_LOG);
MBB_FUNC_REGISTER_STRUCT("mbb-log-del-levels", log_del_levels, MBB_CAP_LOG);
MBB_FUNC_REGISTER_STRUCT("mbb-log-push", log_push, MBB_CAP_LOG);

MBB_FUNC_REGISTER_STRUCT("mbb-log-show-levels", log_show_levels, MBB_CAP_LOG);
MBB_FUNC_REGISTER_STRUCT("mbb-server-log-showall", log_showall, MBB_CAP_LOG);

MBB_FUNC_REGISTER_STRUCT("mbb-log-trace-show", log_trace_show, MBB_CAP_LOG);
MBB_FUNC_REGISTER_STRUCT("mbb-log-trace-add", log_trace_add, MBB_CAP_LOG);
MBB_FUNC_REGISTER_STRUCT("mbb-log-trace-del", log_trace_del, MBB_CAP_LOG);
MBB_FUNC_REGISTER_STRUCT("mbb-log-trace-clean", log_trace_clean, MBB_CAP_LOG);
MBB_FUNC_REGISTER_STRUCT("mbb-log-trace-zclean", log_trace_zclean, MBB_CAP_LOG);

static void __init init(void)
{
	static struct mbb_init_struct entries[] = {
		MBB_INIT_FUNC_STRUCT(log_register),
		MBB_INIT_FUNC_STRUCT(log_unregister),
		MBB_INIT_FUNC_STRUCT(log_set_levels),
		MBB_INIT_FUNC_STRUCT(log_add_levels),
		MBB_INIT_FUNC_STRUCT(log_del_levels),
		MBB_INIT_FUNC_STRUCT(log_show_levels),
		MBB_INIT_FUNC_STRUCT(log_push),
		MBB_INIT_FUNC_STRUCT(log_showall),
		MBB_INIT_FUNC_STRUCT(log_trace_add),
		MBB_INIT_FUNC_STRUCT(log_trace_del),
		MBB_INIT_FUNC_STRUCT(log_trace_clean),
		MBB_INIT_FUNC_STRUCT(log_trace_zclean),
		MBB_INIT_FUNC_STRUCT(log_trace_show)
	};

	mbb_init_pushv(entries, NELEM(entries));
}

