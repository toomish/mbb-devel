#ifndef VAR_CONV_H
#define VAR_CONV_H

#include <glib.h>

typedef gboolean (*var_conv_func_t)(gchar *arg, gpointer p);

enum {
	VAR_CONV_TIME_UNSET = -2,
	VAR_CONV_TIME_PARENT = -1,
	VAR_CONV_TIME_NULL = 0
};

#define var_conv_null var_conv_str

gboolean var_conv_str(gchar *arg, gpointer p);
gboolean var_conv_dir(gchar *arg, gpointer p);
gboolean var_conv_legal(gchar *arg, gpointer p);
gboolean var_conv_dup(gchar *arg, gpointer p);
gboolean var_conv_int(gchar *arg, gpointer p);
gboolean var_conv_uint(gchar *arg, gpointer p);
gboolean var_conv_time(gchar *arg, gpointer p);
gboolean var_conv_etime(gchar *arg, gpointer p);
gboolean var_conv_utime(gchar *arg, gpointer p);
gboolean var_conv_bool(gchar *arg, gpointer p);
gboolean var_conv_inet(gchar *arg, gpointer p);
gboolean var_conv_ipv4(gchar *arg, gpointer p);
gboolean var_conv_regex(gchar *arg, gpointer p);

gchar *var_str_str(gpointer p);
gchar *var_str_int(gpointer p);
gchar *var_str_uint(gpointer p);
gchar *var_str_time(gpointer p);
gchar *var_str_bool(gpointer p);
gchar *var_str_inet(gpointer p);
gchar *var_str_ipv4(gpointer p);

#endif
