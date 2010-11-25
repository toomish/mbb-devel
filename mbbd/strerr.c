/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <string.h>

#include "strerr.h"

G_LOCK_DEFINE_STATIC(g_strerror);

gchar *strerr(gint errnum)
{
	gchar *msg;

	G_LOCK(g_strerror);
	msg = g_strdup(g_strerror(errnum));
	G_UNLOCK(g_strerror);

	return msg;
}

