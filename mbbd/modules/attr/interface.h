/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef ATTR_INTERFACE_H
#define ATTR_INTERFACE_H

#include <glib.h>

struct attrvec {
	gchar *name;
	gchar *value;
};

struct attr_interface {
	gint (*attr_readv)(gchar *, gint, struct attrvec *, gsize, GError **);
};

#endif
