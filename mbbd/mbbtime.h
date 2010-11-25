/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_TIME_H
#define MBB_TIME_H

#include <glib.h>
#include <time.h>

#include "xmltag.h"

enum {
	MBB_TIME_UNSET = -2,
	MBB_TIME_PARENT = -1,
	MBB_TIME_NULL = 0
};

#define TIME_BUFSIZE 128
#define TIME_FORMAT "%Y-%m-%d %H:%M:%S"

typedef gchar time_buf_t[TIME_BUFSIZE];

gchar *timetoa(gchar *buf, time_t t);

gint mbb_time_becmp(time_t ta, time_t tb);
gint mbb_time_pbecmp(time_t ta, time_t tb, time_t pta, time_t ptb);
gint mbb_time_ebcmp(time_t ta, time_t tb);
gint mbb_time_bbcmp(time_t ta, time_t tb);
gint mbb_time_eecmp(time_t ta, time_t tb);
gint mbb_time_rcmp(time_t t, time_t ta, time_t tb);
XmlTag *mbb_time_test_order(time_t a, time_t b, time_t pa, time_t pb);

time_t mbb_time_bmin(time_t ta, time_t tb);
time_t mbb_time_bmax(time_t ta, time_t tb);
time_t mbb_time_emin(time_t ta, time_t tb);
time_t mbb_time_emax(time_t ta, time_t tb);

#endif
