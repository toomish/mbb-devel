#ifndef MBB_LOG_H
#define MBB_LOG_H

#include <glib.h>
#include <stdarg.h>

#include "macros.h"

typedef enum {
	MBB_LOG_MSG		= 1,
	MBB_LOG_XML		= 1 << 1,
	MBB_LOG_HTTP		= 1 << 2,
	MBB_LOG_QUERY		= 1 << 3,

	MBB_LOG_DEBUG		= 1 << 10,
	MBB_LOG_DEBUGV		= 1 << 11,

	MBB_LOG_USER		= 1 << 20,	/* user messages */
	MBB_LOG_SELF		= 1 << 21,	/* self log messages */
	MBB_LOG_TASK		= 1 << 22,	/* task messages */

	MBB_LOG_NONE		= 1 << 31	/* non-existent level */
} mbb_log_lvl_t;

enum {
	LOG_MASK_ADD,
	LOG_MASK_DEL,
	LOG_MASK_SET
};

#define mbb_log_debugv(...) mbb_log_lvl(MBB_LOG_DEBUGV, __VA_ARGS__)
#define mbb_log_debug(...) mbb_log_lvl(MBB_LOG_DEBUG, __VA_ARGS__)
#define mbb_log_self(...) mbb_log_lvl(MBB_LOG_SELF, __VA_ARGS__)

void mbb_log(gchar *fmt, ...);
void mbb_log_lvl(mbb_log_lvl_t lvl, gchar *fmt, ...);
void mbb_log_unregister(void);

void mbb_log_mask(gint how, mbb_log_lvl_t mask, mbb_log_lvl_t *omask);

#endif
