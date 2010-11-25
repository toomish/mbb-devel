/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbmsgqueue.h"

enum {
	MSG_CONST,
	MSG_ALLOC,
	MSG_SHARED
};

struct msg_entry {
	gint type;
	gsize len;
	gsize off;

	union {
		Shared *msg;
		gchar *text;
	} un;
};

struct mbb_msg_queue {
	GQueue *queue;
	GMutex *mutex;

	mbb_msg_func_t func;
	gpointer user_data;
};

MbbMsgQueue *mbb_msg_queue_new(mbb_msg_func_t func, gpointer user_data)
{
	MbbMsgQueue *msg_queue;

	msg_queue = g_new(MbbMsgQueue, 1);
	msg_queue->queue = NULL;
	msg_queue->mutex = g_mutex_new();
	msg_queue->func = func;
	msg_queue->user_data = user_data;

	return msg_queue;
}

static void mbb_msg_queue_push_entry(MbbMsgQueue *msg_queue,
				     struct msg_entry *entry)
{
	g_mutex_lock(msg_queue->mutex);

	entry->off = 0;

	if (msg_queue->queue == NULL)
		msg_queue->queue = g_queue_new();
	g_queue_push_tail(msg_queue->queue, entry);

/*
	if (entry->type == MSG_SHARED)
		msg_warn("push msg %.*s", entry->len, (char *) entry->un.msg->data);
	else
		msg_warn("push msg %.*s", entry->len, entry->un.text);
*/

	g_mutex_unlock(msg_queue->mutex);
}

void mbb_msg_queue_push_shared(MbbMsgQueue *msg_queue, Shared *msg, gsize len)
{
	struct msg_entry *entry;

	entry = g_new(struct msg_entry, 1);
	entry->type = MSG_SHARED;
	entry->len = len;
	entry->un.msg = msg;

	mbb_msg_queue_push_entry(msg_queue, entry);
}

void mbb_msg_queue_push_alloc(MbbMsgQueue *msg_queue, gchar *text, gsize len)
{
	struct msg_entry *entry;

	entry = g_new(struct msg_entry, 1);
	entry->type = MSG_ALLOC;
	entry->len = len;
	entry->un.text = text;

	mbb_msg_queue_push_entry(msg_queue, entry);
}

void mbb_msg_queue_push_const(MbbMsgQueue *msg_queue, gchar *text, gsize len)
{
	struct msg_entry *entry;

	entry = g_new(struct msg_entry, 1);
	entry->type = MSG_CONST;
	entry->len = len;
	entry->un.text = text;

	mbb_msg_queue_push_entry(msg_queue, entry);
}

static void msg_entry_free(struct msg_entry *entry)
{
	if (entry->type == MSG_ALLOC)
		g_free(entry->un.text);
	else if (entry->type == MSG_SHARED)
		shared_unref(entry->un.msg);
	g_free(entry);
}

gint mbb_msg_queue_pop(MbbMsgQueue *msg_queue)
{
	struct msg_entry *entry = NULL;
	gchar *text;
	gsize len;
	gint n;

	g_mutex_lock(msg_queue->mutex);

	if (msg_queue->queue != NULL)
		entry = g_queue_peek_head(msg_queue->queue);

	g_mutex_unlock(msg_queue->mutex);

	if (entry == NULL)
		return 0;

	if (entry->type == MSG_SHARED)
		text = entry->un.msg->data;
	else
		text = entry->un.text;

	text += entry->off;
	len = entry->len - entry->off;

/*
	msg_warn("pop msg %.*s", len, text);
*/
	n = msg_queue->func(text, len, msg_queue->user_data);

	if (n > 0) {
		if ((gsize) n < len)
			entry->off += n;
		else {
			g_mutex_lock(msg_queue->mutex);
			g_queue_pop_head(msg_queue->queue);
			g_mutex_unlock(msg_queue->mutex);
			msg_entry_free(entry);
		}
	}

	return n;
}

gboolean mbb_msg_queue_is_empty(MbbMsgQueue *msg_queue)
{
	gboolean retval = TRUE;

	g_mutex_lock(msg_queue->mutex);

	if (msg_queue->queue != NULL && msg_queue->queue->length != 0)
		retval = FALSE;

	g_mutex_unlock(msg_queue->mutex);

	return retval;
}

guint mbb_msg_queue_get_length(MbbMsgQueue *msg_queue)
{
	guint length = 0;

	g_mutex_lock(msg_queue->mutex);

	if (msg_queue->queue != NULL)
		length = msg_queue->queue->length;

	g_mutex_unlock(msg_queue->mutex);

	return length;
}

void mbb_msg_queue_free(MbbMsgQueue *msg_queue)
{
	if (msg_queue->queue != NULL) {
		g_queue_foreach(
			msg_queue->queue, (GFunc) msg_entry_free, NULL
		);
		g_queue_free(msg_queue->queue);
	}

	g_mutex_free(msg_queue->mutex);
	g_free(msg_queue);
}

