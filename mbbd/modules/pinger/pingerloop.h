/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef PINGER_LOOP_H
#define PINGER_LOOP_H

#include <glib.h>

gboolean pinger_loop_init(void);
void pinger_loop_end(void);

#endif
