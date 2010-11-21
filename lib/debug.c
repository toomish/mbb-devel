#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "debug.h"

#ifndef NOCOLOR
#define GREEN	"[32m"
#define DEF	"[0m"
#define RED 	"[31m"
#else
#define GREEN
#define DEF
#define RED
#endif

#define PROMPT_FMT GREEN "%s" DEF ":" GREEN "%s" DEF ":" GREEN "%d" DEF

static inline const char *path_basename(const char *path)
{
	char *s;

	if ((s = strrchr(path, '/')) == NULL)
		return path;

	return s + 1;
}

void debug_print_warn(char *file, const char *func, int line, char *fmt, ...)
{
	gchar *s1, *s2;
	va_list ap;

	va_start(ap, fmt);
	s1 = g_strdup_printf(PROMPT_FMT, path_basename(file), func, line);
	s2 = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	g_printerr("%s: %s\n", s1, s2);
	g_free(s1);
	g_free(s2);
}

void debug_print_err(char *file, const char *func, int line, char *fmt, ...)
{
	gchar *s1, *s2;
	va_list ap;

	va_start(ap, fmt);
	s1 = g_strdup_printf(PROMPT_FMT, path_basename(file), func, line);
	s2 = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	g_printerr("%s: %s: %s\n", s1, s2, g_strerror(errno));
	g_free(s1);
	g_free(s2);
}

