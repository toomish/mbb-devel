#include <stdarg.h>
#include <time.h>

#include "query.h"
#include "debug.h"

struct vararg {
	va_list ap;
};

#define vararg_init(v, ap) va_copy(v.ap, ap)
#define vararg_start(v, last) va_start(v.ap, last)
#define vararg_end(v) va_end(v.ap)

static gchar *(*query_escape_func)(gchar *);

static void buf_free(GString *buf)
{
	g_string_free(buf, TRUE);
}

static GString *getbuf(void)
{
	static GStaticPrivate buf_key = G_STATIC_PRIVATE_INIT;
	GString *buf;

	buf = g_static_private_get(&buf_key);
	if (buf == NULL) {
		buf = g_string_new(NULL);
		g_static_private_set(&buf_key, buf, (GDestroyNotify) buf_free);
	}

	return buf;
}

static inline gchar *query_escape_text(gchar *text)
{
	gchar *str;

	str = text;
	if (query_escape_func != NULL) {
		if ((str = query_escape_func(text)) == NULL)
			str = text;
	}

	return str;
}

gchar *query_escape(gchar *str)
{
	return query_escape_text(str);
}

void query_set_escape(gchar *(*escape_func)(gchar *))
{
	query_escape_func = escape_func;
}

static GString *select_common(gchar *table, struct vararg *va)
{
	GString *buf;
	gchar *arg;

	buf = getbuf();

	g_string_assign(buf, "select ");
	arg = va_arg(va->ap,  gchar *);
	if (arg == NULL)
		g_string_append_c(buf, '*');
	else {
		g_string_append(buf, arg);

		while ((arg = va_arg(va->ap, gchar *)) != NULL)
			g_string_append_printf(buf, ", %s", arg);
	}

	if (table != NULL)
		g_string_append_printf(buf, " from %s", table);

	return buf;
}

gchar *query_select_full(gchar *table, gchar *opt, ...)
{
	struct vararg va;
	GString *buf;

	vararg_start(va, opt);
	buf = select_common(table, &va);
	vararg_end(va);

	if (opt != NULL)
		g_string_append_printf(buf, " where %s", opt);
	g_string_append_c(buf, ';');

	return buf->str;
}

gchar *query_select_ap(gchar *table, va_list ap)
{
	struct vararg va;
	GString *buf;
	gchar *fmt;

	vararg_init(va, ap);
	buf = select_common(table, &va);
	fmt = va_arg(va.ap, gchar *);

	if (fmt != NULL) {
		g_string_append(buf, " where ");
		g_string_append_vprintf(buf, fmt, va.ap);
	}

	g_string_append_c(buf, ';');
	vararg_end(va);

	return buf->str;
}

static gboolean qfmt(GString *buf, gchar fmt, struct vararg *va)
{
	gchar *arg;

	switch (fmt) {
	case 'S':
		arg = va_arg(va->ap, gchar *);
		if (arg == NULL)
			arg = "NULL";
		g_string_append(buf, arg);
		break;
	case 's':
		arg = va_arg(va->ap, gchar *);
		if (arg == NULL)
			g_string_append(buf, "NULL");
		else {
			gchar *tmp;

			tmp = query_escape_text(arg);
			g_string_append_printf(buf, "'%s'", tmp);

			if (tmp != arg)
				g_free(tmp);
		} break;
	case 't': {
		time_t t;

		t = va_arg(va->ap, time_t);
		if (t >= 0)
			g_string_append_printf(buf, "%ld", t);
		else
			g_string_append(buf, "NULL");
	}	break;
	case 'l':
		g_string_append_printf(buf, "%ld", va_arg(va->ap, glong));
		break;
	case 'q':
		g_string_append_printf(buf,
			"%" G_GUINT64_FORMAT, va_arg(va->ap, guint64)
		);
		break;
	case 'd':
		g_string_append_printf(buf, "%d", va_arg(va->ap, gint));
		break;
	case 'c': {
		gchar str[2] = { (gchar) va_arg(va->ap, gint), '\0' };
		gchar *tmp;

		tmp = query_escape_text(str);
		g_string_append_printf(buf, "'%s'", tmp);

		if (tmp != str)
			g_free(tmp);
	}	break;
	case 'b':
		g_string_append_printf(
			buf, "%s", va_arg(va->ap, gint) ? "true" : "false"
		);
		break;
	default:
		msg_warn("invalid format char '%c'", fmt);
		return FALSE;
	}

	return TRUE;
}

static gboolean buf_append_arg(GString *buf, gchar **pp, struct vararg *va,
			       gchar *str)
{
	if (pp == NULL || *pp == NULL) {
		gchar *arg;

		arg = va_arg(va->ap, gchar *);
		if (pp != NULL && arg == NULL)
			return FALSE;

		if (str != NULL)
			g_string_append(buf, str);

		if (arg == NULL)
			g_string_append(buf, "NULL");
		else {
			gchar *tmp;

			tmp = query_escape_text(arg);
			g_string_append_printf(buf, "'%s'", tmp);

			if (tmp != arg)
				g_free(tmp);
		}

		return TRUE;
	}

	if (**pp == '\0')
		return FALSE;

	if (str != NULL)
		g_string_append(buf, str);

	qfmt(buf, **pp, va);
	++*pp;

	return TRUE;
}

