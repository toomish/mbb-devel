#ifndef MBB_GATEWAY_H
#define MBB_GATEWAY_H

#include <glib.h>
#include <time.h>

#include "inet.h"

struct mbb_gwlink;
typedef struct mbb_gateway MbbGateway;

struct mbb_gateway {
	gint id;
	gchar *name;
	ipv4_t addr;

	GHashTable *ht;
};

struct mbb_gateway *mbb_gateway_new(gint id, gchar *name, ipv4_t addr);
void mbb_gateway_free(struct mbb_gateway *gw);
void mbb_gateway_join(struct mbb_gateway *gw);
void mbb_gateway_detach(struct mbb_gateway *gw);
void mbb_gateway_remove(struct mbb_gateway *gw);

gboolean mbb_gateway_mod_name(struct mbb_gateway *gw, gchar *newname);

gchar *mbb_gateway_get_ename(struct mbb_gateway *gw, gboolean addr);
struct mbb_gateway *mbb_gateway_get_by_id(gint id);
struct mbb_gateway *mbb_gateway_get_by_name(gchar *name);
struct mbb_gateway *mbb_gateway_get_by_addr(ipv4_t addr);
struct mbb_gateway *mbb_gateway_get_by_ename(gchar *ename);

gboolean mbb_gateway_can_add(struct mbb_gateway *gw, struct mbb_gwlink *gl,
			     gboolean create);
gboolean mbb_gateway_add_link(struct mbb_gateway *gw, struct mbb_gwlink *gl,
			      struct mbb_gwlink **old);
void mbb_gateway_del_link(struct mbb_gateway *gw, struct mbb_gwlink *gl);
gboolean mbb_gateway_reorder_link(struct mbb_gateway *gw, struct mbb_gwlink *gl);
guint mbb_gateway_nlink(struct mbb_gateway *gw);

struct mbb_gwlink *mbb_gateway_get_link(struct mbb_gateway *gw, guint link, time_t t);
struct mbb_gwlink *mbb_gateway_find_link(ipv4_t ip, guint link, time_t t);

void mbb_gateway_foreach(GFunc func, gpointer user_data);
void mbb_gateway_link_foreach(struct mbb_gateway *gw, GFunc func, gpointer user_data);

#endif
