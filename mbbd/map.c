#include "map.h"

#include "debug.h"

/* #define MAP_WITHOUT_SPLIT 1 */

struct map_entry {
	struct slice slice;
	gpointer data;
	volatile gint ref_count;
};

struct map_data {
	struct map_cross *cross;
	struct map_entry *entry;
	struct slice_ops *ops;
	struct map *map;
};

struct map_search_data {
	GCompareFunc cmp;
	gpointer user_data;
	struct slice_ops *ops;
};

static GQueue *map_data_add(GQueue *queue, struct map_data *data);
static GQueue *map_data_del(GQueue *queue, struct map_data *data);
static GQueue *map_data_dup(GQueue *queue, struct map *map);
static void map_data_free(GQueue *queue, struct map *map);
static gboolean map_data_is_null(GQueue *queue);

static struct slice_data_ops map_data_ops = {
	.add = (s_add_func_t) map_data_add,
	.del = (s_del_func_t) map_data_del,
	.dup = (s_data_dup_func_t) map_data_dup,
	.free = (s_data_free_func_t) map_data_free,
	.is_null = (s_data_is_null_func_t) map_data_is_null
};

static struct map_entry *map_entry_new(gpointer pa, gpointer pb, gpointer data)
{
	struct map_entry *entry;

	entry = g_new(struct map_entry, 1);
	entry->slice.begin = pa;
	entry->slice.end = pb;
	entry->data = data;
	entry->ref_count = 1;

	MSG_WARN("new %p", (void *) entry);

	return entry;
}

static struct map_node *map_node_new(GQueue *queue, GList *list)
{
	struct map_node *node;

	node = g_new(struct map_node, 1);
	node->queue = queue;
	node->list = list;

	return node;
}

static struct map_entry *map_entry_ref(struct map_entry *entry)
{
	g_atomic_int_inc(&entry->ref_count);

	return entry;
}

static void map_entry_unref(struct map_entry *entry, struct slice_ops *ops)
{
	if (g_atomic_int_dec_and_test(&entry->ref_count)) {
		if (ops->free != NULL) {
			ops->free(entry->slice.begin);
			ops->free(entry->slice.end);
		}

		MSG_WARN("free %p", (void *) entry);
		g_free(entry);
	}
}

static inline void map_node_handle_add(struct map *map, GQueue *queue, GList *list)
{
	if (map->node_add != NULL) {
		struct map_node node;

		node.queue = queue;
		node.list = list;

		map->node_add(&node);
	}
}

static inline void map_node_handle_del(struct map *map, GQueue *queue, GList *list)
{
	if (map->node_del != NULL) {
		struct map_node node;

		node.queue = queue;
		node.list = list;

		map->node_del(&node);
	}
}

static inline void map_data_insert_tail(GQueue *queue, struct map_entry *cur,
					struct map_data *data)
{
	g_queue_push_tail(queue, cur);
	map_node_handle_add(data->map, queue, queue->tail);
}

static inline void map_data_insert_before(GQueue *queue, GList *list, struct map_entry *cur,
					  struct map_data *data)
{
	g_queue_insert_before(queue, list, cur);
	map_node_handle_add(data->map, queue, list->prev);
}

#ifndef MAP_WITHOUT_SPLIT
static inline gboolean map_data_split(GList *list, struct map_entry *mea,
				      struct map_entry *meb, struct map_data *data)
{
	gpointer begin, end;

	if (mea->data != meb->data)
		return FALSE;

	if (data->ops->cmp(meb->slice.begin, mea->slice.end) != 1)
		return FALSE;

	begin = data->ops->dup(mea->slice.begin);
	end = data->ops->dup(meb->slice.end);

	list->data = map_entry_new(begin, end, mea->data);

	map_entry_unref(mea, data->ops);
	map_entry_unref(meb, data->ops);

	return TRUE;
}

static inline gboolean map_data_split_links(GQueue *queue, GList *la, GList *lb,
					    struct map_data *data)
{
	if (map_data_split(la, la->data, lb->data, data)) {
		g_queue_delete_link(queue, lb);
		return TRUE;
	}

	return FALSE;
}
#endif

static GQueue *map_data_add(GQueue *queue, struct map_data *data)
{
	struct map_entry *entry;
	struct map_entry *cur;
	GList *list;
	gint n;

	if (data->cross->found)
		return queue;

	cur = map_entry_ref(data->entry);

	if (queue == NULL)
		queue = g_queue_new();

	if (queue->length == 0) {
		g_queue_push_head(queue, cur);
		map_node_handle_add(data->map, queue, queue->head);

		return queue;
	}

	for (list = queue->head; list != NULL; list = list->next) {
		entry = (struct map_entry *) list->data;
		n = slice_cmp(&cur->slice, &entry->slice, data->ops);

		if (n > 0)
			continue;

		if (n < 0)
			break;

		data->cross->slice.begin = data->ops->dup(entry->slice.begin);
		data->cross->slice.end = data->ops->dup(entry->slice.end);
		data->cross->data = entry->data;
		data->cross->found = TRUE;

		map_entry_unref(cur, data->ops);
		return queue;
	}

#ifdef MAP_WITHOUT_SPLIT
	if (list == NULL)
		map_data_insert_tail(queue, cur, data);
	else
		map_data_insert_before(queue, list, cur, data);
#else
	if (list == NULL) {
		list = queue->tail;

		if (! map_data_split(list, list->data, cur, data))
			map_data_insert_tail(queue, cur, data);
	} else do {
		gboolean ret;

		ret = map_data_split(list, cur, list->data, data);

		if (list->prev == NULL) {
			if (ret == FALSE)
				map_data_insert_before(queue, list, cur, data);
			break;
		}

		list = list->prev;

		if (ret == TRUE) {
			map_data_split_links(queue, list, list->next, data);
			break;
		}

		if (! map_data_split(list, list->data, cur, data))
			map_data_insert_before(queue, list->next, cur, data);
	} while (0);
#endif

	return queue;
}

