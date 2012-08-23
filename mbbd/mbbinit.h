/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_INIT_H
#define MBB_INIT_H

#include "macros.h"

#include <glib.h>

struct mbb_init_struct {
	void (*init_func)(gpointer data);
	gpointer data;
	struct mbb_init_struct *next;
};

#define MBB_INIT_STRUCT(func, data) { (void (*)(gpointer)) func, data, NULL }
#define MBB_INIT_VARS MBB_INIT_STRUCT(init_vars, NULL)
#define MBB_INIT_LOCAL MBB_INIT_STRUCT(init_local, NULL)

#define MBB_ON_INIT(...) \
static void __init init(void) \
{ \
	static struct mbb_init_struct entries[] = { \
		__VA_ARGS__ \
	}; \
\
	mbb_init_pushv(entries); \
}

#define mbb_init_pushv(table) mbb_init_push(table, NELEM(table))

void mbb_init_push(struct mbb_init_struct *init_struct, gsize count);
void mbb_init(void);
gboolean mbb_initialized(void);

#endif
