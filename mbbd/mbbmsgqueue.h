#ifndef MBB_MSG_QUEUE_H
#define MBB_MSG_QUEUE_H

#include <glib.h>

#include "shared.h"

typedef struct mbb_msg_queue MbbMsgQueue;
typedef gint (*mbb_msg_func_t)(gchar *text, gsize len, gpointer user_data);

MbbMsgQueue *mbb_msg_queue_new(mbb_msg_func_t func, gpointer user_data);
void mbb_msg_queue_push_shared(MbbMsgQueue *msg_queue, Shared *msg, gsize len);
void mbb_msg_queue_push_alloc(MbbMsgQueue *msg_queue, gchar *msg, gsize len);
void mbb_msg_queue_push_const(MbbMsgQueue *msg_queue, gchar *text, gsize len);
gint mbb_msg_queue_pop(MbbMsgQueue *msg_queue);
gboolean mbb_msg_queue_is_empty(MbbMsgQueue *msg_queue);
guint mbb_msg_queue_get_length(MbbMsgQueue *msg_queue);
void mbb_msg_queue_free(MbbMsgQueue *msg_queue);

#endif