static GQueue *map_data_del(GQueue *queue, struct map_data *data)
{
	struct map_entry *entry, *cur, *tmp;
	struct slice smem[2];
	struct slice *pp[3];
	GList *list;
	GList *next;

	if (queue == NULL)
		return NULL;

	pp[1] = NULL;
	cur = data->entry;
	for (list = queue->head; list != NULL; list = next) {
		next = list->next;

		entry = (struct map_entry *) list->data;

		if (cur->data != NULL && cur->data != entry->data)
			continue;

		if (slice_cmp(&entry->slice, &cur->slice, data->ops))
			continue;

		pp[0] = &smem[0]; pp[2] = &smem[1];
		slice_cross(data->ops, &entry->slice, &cur->slice, pp);

		map_node_handle_del(data->map, queue, list);

		if (pp[0] != NULL) {
			tmp = map_entry_new(pp[0]->begin, pp[0]->end, entry->data);
			g_queue_insert_before(queue, list, tmp);
			map_node_handle_add(data->map, queue, list->prev);
		}

		if (pp[2] != NULL) {
			tmp = map_entry_new(pp[2]->begin, pp[2]->end, entry->data);
			g_queue_insert_before(queue, list, tmp);
			map_node_handle_add(data->map, queue, list->prev);
		}

		map_entry_unref(entry, data->ops);
		g_queue_delete_link(queue, list);
	}

	if (queue->length == 0) {
		g_queue_free(queue);
		queue = NULL;
	}

	return queue;
}

static GQueue *map_data_dup(GQueue *queue, struct map *map)
{
	GQueue *new_queue;
	GList *list;

	if (queue == NULL)
		return NULL;

	new_queue = g_queue_new();
	for (list = queue->head; list != NULL; list = list->next) {
		g_queue_push_tail(new_queue, list->data);
		map_entry_ref(list->data);
		map_node_handle_add(map, new_queue, new_queue->tail);
	}

/*
	g_queue_foreach(queue, (GFunc) map_entry_ref, NULL);
*/

	return new_queue;
}

static void map_data_free(GQueue *queue, struct map *map)
{
	GList *list;

	for (list = queue->head; list != NULL; list = list->next) {
		map_node_handle_del(map, queue, list);
		map_entry_unref(list->data, map->dkey_ops);
	}

	g_queue_free(queue);
}

static gboolean map_data_is_null(GQueue *queue)
{
	if (queue == NULL)
		return TRUE;

	if (queue->length == 0)
		return TRUE;

	return FALSE;
}

void map_init(struct map *map, struct slice_ops *dkey_ops)
{
	map->slicer = NULL;
	map->dkey_ops = dkey_ops;

	if (dkey_ops == NULL) {
		map->node_add = NULL;
		map->node_del = NULL;
	}
}

void map_init_slicer(struct map *map, Slicer *(*create)(struct slice_data_ops *))
{
	if (map->slicer != NULL)
		slicer_destroy(map->slicer);

	map->slicer = create(&map_data_ops);
	slicer_set_user_data(map->slicer, map);
}

void map_clear(struct map *map)
{
	if (map->slicer != NULL) {
		slicer_destroy(map->slicer);
		map->slicer = NULL;
	}
}

void map_move(struct map *dst, struct map *src)
{
	map_clear(dst);
	*dst = *src;

	slicer_set_user_data(dst->slicer, dst);
}

struct map_node *map_node_dup(struct map_node *node)
{
	return map_node_new(node->queue, node->list);
}

gpointer map_node_get_data(struct map_node *node)
{
	struct map_entry *entry;

	entry = (struct map_entry *) node->list->data;

	return entry->data;
}

void map_node_get_slice(struct map_node *node, struct slice *slice)
{
	struct map_entry *entry;

	entry = (struct map_entry *) node->list->data;
	*slice = entry->slice;
}

gint map_node_cmp(struct map_node *a, struct map_node *b)
{
	if (a->queue != b->queue)
		return 1;

	if (a->list != b->list)
		return 1;

	return 0;
}

gint map_node_data_cmp(struct map_node *a, struct map_node *b)
{
	struct map_entry *pa;
	struct map_entry *pb;

	pa = (struct map_entry *) a->list->data;
	pb = (struct map_entry *) b->list->data;

	return pa->data != pb->data;
}

