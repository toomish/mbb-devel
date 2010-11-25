/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "inetslicer.h"

#include "inet.h"

static gint inet_cmp(gpointer a, gpointer b);
static gpointer inet_inc(gpointer p);
static gpointer inet_dec(gpointer p);

struct slice_ops inet_ops = {
	.cmp = inet_cmp,
	.dup = slicer_dup_dummy,
	.inc = inet_inc,
	.dec = inet_dec
};

static gint inet_cmp(gpointer a, gpointer b)
{
	ipv4_t ip1, ip2;

	ip1 = GPOINTER_TO_IPV4(a);
	ip2 = GPOINTER_TO_IPV4(b);

	if (ip1 < ip2)
		return -1;

	if (ip1 > ip2)
		return 1;

	return 0;
}

static gpointer inet_inc(gpointer p)
{
	ipv4_t ip;

	ip = GPOINTER_TO_IPV4(p);

	if (ip < IPV4_MAX)
		++ip;

	return IPV4_TO_POINTER(ip);
}

static gpointer inet_dec(gpointer p)
{
	ipv4_t ip;

	ip = GPOINTER_TO_IPV4(p);

	if (ip > 0)
		--ip;

	return IPV4_TO_POINTER(ip);
}

Slicer *inet_slicer_new(struct slice_data_ops *sdops)
{
	Slicer *slicer;

	slicer = slicer_new(&inet_ops, sdops);
	slicer_init(slicer, NULL, 0, IPV4_TO_POINTER(IPV4_MAX));

	return slicer;
}

