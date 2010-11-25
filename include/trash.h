/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_TRASH_H
#define MBB_TRASH_H

#include <glib.h>

typedef struct {
	GSList *list;
} Trash;

#define TRASH_INITIALIZER { NULL }

Trash *trash_new(void);
void trash_push(Trash *trash, gpointer data, GDestroyNotify destroy);
void trash_empty(Trash *trash);
void trash_release_data(Trash *trash);
void trash_free(Trash *trash);

#endif
