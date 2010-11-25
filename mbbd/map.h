/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MIKE_WAS_HERE_MAP_H
#define MIKE_WAS_HERE_MAP_H

#include "slicer.h"

#define MAP_INIT { NULL, NULL, NULL, NULL }

typedef struct map_node MapNode;

struct map_node {
	GQueue *queue;
	GList *list;
};

struct map {
	Slicer *slicer;
	struct slice_ops *dkey_ops;

	void (*node_add)(MapNode *node);
	void (*node_del)(MapNode *node);
};

struct map_cross {
	struct slice slice;
	gpointer data;
	gboolean found;
};

typedef SlicerIter MapIter;
typedef struct map_data_iter MapDataIter;

struct map_data_iter {
	gpointer p;
};

void map_init(struct map *map, struct slice_ops *dkey_ops);
void map_init_slicer(struct map *map, Slicer *(*create)(struct slice_data_ops *));
void map_clear(struct map *map);
void map_move(struct map *dst, struct map *src);

struct map_node *map_node_dup(struct map_node *node);
gpointer map_node_get_data(struct map_node *node);
void map_node_get_slice(struct map_node *node, struct slice *slice);
void map_node_remove(struct map *map, struct map_node *node);
gint map_node_cmp(struct map_node *a, struct map_node *b);
gint map_node_data_cmp(struct map_node *a, struct map_node *b);

void map_iter_init(MapIter *iter, struct map *map);
gboolean map_iter_next(MapIter *iter, MapDataIter *data_iter, struct slice *slice);
gboolean map_data_iter_is_null(MapDataIter *iter);
gboolean map_data_iter_next(MapDataIter *iter, gpointer *data, struct slice *slice);

void map_iter_last(MapIter *iter);
void map_data_iter_last(MapDataIter *iter);

void map_add(struct map *map, struct slice *key, struct slice *dkey,
	     gpointer data, struct map_cross *cross);

void map_del(struct map *map, struct slice *key, struct slice *dkey,
	     gpointer data);

void map_remove_custom(struct map *map, gpointer user_data, GCompareFunc cmp);
void map_glue_null(struct map *map);

gpointer map_find(struct map *map, gpointer key, gpointer dkey);
GSList *map_search(struct map *map, struct slice *key, struct slice *dkey);

#endif
