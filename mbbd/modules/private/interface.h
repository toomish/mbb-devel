/* Copyright (C) 2012 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef PRIVATE_INTERFACE_H
#define PRIVATE_INTERFACE_H

#include <glib.h>

struct private_interface {
	void (*init)(gpointer key, GDestroyNotify notify);
	gboolean (*set)(gpointer key, gpointer data);
	gpointer (*get)(gpointer key);
	void (*free)(gpointer key);
	void (*remove)(gpointer key);
};

#endif
