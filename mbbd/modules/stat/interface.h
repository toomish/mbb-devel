/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef STAT_INTERFACE_H
#define STAT_INTERFACE_H

#include <glib.h>
#include <time.h>

#include "xmltag.h"
#include "entry.h"

struct mbb_stat_pool;

struct stat_interface {
	gboolean (*pool_init)(void);

	struct mbb_stat_pool *(*pool_new)(void);
	void (*pool_feed)(struct mbb_stat_pool *, struct mbb_stat_entry *);
	void (*pool_save)(struct mbb_stat_pool *);
	void (*pool_free)(struct mbb_stat_pool *);

	gboolean (*db_wipe)(time_t, time_t, GError **);

	gboolean (*parse_tag)(time_t *, time_t *, gchar *, gchar *, XmlTag **);
};

#endif
