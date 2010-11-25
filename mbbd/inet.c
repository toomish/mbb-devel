/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "inet.h"

#include <string.h>
#include <glib.h>

gchar *ipv4toa(gchar *buf, ipv4_t ip)
{
	byte_t b0, b1, b2, b3;
	ipv4_buf_t ipv4_buf;

	b0 = (byte_t) ip;
	ip >>= 8;
	b1 = (byte_t) ip;
	ip >>= 8;
	b2 = (byte_t) ip;
	ip >>= 8;
	b3 = (byte_t) ip;

	if (buf == NULL)
		buf = ipv4_buf;

	g_snprintf(buf, sizeof(ipv4_buf_t), "%d.%d.%d.%d", b3, b2, b1, b0);

	if (buf == ipv4_buf)
		buf = g_strdup(buf);

	return buf;
}

gchar *inettoa(gchar *buf, struct inet *inet)
{
	inet_buf_t inet_buf;
	ipv4_buf_t ipv4_buf;

	g_return_val_if_fail(inet != NULL, NULL);

	if (buf == NULL)
		buf = inet_buf;

	ipv4toa(ipv4_buf, inet->addr);
	if (inet->mask < 32)
		g_snprintf(buf, sizeof(inet_buf_t), "%s/%d", ipv4_buf, inet->mask);
	else
		strcpy(buf, ipv4_buf);

	if (buf == inet_buf)
		buf = g_strdup(buf);

	return buf;
}

static gint read_byte(gchar **pbuf, byte_t *dst)
{
	guint num = 0;
	gchar *p = *pbuf;
	gchar ch;

	ch = *p;
	if (!g_ascii_isdigit(ch))
		return -1;

	do {
		num = num * 10 + g_ascii_digit_value(ch);
		if (num > G_MAXUINT8)
			return -1;
		ch = *++p;
	} while (g_ascii_isdigit(ch));

	*dst = (byte_t) num;
	*pbuf = p;

	return 0;
}

gint strtoipv4(gchar *buf, gchar **endptr, ipv4_t *dst)
{
	gchar *p = buf;
	ipv4_t ip = 0;
	byte_t byte;
	gint n;

	g_return_val_if_fail(buf != NULL, -1);
	g_return_val_if_fail(dst != NULL, -1);

	for (n = 0; n < 4; n++) {
		if (n) {
			if (*p != '.')
				return -1;
			p++;
		}

		if (read_byte(&p, &byte) < 0)
			return -1;
		ip = ip << 8 | (ipv4_t) byte;
	}

	if (endptr != NULL)
		*endptr = p;
	*dst = ip;

	return 0;
}

gint strtoinet(gchar *buf, gchar **endptr, struct inet *inet)
{
	gchar *p = buf;
	ipv4_t ip;
	byte_t mask;

	g_return_val_if_fail(buf != NULL, -1);
	g_return_val_if_fail(inet != NULL, -1);

	if (strtoipv4(p, &p, &ip) < 0)
		return -1;

	if (*p != '/')
		mask = 32;
	else {
		p++;
		if (read_byte(&p, &mask) < 0)
			return -1;
		if (mask > 32)
			return -1;
	}

	inet->addr = ip;
	inet->mask = mask;

	if (endptr != NULL)
		*endptr = p;

	return 0;
}

struct inet *atoipv4(gchar *buf, struct inet *dst)
{
	struct inet *inet = dst;
	ipv4_t addr;
	gchar *p = buf;

	g_return_val_if_fail(buf != NULL, NULL);

	if (strtoipv4(p, &p, &addr) < 0)
		return NULL;
	if (g_ascii_isgraph(*p))
		return NULL;
	if (inet == NULL)
		inet = g_new(struct inet, 1);

	inet->addr = addr;
	inet->mask = 32;

	return inet;
}

struct inet *atoinet(gchar *buf, struct inet *dst)
{
	struct inet tmp, *inet;
	gchar *p = buf;

	g_return_val_if_fail(buf != NULL, NULL);

	if (strtoinet(p, &p, &tmp) < 0)
		return NULL;
	if (g_ascii_isgraph(*p))
		return NULL;

	inet = dst;
	if (inet == NULL)
		inet = g_new(struct inet, 1);

	memcpy(inet, &tmp, sizeof(*inet));
	return inet;
}

guint inet_hash(struct inet *inet)
{
	return inet->addr;
}

gboolean inet_equal(struct inet *a, struct inet *b)
{
	if (a->addr == b->addr && a->mask == b->mask)
		return TRUE;
	return FALSE;
}

guint ipv4_hash(gconstpointer v)
{
	return (guint) *(const ipv4_t *) v;
}

gboolean ipv4_equal(gconstpointer v1, gconstpointer v2)
{
	return *(const ipv4_t *) v1 == *(const ipv4_t *) v2;
}

