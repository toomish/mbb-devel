/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_INIT_H
#define MBB_INIT_H

#include <glib.h>

struct mbb_init_struct {
	void (*init_func)(gpointer data);
	gpointer data;
	struct mbb_init_struct *next;
};

#define MBB_INIT_STRUCT(func, data) { (void (*)(gpointer)) func, data, NULL }
#define MBB_INIT_VARS MBB_INIT_STRUCT(init_vars, NULL)

void mbb_init_push(struct mbb_init_struct *init_struct);
void mbb_init_pushv(struct mbb_init_struct *p, gsize count);
void mbb_init(void);
gboolean mbb_initialized(void);

#endif
