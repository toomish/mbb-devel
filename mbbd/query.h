#ifndef QUERY_H
#define QUERY_H

#include <glib.h>

#include "macros.h"

#define query_select(table, arg, ...) query_select_full(table, arg, ## __VA_ARGS__, NULL)
#define query_insert(table, arg, ...) query_insert_full(table, NULL, arg, __VA_ARGS__, NULL)
#define query_update(table, arg, n, ...) \
	query_update_full(table, NULL, arg, n, ## __VA_ARGS__, NULL)

#define query_insertf(table, fmt, arg, ...) query_insert_full(table, fmt, arg, __VA_ARGS__, NULL)
#define query_updatef(table, fmt, arg, ...) \
	query_update_full(table, fmt, arg, ## __VA_ARGS__, NULL)

#define query_format(fmt, ...) query_format_full(FALSE, fmt, ## __VA_ARGS__)
#define query_init(txt) query_format_full(FALSE, "%S", txt)
#define query_append_format(fmt, ...) query_format_full(TRUE, fmt, ## __VA_ARGS__)
#define query_append(txt) query_format_full(TRUE, "%S", txt)

typedef gchar *(*query_list_func_t)(gpointer);

void query_set_escape(gchar *(*escape_func)(gchar *));
gchar *query_escape(gchar *str);

gchar *query_select_full(gchar *table, gchar *opt, ...) __sentinel(0);
gchar *query_insert_full(gchar *table, gchar *fmt, ...) __sentinel(0);
gchar *query_insert_ap(gchar *table, gchar *field, gchar *fmt, va_list ap);
gchar *query_update_full(gchar *table, gchar *fmt, gchar *opt,
			 gchar *name, ...) __sentinel(0);
gchar *query_update_ap(gchar *table, gchar *fmt, gchar *name, va_list ap);

gchar *query_delete(gchar *table, gchar *opt);
gchar *query_delete_ap(gchar *table, gchar *fmt, va_list ap);
gchar *query_select_ap(gchar *table, va_list ap);

gchar *query_function(gchar *func_name, gchar *fmt, ...);
gchar *query_function_ap(gchar *func_name, gchar *fmt, va_list ap);

gchar *query_format_full(gboolean append, gchar *fmt, ...);

#endif
