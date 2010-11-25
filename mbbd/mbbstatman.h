/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_STATMAN_H
#define MBB_STATMAN_H

#include "xmltag.h"

#include <time.h>

gboolean mbb_statman_time_args_parse(time_t *start, time_t *end,
				     gchar *period, gchar *step,
				     XmlTag **ans);

#endif
