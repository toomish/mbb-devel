/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#define _XOPEN_SOURCE

#include "mbbtime.h"
#include "varconv.h"
#include "strconv.h"
#include "macros.h"
#include "debug.h"
#include "inet.h"
#include "re.h"

#include <string.h>

static gchar *dtformats[] = {
        "%Y-%m-%d %H:%M:%S",
        "%Y-%m-%d %H:%M",
        "%Y-%m-%d %H",
        "%Y-%m-%d",
        "%Y-%m",
	"%Y",
        NULL
};

gboolean var_conv_str(gchar *arg, gpointer p)
{
	*(gchar **) p = arg;
	return TRUE;
}

gboolean var_conv_dir(gchar *arg, gpointer p)
{
	gchar **dest = (gchar **) p;
	gsize len;

	if ((len = strlen(arg)) == 0)
		return FALSE;

	if (*arg != '/' || (len > 1 && arg[len - 1] != '/'))
		return FALSE;

	g_free(*dest);
	*dest = g_strdup(arg);

	return TRUE;
}

gboolean var_conv_legal(gchar *arg, gpointer p)
{
	gunichar uc;
	gchar *s;

	for (s = arg; *s != '\0'; s = g_utf8_next_char(s)) {
		uc = g_utf8_get_char(s);
		if (! g_unichar_isalnum(uc))
			return FALSE;
	}

	*(gchar **) p = arg;
	return TRUE;
}

gboolean var_conv_dup(gchar *arg, gpointer p)
{
	g_free(*(gchar **) p);
	*(gchar **) p = g_strdup(arg);

	return TRUE;
}

gboolean var_conv_int(gchar *arg, gpointer p)
{
	GError *error = NULL;
	gint val;

	val = str_conv_to_int64(arg, &error);
	if (error != NULL) {
		g_error_free(error);
		return FALSE;
	}

	if (val > G_MAXINT || val < G_MININT)
		return FALSE;

	*(gint *) p = val;
	return TRUE;
}

gboolean var_conv_uint(gchar *arg, gpointer p)
{
	GError *error = NULL;
	guint val;

	val = str_conv_to_uint64(arg, &error);
	if (error != NULL) {
		g_error_free(error);
		return FALSE;
	}

	if (val > G_MAXUINT)
		return FALSE;

	*(guint *) p = val;
	return TRUE;
}

gboolean var_conv_utime(gchar *arg, gpointer p)
{
	GError *error = NULL;
	guint64 val;

	val = str_conv_to_uint64(arg, &error);
	if (error != NULL) {
		g_error_free(error);
		return FALSE;
	}

	if (val > G_MAXLONG)
		return FALSE;

	*(time_t *) p = val;
	return TRUE;
}

gboolean var_conv_time(gchar *str, gpointer p)
{
        struct tm tm;
        gchar **fmt;
	time_t *t;

	t = (time_t *) p;

        if (!strcmp(str, "now")) {
                time(t);
                return TRUE;
        }

        for (fmt = dtformats; *fmt != NULL; fmt++) {
		gchar *s;

                memset(&tm, 0, sizeof(tm));
                s = strptime(str, *fmt, &tm);
                if (s != NULL && *s == '\0')
                        break;
        }

        if (*fmt == NULL)
                return FALSE;

        if (tm.tm_mday == 0)
                tm.tm_mday++;

        if (!g_date_valid_dmy(tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900))
                return FALSE;

        tm.tm_isdst = -1;
        *t = mktime(&tm);

        return TRUE;
}

gboolean var_conv_etime(gchar *str, gpointer p)
{
	time_t *t;

	t = (time_t *) p;

        if (!strcmp(str, "parent")) {
                *t = VAR_CONV_TIME_PARENT;
                return TRUE;
        } else if (!strcmp(str, "null") || !strcmp(str, "inf")) {
                *t = VAR_CONV_TIME_NULL;
                return TRUE;
	}

	return var_conv_time(str, p);
}

gboolean var_conv_bool(gchar *arg, gpointer p)
{
	if (!strcmp(arg, "true"))
		*(gboolean *) p = TRUE;
	else if (!strcmp(arg, "false"))
		*(gboolean *) p = FALSE;
	else
		return FALSE;

	return TRUE;
}

gboolean var_conv_inet(gchar *arg, gpointer p)
{
	return atoinet(arg, p) != NULL;
}

gboolean var_conv_ipv4(gchar *arg, gpointer p)
{
	struct inet inet;

	if (atoipv4(arg, &inet) == NULL)
		return FALSE;

	*(ipv4_t *) p = inet.addr;

	return TRUE;
}

gboolean var_conv_regex(gchar *arg, gpointer p)
{
	Regex re;

	re = re_new(arg);
	if (re == NULL)
		return FALSE;

	*(Regex *) p = re;

	return TRUE;
}

gchar *var_str_str(gpointer p)
{
	gchar *s = *(gchar **) p;

	if (s == NULL) s = "(dummy)";

	return g_strdup(s);
}

gchar *var_str_int(gpointer p)
{
	return g_strdup_printf("%d", *(gint *) p);
}

gchar *var_str_uint(gpointer p)
{
	return g_strdup_printf("%u", *(gint *) p);
}

gchar *var_str_time(gpointer p)
{
	time_buf_t buf;
	
	timetoa(buf, *(time_t *) p);

	return g_strdup(buf);
}

gchar *var_str_bool(gpointer p)
{
	gchar *s;

	s = *(gboolean *) p ? "true" : "false";

	return g_strdup(s);
}

gchar *var_str_inet(gpointer p)
{
	return inettoa(NULL, (struct inet *) p);
}

gchar *var_str_ipv4(gpointer p)
{
	return ipv4toa(NULL, *(ipv4_t *) p);
}

