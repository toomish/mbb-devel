#ifndef RANGE_H
#define RANGE_H

#include <glib.h>

#include "inet.h"

#define RANGE_BUFSIZE 64

typedef gchar range_buf_t[RANGE_BUFSIZE];

typedef struct range Range;

struct range {
	ipv4_t r_min;
	ipv4_t r_max;
};

struct range *inet2range(struct inet *inet, struct range *r);
struct range *range_new_uno(ipv4_t value, struct range *r);
struct range *range_new(ipv4_t min, ipv4_t max, struct range *r);
gchar *rangetoa(gchar *buf, struct range *r);

gint range_cmp(struct range *ra, struct range *rb);
gint range_ipcmp(struct range *r, ipv4_t *ip);

#endif
