/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef STR_CONV_H
#define STR_CONV_H

#include <glib.h>

typedef enum {
	STR_CONV_ERROR_OVERFLOW,
	STR_CONV_ERROR_INVALID_BASE,
	STR_CONV_ERROR_FAIL,
	STR_CONV_ERROR_EXTRA_SYMBOLS
} StrConvError;

#define STR_CONV_ERROR (str_conv_error_quark())

GQuark str_conv_error_quark(void);

gint64 str_conv_to_int64(const gchar *str, GError **error);
guint64 str_conv_to_uint64(const gchar *str, GError **error);
guint str_conv_to_mask(const gchar *str, GError **error);

#endif
