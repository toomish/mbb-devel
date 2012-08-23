/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#define _XOPEN_SOURCE
#include <string.h>
#include <time.h>

#include "mbbxmlmsg.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbbtime.h"
#include "mbbxtv.h"

#include "variant.h"
#include "xmltag.h"
#include "macros.h"

gchar *timetoa(gchar *buf, time_t t)
{
	time_buf_t time_buf;
	struct tm tm;

	if (buf == NULL)
		buf = time_buf;

	localtime_r(&t, &tm);
	strftime(buf, sizeof(time_buf_t), TIME_FORMAT, &tm);

	if (buf == time_buf)
		buf = g_strdup(buf);

	return buf;
}

static inline int timecmp(time_t a, time_t b)
{
	if (a < b) return -1;
	if (a > b) return 1;

	return 0;
}

gint mbb_time_becmp(time_t ta, time_t tb)
{
	if (ta == 0 || tb == 0)
		return -1;

	return timecmp(ta, tb);
}

gint mbb_time_pbecmp(time_t ta, time_t tb, time_t pta, time_t ptb)
{
	if (ta == MBB_TIME_PARENT)
		ta = pta;
	if (tb == MBB_TIME_PARENT)
		tb = ptb;

	return mbb_time_becmp(ta, tb);
}

gint mbb_time_ebcmp(time_t ta, time_t tb)
{
	if (ta == 0 || tb == 0)
		return 1;

	return timecmp(ta, tb);
}

gint mbb_time_bbcmp(time_t ta, time_t tb)
{
	if (ta == 0 && tb == 0)
		return 0;
	if (ta == 0)
		return -1;
	if (tb == 0)
		return 1;

	return timecmp(ta, tb);
}

gint mbb_time_eecmp(time_t ta, time_t tb)
{
	if (ta == 0 && tb == 0)
		return 0;
	if (ta == 0)
		return 1;
	if (tb == 0)
		return -1;

	return timecmp(ta, tb);
}

time_t mbb_time_bmin(time_t ta, time_t tb)
{
	if (mbb_time_bbcmp(ta, tb) < 0)
		return ta;

	return tb;
}

gint mbb_time_rcmp(time_t t, time_t ta, time_t tb)
{
	if (mbb_time_ebcmp(t, ta) < 0)
		return -1;
	if (mbb_time_becmp(t, tb) > 0)
		return 1;
	return 0;
}

time_t mbb_time_bmax(time_t ta, time_t tb)
{
	if (mbb_time_bbcmp(ta, tb) > 0)
		return ta;

	return tb;
}

time_t mbb_time_emin(time_t ta, time_t tb)
{
	if (mbb_time_eecmp(ta, tb) < 0)
		return ta;

	return tb;
}

time_t mbb_time_emax(time_t ta, time_t tb)
{
	if (mbb_time_eecmp(ta, tb) > 0)
		return ta;

	return tb;
}

XmlTag *mbb_time_test_order(time_t a, time_t b, time_t pa, time_t pb)
{
	if (a < 0 && b < 0)
		;
	else if (a < 0) {
		if (mbb_time_becmp(pa, b) > 0)
			return mbb_xml_msg(MBB_MSG_PSTART_MORE_END);
	} else if (b < 0) {
		if (mbb_time_becmp(a, pb) > 0)
			return mbb_xml_msg(MBB_MSG_PEND_LESS_START);
	} else if (mbb_time_becmp(a, b) > 0)
		return mbb_xml_msg(MBB_MSG_INVALID_TIME_ORDER);

	return NULL;
}

static void server_get_time(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	DEFINE_XTV(XTV_TIME_VALUE_);

	gchar buf[32];
	struct tm tm;
	time_t t;

	time(&t);
	MBB_XTV_CALL(&t);

	asctime_r(localtime_r(&t, &tm), buf);
	*ans = mbb_xml_msg_ok();

	xml_tag_add_child(*ans, xml_tag_newc(
		"time", "value", variant_new_string(buf)
	));
}

MBB_INIT_FUNCTIONS_DO
	MBB_FUNC_STRUCT("mbb-server-get-time", server_get_time, MBB_CAP_ALL),
MBB_INIT_FUNCTIONS_END

MBB_ON_INIT(MBB_INIT_FUNCTIONS)
