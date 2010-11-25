/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <stdio.h>

#include "range.h"
#include "debug.h"

struct range *inet2range(struct inet *inet, struct range *r)
{
	if (inet->mask >= 32)
		return range_new_uno(inet->addr, r);
	else {
		ipv4_t mask, tmp;
		ipv4_t ip;

		tmp = 0xffffffff >> inet->mask;
		mask = ~tmp;
		ip = inet->addr & mask;

		return range_new(ip, ip | tmp, r);
	}
}

struct range *range_new_uno(ipv4_t value, struct range *r)
{
	if (r == NULL)
		r = g_new(struct range, 1);

	r->r_min = r->r_max = value;

	return r;
}

struct range *range_new(ipv4_t min, ipv4_t max, struct range *r)
{
	DBG(
		if (min > max)
			err_quit("min > max: %s, %s", ipv4toa(NULL, min),
				 ipv4toa(NULL, max));
	);

	if (r == NULL)
		r = g_new(struct range, 1);

	r->r_min = min;
	r->r_max = max;

	return r;
}

static inline struct range *range_dup(struct range *r)
{
	return range_new(r->r_min, r->r_max, r);
}

int range_cmp(struct range *ra, struct range *rb)
{
	if (ra->r_max < rb->r_min)
		return -1;
	if (ra->r_min > rb->r_max)
		return 1;
	return 0;
}

int range_ipcmp(struct range *r, ipv4_t *ip)
{
        if (*ip < r->r_min)
                return 1;
        if (r->r_max < *ip)
                return -1;

        return 0;
}

gchar *rangetoa(gchar *buf, struct range *r)
{
	if (r->r_min == r->r_max)
		return ipv4toa(buf, r->r_min);
	else {
		range_buf_t range_buf;
		ipv4_buf_t ipv4_buf;
		gint n;

		if (buf == NULL)
			buf = range_buf;

		ipv4toa(ipv4_buf, r->r_min);
		n = g_snprintf(range_buf, sizeof(range_buf_t), "%s-", ipv4_buf);

		ipv4toa(ipv4_buf, r->r_max);
		g_snprintf(range_buf + n, sizeof(range_buf_t) - n, "%s", ipv4_buf);

		if (buf == range_buf)
			buf = g_strdup(buf);

		return buf;
	}
}

