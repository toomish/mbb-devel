#ifndef INET_H
#define INET_H

#include <glib.h>

#define IPV4_MAX ((ipv4_t) 0xffffffff)

#define IPV4_BUFSIZE 32
#define INET_BUFSIZE 32

typedef guint32 ipv4_t;
typedef guint8 byte_t;

typedef gchar ipv4_buf_t[IPV4_BUFSIZE];
typedef gchar inet_buf_t[INET_BUFSIZE];

typedef struct inet Inet;

struct inet {
	ipv4_t addr;
	byte_t mask;
};

#define ipv4ton(ip) g_htonl(ip)

gchar *ipv4toa(gchar *buf, ipv4_t ip);
gchar *inettoa(gchar *buf, struct inet *inet);
gint strtoipv4(gchar *buf, gchar **endptr, ipv4_t *ip);
gint strtoinet(gchar *buf, gchar **endptr, struct inet *inet);

struct inet *atoipv4(gchar *buf, struct inet *inet);
struct inet *atoinet(gchar *buf, struct inet *inet);

guint inet_hash(struct inet *inet);
gboolean inet_equal(struct inet *a, struct inet *b);

guint ipv4_hash(gconstpointer v);
gboolean ipv4_equal(gconstpointer v1, gconstpointer v2);

#endif
