#ifndef MBB_STATMAN_H
#define MBB_STATMAN_H

#include "xmltag.h"

#include <time.h>

gboolean mbb_statman_time_args_parse(time_t *start, time_t *end,
				     gchar *period, gchar *step,
				     XmlTag **ans);

#endif
