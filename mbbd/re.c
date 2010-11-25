/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <sys/types.h>
#include <regex.h>

#include "re.h"

Regex re_new(gchar *str)
{
	GRegex *re;

	re = g_regex_new(str, G_REGEX_CASELESS, 0, NULL);

	return re;
}

gboolean re_ismatch(Regex re, gchar *str)
{
	return g_regex_match(re, str, 0, NULL);
}

void re_free(Regex re)
{
	if (re != NULL) {
		g_regex_unref(re);
	}
}

