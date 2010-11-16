#ifndef MBB_TASK_H
#define MBB_TASK_H

#include <glib.h>

typedef gboolean (*mbb_task_work_t)(gpointer data);
typedef void (*mbb_task_free_t)(gpointer data);

struct mbb_task_hook {
	gboolean (*init)(gpointer data);
	void (*fini)(gpointer data);
	gboolean (*work)(gpointer data);
};

gint mbb_task_create(GQuark name, struct mbb_task_hook *hook, gpointer data);

gint mbb_task_get_id(void);
gint mbb_task_get_tid(void);
gint mbb_task_get_uid(void);
gchar *mbb_task_get_name(void);

gboolean mbb_task_poll_state(void);

#endif
