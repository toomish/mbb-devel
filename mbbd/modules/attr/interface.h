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
