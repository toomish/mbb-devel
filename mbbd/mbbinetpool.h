/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_INET_POOL_H
#define MBB_INET_POOL_H

#include "mbbobject.h"
#include "inet.h"
#include "map.h"

#include <time.h>

typedef struct mbb_inet_pool MbbInetPool;
typedef struct mbb_inet_pool_entry MbbInetPoolEntry;

struct mbb_inet_pool_entry {
	gint id;

	struct inet inet;
	gboolean flag;

	time_t start;
	time_t end;

	guint nice;

	struct mbb_object *owner;

	GList *node_list;
};

struct mbb_inet_pool_entry *mbb_inet_pool_entry_new(struct mbb_object *owner);
void mbb_inet_pool_entry_init(struct mbb_inet_pool_entry *entry);
void mbb_inet_pool_entry_free(struct mbb_inet_pool_entry *entry);
void mbb_inet_pool_entry_delete(struct mbb_inet_pool_entry *entry);
gboolean mbb_inet_pool_entry_test_time(struct mbb_inet_pool_entry *entry,
				       time_t start, time_t end);

time_t mbb_inet_pool_entry_get_start(MbbInetPoolEntry *entry);
time_t mbb_inet_pool_entry_get_end(MbbInetPoolEntry *entry);

struct mbb_inet_pool *mbb_inet_pool_new(void);
void mbb_inet_pool_add(struct mbb_inet_pool *pool, struct mbb_inet_pool_entry *entry);
void mbb_inet_pool_del(struct mbb_inet_pool *pool, gint id);
struct mbb_inet_pool_entry *mbb_inet_pool_get(struct mbb_inet_pool *pool, gint id);
void mbb_inet_pool_foreach(struct mbb_inet_pool *pool, GFunc func, gpointer user_data);

void mbb_inet_pool_map_add_handler(MapNode *node);
void mbb_inet_pool_map_del_handler(MapNode *node);

#endif
