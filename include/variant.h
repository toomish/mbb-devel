/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef VARIANT_H
#define VARIANT_H

#include <glib.h>

typedef struct variant Variant;

void variant_free(Variant *var);

Variant *variant_new_int(gint value);
Variant *variant_new_long(glong value);
Variant *variant_new_string(gchar *str);
Variant *variant_new_alloc_string(gchar *str);
Variant *variant_new_static_string(gchar *str);
Variant *variant_new_pointer(gpointer *data, gchar *(*convert)(gpointer *),
			     void (*free)(gpointer *));

gboolean variant_is_int(Variant *var);
gboolean variant_is_long(Variant *var);
gboolean variant_is_string(Variant *var);
gboolean variant_is_pointer(Variant *var);

gint variant_get_int(Variant *var);
glong variant_get_long(Variant *var);
gchar *variant_get_string(Variant *var);
gpointer *variant_get_pointer(Variant *var);

gchar *variant_to_string(Variant *var);

#endif
