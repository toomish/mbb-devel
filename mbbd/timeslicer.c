#include "timeslicer.h"

#include <time.h>

static gint time_cmp(gpointer a, gpointer b);
static gpointer time_inc(gpointer p);
static gpointer time_dec(gpointer p);

struct slice_ops time_ops = {
	.cmp = time_cmp,
	.dup = slicer_dup_dummy,
	.inc = time_inc,
	.dec = time_dec
};

static gint time_cmp(gpointer a, gpointer b)
{
	time_t t1, t2;

	t1 = GPOINTER_TO_TIME(a);
	t2 = GPOINTER_TO_TIME(b);

	return t1 - t2;
}

static gpointer time_inc(gpointer p)
{
	time_t t;

	t = GPOINTER_TO_TIME(p);
	++t;

	return TIME_TO_POINTER(t);
}

static gpointer time_dec(gpointer p)
{
	time_t t;

	t = GPOINTER_TO_TIME(p);
	if (t > 0)
		--t;

	return TIME_TO_POINTER(t);
}

Slicer *time_slicer_new(struct slice_data_ops *sdops)
{
	Slicer *slicer;

	slicer = slicer_new(&time_ops, sdops);
	slicer_init(slicer, NULL, 0, 0);

	return slicer;
}

