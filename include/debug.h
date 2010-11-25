/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdlib.h>
#include <glib.h>

#ifndef NDEBUG
#define DBG(code) \
do { \
	code; \
} while (0)
#define MSG_WARN(...) msg_warn(__VA_ARGS__)
#else
#define DBG(code)
#define MSG_WARN(...)
#endif

void debug_print_warn(char *file, const char *func, int line, char *fmt, ...)
	G_GNUC_PRINTF(4, 5);
void debug_print_err(char *file, const char *func, int line, char *fmt, ...)
	G_GNUC_PRINTF(4, 5);

#define msg_err(...) \
	debug_print_err(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#define msg_warn(...) \
	debug_print_warn(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#define err_sys(...) \
do { \
	msg_err(__VA_ARGS__); \
	exit(1); \
} while (0)

#define err_quit(...) \
do { \
	msg_warn(__VA_ARGS__); \
 	exit(1); \
} while (0)

#endif