void map_node_remove(struct map *map, struct map_node *node)
{
	map_entry_unref(node->list->data, map->dkey_ops);
	g_queue_delete_link(node->queue, node->list);
}

void map_iter_init(MapIter *iter, struct map *map)
{
	slicer_iter_init(iter, map->slicer);
}

void map_iter_last(MapIter *iter)
{
	iter->p = g_list_last(iter->p);
}

gboolean map_iter_next(MapIter *iter, MapDataIter *data_iter, struct slice *slice)
{
	GQueue *queue;
	gboolean ret;

	ret = slicer_iter_next(iter, (gpointer) &queue, slice);
	if (ret == FALSE)
		return FALSE;

	if (queue == NULL)
		data_iter->p = NULL;
	else
		data_iter->p = queue->head;

	return TRUE;
}

gboolean map_data_iter_is_null(MapDataIter *iter)
{
	return iter->p == NULL;
}

gboolean map_data_iter_next(MapDataIter *iter, gpointer *data, struct slice *slice)
{
	struct map_entry *entry;
	GList *list;

	if (iter->p == NULL)
		return FALSE;

	list = (GList *) iter->p;
	entry = (struct map_entry *) list->data;

	*data = entry->data;
	if (slice != NULL) {
		slice->begin = entry->slice.begin;
		slice->end = entry->slice.end;
	}

	iter->p = list->next;
	return TRUE;
}

void map_data_iter_last(MapDataIter *iter)
{
	iter->p = g_list_last(iter->p);
}

void map_add(struct map *map, struct slice *key, struct slice *dkey,
	     gpointer ptr, struct map_cross *cross)
{
	struct map_entry *entry;
	struct map_data data;

	entry = map_entry_new(
		map->dkey_ops->dup(dkey->begin),
		map->dkey_ops->dup(dkey->end),
		ptr
	);

	cross->found = FALSE;
	data.cross = cross;
	data.entry = entry;
	data.ops = map->dkey_ops;
	data.map = map;

	slicer_insert(map->slicer, &data, key->begin, key->end);

	map_entry_unref(entry, map->dkey_ops);
}

void map_del(struct map *map, struct slice *key, struct slice *dkey, gpointer ptr)
{
	struct map_entry *entry;
	struct map_data data;

	entry = map_entry_new(
		map->dkey_ops->dup(dkey->begin),
		map->dkey_ops->dup(dkey->end),
		ptr
	);

	data.cross = NULL;
	data.entry = entry;
	data.ops = map->dkey_ops;
	data.map = map;

	slicer_delete(map->slicer, &data, key->begin, key->end);

	map_entry_unref(entry, map->dkey_ops);
}

static void map_data_remove_custom(GQueue *queue, struct slice *slice G_GNUC_UNUSED,
				   struct map_search_data *data)
{
	struct map_entry *entry;
	GList *list;
	GList *next;

	for (list = queue->head; list != NULL; list = next) {
		next = list->next;

		entry = (struct map_entry *) list->data;
		if (data->cmp(entry->data, data->user_data)) {
			map_entry_unref(entry, data->ops);
			g_queue_delete_link(queue, list);
		}
	}
}

void map_remove_custom(struct map *map, gpointer user_data, GCompareFunc cmp)
{
	struct map_search_data data;

	if (map == NULL || map->slicer == NULL)
		return;

	data.cmp = cmp;
	data.user_data = user_data;
	data.ops = map->dkey_ops;

	slicer_foreach(map->slicer, (s_user_func_t) map_data_remove_custom, &data);
}

gpointer map_find(struct map *map, gpointer key, gpointer dkey)
{
	struct map_entry *entry;
	GQueue *queue;
	GList *list;
	gint n;

	queue = slicer_find(map->slicer, key);
	if (queue == NULL)
		return NULL;

	for (list = queue->head; list != NULL; list = list->next) {
		entry = (struct map_entry *) list->data;

		n = slice_cmp_point(&entry->slice, dkey, map->dkey_ops);
		if (n == 0)
			return entry->data;
		else if (n > 0)
			break;
	}

	return NULL;
}

static void map_data_search(GQueue *data_queue, GSList **found,
			    struct slice *dkey, struct slice_ops *ops)
{
	struct map_entry *entry;
	GList *list;

	for (list = data_queue->head; list != NULL; list = list->next) {
		entry = (struct map_entry *) list->data;

		if (! slice_cmp(dkey, &entry->slice, ops))
			*found = g_slist_prepend(*found, entry->data);
	}
}

GSList *map_search(struct map *map, struct slice *key, struct slice *dkey)
{
	GQueue *key_queue;
	GSList *found;
	GList *list;

	key_queue = slicer_search(map->slicer, key->begin, key->end);
	if (key_queue == NULL)
		return NULL;

	found = NULL;
	for (list = key_queue->head; list != NULL; list = list->next)
		map_data_search(list->data, &found, dkey, map->dkey_ops);

	return found;
}

void map_glue_null(struct map *map)
{
	if (map->slicer != NULL)
		slicer_glue_null(map->slicer);
}