gchar *query_insert_full(gchar *table, gchar *fmt, ...)
{
	struct vararg va;
	GString *buf;
	gchar *arg;
	gchar *p;

	p = fmt;
	buf = getbuf();

	g_string_printf(buf, "insert into %s ", table);

	vararg_start(va, fmt);
	arg = va_arg(va.ap, gchar *);
	if (arg != NULL) {
		g_string_append_printf(buf, "(%s", arg);

		while ((arg = va_arg(va.ap, gchar *)) != NULL)
			g_string_append_printf(buf, ", %s", arg);
		
		g_string_append(buf, ") ");
	}

	g_string_append(buf, "values (");
	if (buf_append_arg(buf, &fmt, &va, NULL))
		while (buf_append_arg(buf, &fmt, &va, ", "));

	g_string_append(buf, ");");
	vararg_end(va);

	return buf->str;
}

gchar *query_insert_ap(gchar *table, gchar *field, gchar *fmt, va_list ap)
{
	struct vararg va;
	gchar *comma = NULL;
	GString *values;
	GString *buf;
	gchar *arg;

	values = g_string_new(NULL);
	buf = getbuf();

	vararg_init(va, ap);

	g_string_printf(buf, "insert into %s (", table);
	while (fmt == NULL || *fmt != '\0') {
		if ((arg = va_arg(va.ap, gchar *)) == NULL)
			break;

		if (comma != NULL)
			g_string_append(buf, ", ");

		g_string_append_printf(buf, "%s", arg);
		if (! buf_append_arg(values, &fmt, &va, comma))
			break;

		comma = ", ";
	}

	g_string_append_printf(buf, ") values (%s)", values->str);
	g_string_free(values, TRUE);

	if (field == NULL)
		g_string_append_c(buf, ';');
	else
		g_string_append_printf(buf, " returning %s;", field);

	vararg_end(va);

	return buf->str;
}

static inline void query_delete_init(GString *buf, gchar *table)
{
	g_string_printf(buf, "delete from %s", table);
}

gchar *query_delete(gchar *table, gchar *opt)
{
	GString *buf;

	buf = getbuf();

	query_delete_init(buf, table);
	if (opt != NULL)
		g_string_append_printf(buf, " where %s", opt);
	g_string_append_c(buf, ';');

	return buf->str;
}

gchar *query_delete_ap(gchar *table, gchar *fmt, va_list ap)
{
	GString *buf;

	buf = getbuf();

	query_delete_init(buf, table);
	if (fmt != NULL) {
		g_string_append(buf, " where ");
		g_string_append_vprintf(buf, fmt, ap);
	}

	g_string_append_c(buf, ';');

	return buf->str;
}

static GString *update_common(gchar *table, gchar *fmt, gchar *name,
			      struct vararg *va)
{
	GString *buf;
	gchar *arg;
	gchar **pp;

	pp = fmt != NULL ? &fmt : NULL;
	buf = getbuf();

	g_string_printf(buf, "update %s set %s = ", table, name);

	for (;;) {
		buf_append_arg(buf, pp, va, NULL);

		if (pp != NULL && **pp == '\0')
			break;

		if ((arg = va_arg(va->ap, gchar *)) == NULL)
			break;

		g_string_append_printf(buf, ", %s = ", arg);
	}

	return buf;
}

gchar *query_update_full(gchar *table, gchar *fmt, gchar *opt, gchar *name, ...)
{
	struct vararg va;
	GString *buf;

	vararg_start(va, name);
	buf = update_common(table, fmt, name, &va);
	vararg_end(va);

	if (opt != NULL)
		g_string_append_printf(buf, " where %s", opt);

	g_string_append_c(buf, ';');

	return buf->str;
}

gchar *query_update_ap(gchar *table, gchar *fmt, gchar *name, va_list ap)
{
	struct vararg va;
	GString *buf;
	gchar *arg;

	vararg_init(va, ap);
	buf = update_common(table, fmt, name, &va);
	if ((arg = va_arg(va.ap, gchar *)) != NULL) {
		g_string_append(buf, " where ");
		g_string_append_vprintf(buf, arg, va.ap);
	}
	g_string_append_c(buf, ';');
	vararg_end(va);

	return buf->str;
}

gchar *query_function(gchar *func_name, gchar *fmt, ...)
{
	va_list ap;
	gchar *s;

	va_start(ap, fmt);
	s = query_function_ap(func_name, fmt, ap);
	va_end(ap);

	return s;
}

gchar *query_function_ap(gchar *func_name, gchar *fmt, va_list ap)
{
	struct vararg va;
	GString *buf;

	buf = getbuf();

	vararg_init(va, ap);
	g_string_printf(buf, "select %s(", func_name);
	if (buf_append_arg(buf, &fmt, &va, NULL))
		while (buf_append_arg(buf, &fmt, &va, ", "));
	g_string_append(buf, ");");
	vararg_end(va);

	return buf->str;
}

gchar *query_format_full(gboolean append, gchar *fmt, ...)
{
	struct vararg va;
	GString *buf;

	buf = getbuf();
	if (append == FALSE)
		g_string_truncate(buf, 0);

	if (fmt == NULL)
		return buf->str;

	vararg_start(va, fmt);
	for (; *fmt != '\0'; fmt++) {
		if (*fmt != '%') {
			g_string_append_c(buf, *fmt);
			continue;
		}

		++fmt;

		if (*fmt == '\0')
			break;
		else if (*fmt == '%')
			g_string_append_c(buf, '%');
		else
			qfmt(buf, *fmt, &va);
	}
	vararg_end(va);

	return buf->str;
}

