/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef SHARED_H
#define SHARED_H

#include <glib.h>

typedef struct {
	gpointer data;
} Shared;

#define shared_new(data, destroy) shared_new_full(data, destroy, 1)

Shared *shared_new_full(gpointer data, GDestroyNotify destroy, gint ref_count);
Shared *shared_ref(Shared *shar);
void shared_unref(Shared *shar);
void shared_free(Shared *shar);

#endif
