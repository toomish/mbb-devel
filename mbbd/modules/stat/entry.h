/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_STAT_ENTRY_H
#define MBB_STAT_ENTRY_H

struct mbb_stat_entry {
	guint unit_id;
	guint link_id;

	guint64 nbyte_in;
	guint64 nbyte_out;

	time_t point;
};

#endif
