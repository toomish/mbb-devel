/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <glib.h>

#include "slicer.h"
#include "debug.h"

typedef void (*op_func_t)(
	struct slicer *, GList *, gpointer, gpointer, struct slice *
);

struct slicer_entry {
	struct slice slice;
	gpointer data;
};

struct slicer {
	GQueue *queue;
	GTree *tree;
	gint count;

	gpointer user_data;

	struct slice_ops *sops;
	struct slice_data_ops *sdops;
};

struct slicer_search_data {
	gpointer point;
	struct slice_ops *sops;
};

gpointer slicer_dup_dummy(gpointer data)
{
	return data;
}

struct slice_ops *slicer_get_ops(Slicer *slicer)
{
	return slicer->sops;
}

static inline gint begin_end_cmp(struct slice_ops *sops, gpointer begin, gpointer end)
{
	if (begin == NULL || end == NULL)
		return -1;

	return sops->cmp(begin, end);
}

static inline gint end_begin_cmp(struct slice_ops *sops, gpointer end, gpointer begin)
{
	if (end == NULL || begin == NULL)
		return 1;

	return sops->cmp(end, begin);
}

static inline gint begin_begin_cmp(struct slice_ops *sops, gpointer begin1, gpointer begin2)
{
	if (begin1 == NULL && begin2 == NULL)
		return 0;
	if (begin1 == NULL)
		return -1;
	if (begin2 == NULL)
		return 1;

	return sops->cmp(begin1, begin2);
}

static inline gint end_end_cmp(struct slice_ops *sops, gpointer end1, gpointer end2)
{
	if (end1 == NULL && end2 == NULL)
		return 0;
	if (end1 == NULL)
		return 1;
	if (end2 == NULL)
		return -1;

	return sops->cmp(end1, end2);
}

gint slice_cmp(struct slice *a, struct slice *b, struct slice_ops *sops)
{
	if (end_begin_cmp(sops, a->end, b->begin) < 0)
		return -1;
	if (begin_end_cmp(sops, a->begin, b->end) > 0)
		return 1;
	return 0;
}

void slice_cross(struct slice_ops *sops, struct slice *a, struct slice *b,
		 struct slice *slices[3])
{
	gpointer begin, end;

	if (slice_cmp(a, b, sops)) {
		slices[0] = slices[1] = slices[2] = NULL;
		return;
	}

	if (begin_begin_cmp(sops, a->begin, b->begin) >= 0) {
		begin = a->begin;
		slices[0] = NULL;
	} else {
		begin = b->begin;

		if (slices[0] != NULL) {
			slices[0]->begin = sops->dup(a->begin);
			slices[0]->end = sops->dec(sops->dup(b->begin));
		}
	}

	if (end_end_cmp(sops, b->end, a->end) >= 0) {
		end = a->end;
		slices[2] = NULL;
	} else {
		end = b->end;

		if (slices[2] != NULL) {
			slices[2]->begin = sops->inc(sops->dup(b->end));
			slices[2]->end = sops->dup(a->end);
		}
	}

	if (slices[1] != NULL) {
		slices[1]->begin = sops->dup(begin);
		slices[1]->end = sops->dup(end);
	}
}

static struct slicer_entry *slicer_entry_new(gpointer data, gpointer begin, gpointer end)
{
	struct slicer_entry *se;

	se = g_new(struct slicer_entry, 1);
	se->slice.begin = begin;
	se->slice.end = end;
	se->data = data;

	return se;
}

struct slicer *slicer_new(struct slice_ops *sops, struct slice_data_ops *sdops)
{
	struct slicer *slicer;

	slicer = g_new(struct slicer, 1);
	slicer->count = 0;
	slicer->sops = sops;
	slicer->sdops = sdops;

	slicer->queue = g_queue_new();

	slicer->tree = g_tree_new_with_data(
		(GCompareDataFunc) slice_cmp, slicer->sops
	);

	slicer->user_data = NULL;

	return slicer;
}

void slicer_set_user_data(struct slicer *slicer, gpointer user_data)
{
	slicer->user_data = user_data;
}

gpointer slicer_get_user_data(struct slicer *slicer)
{
	return slicer->user_data;
}

static struct slicer_entry *
slicer_entry_copy(struct slicer_entry *entry, struct slicer *slicer)
{
	struct slicer_entry *new;
	s_dup_func_t key_dup;
	s_data_dup_func_t data_dup;

	key_dup = slicer->sops->dup;
	data_dup = slicer->sdops->dup;

	new = slicer_entry_new(
		key_dup(entry->slice.begin), key_dup(entry->slice.end),
		data_dup(entry->data, slicer->user_data)
	);

	return new;
}

static void slicer_tree_join(GTree *tree, GList *list)
{
	struct slicer_entry *entry;

	entry = (struct slicer_entry *) list->data;
	g_tree_insert(tree, &entry->slice, list);
}

struct slicer *slicer_copy(struct slicer *slicer)
{
	struct slicer *new;
	GList *list;

	new = g_new(struct slicer, 1);
	new->count = slicer->count;
	new->sops = slicer->sops;
	new->sdops = slicer->sdops;

	new->queue = g_queue_copy(slicer->queue);
	new->tree = g_tree_new_with_data(
		(GCompareDataFunc) slice_cmp, new->sops
	);

	g_queue_foreach(new->queue, (GFunc) slicer_entry_copy, new);

	for (list = new->queue->head; list != NULL; list = list->next)
		slicer_tree_join(new->tree, list);

	return new;
}

void slicer_init(struct slicer *slicer, gpointer data, gpointer begin, gpointer end)
{
	struct slicer_entry *entry;

	if (slicer->queue->length == 0) {
		if (begin != NULL)
			begin = slicer->sops->dup(begin);

		if (end != NULL)
			end = slicer->sops->dup(end);

		if (data != NULL)
			data = slicer->sdops->dup(data, slicer->user_data);

		entry = slicer_entry_new(data, begin, end);
		g_queue_push_head(slicer->queue, entry);
		g_tree_insert(slicer->tree, &entry->slice, slicer->queue->head);
	}
}

static void slicer_entry_free(struct slicer_entry *entry, struct slicer *slicer)
{
	s_data_free_func_t data_free_func;
	s_free_func_t free_func;

	free_func = slicer->sops->free;
	if (free_func != NULL) {
		free_func(entry->slice.begin);
		free_func(entry->slice.end);
	}

	if (entry->data != NULL) {
		data_free_func = slicer->sdops->free;
		if (data_free_func != NULL)
			data_free_func(entry->data, slicer->user_data);

		slicer->count--;
	}

	g_free(entry);
}

void slicer_destroy(struct slicer *slicer)
{
	g_queue_foreach(slicer->queue, (GFunc) slicer_entry_free, slicer);
	g_queue_free(slicer->queue);
	g_tree_destroy(slicer->tree);
	g_free(slicer);
}

static void insert_before(struct slicer *slicer, GList *next,
			  struct slicer_entry *entry)
{
	GList *list;

	if (next != NULL) {
		g_queue_insert_before(slicer->queue, next, entry);
		list = next->prev;
	} else {
		g_queue_push_tail(slicer->queue, entry);
		list = slicer->queue->tail;
	}

	g_tree_insert(slicer->tree, &entry->slice, list);
}

static void insert_dup(struct slicer *slicer, GList *next, gpointer data,
		       struct slice *slice)
{
	struct slicer_entry *entry;

	data = slicer->sdops->dup(data, slicer->user_data);
	entry = slicer_entry_new(data, slice->begin, slice->end);
	insert_before(slicer, next, entry);

	if (data != NULL)
		slicer->count++;
}

static void insert_add(struct slicer *slicer, GList *next, gpointer cur_data,
		       gpointer new_data, struct slice *slice)
{
	struct slicer_entry *entry;
	gpointer data;

	data = slicer->sdops->dup(cur_data, slicer->user_data);
	data = slicer->sdops->add(data, new_data);
	entry = slicer_entry_new(data, slice->begin, slice->end);

	insert_before(slicer, next, entry);
	slicer->count++;
}

static void insert_del(struct slicer *slicer, GList *next, gpointer cur_data,
		       gpointer new_data, struct slice *slice)
{
	struct slicer_entry *entry;
	gpointer data;

	data = slicer->sdops->dup(cur_data, slicer->user_data);
	data = slicer->sdops->del(data, new_data);
	entry = slicer_entry_new(data, slice->begin, slice->end);

	insert_before(slicer, next, entry);
	if (data != NULL)
		slicer->count++;
}

static gint change_data(struct slicer *slicer, GList *list, gpointer new_data,
			struct slice *new_slice, op_func_t func)
{
	struct slicer_entry *entry;
	struct slice *cur_slice;
	struct slice slices_buf[3];
	struct slice *slices[3] = { NULL, NULL, NULL };
	gpointer cur_data;
	GList *next;

	entry = (struct slicer_entry *) list->data;
	cur_slice = &entry->slice;
	cur_data = entry->data;

	if (slice_cmp(cur_slice, new_slice, slicer->sops))
		return 0;

	next = list->next;
	g_tree_remove(slicer->tree, cur_slice);
	g_queue_delete_link(slicer->queue, list);

	slices[0] = &slices_buf[0];
	slices[1] = &slices_buf[1];
	slices[2] = &slices_buf[2];
	slice_cross(slicer->sops, cur_slice, new_slice, slices);

	if (slices[0] != NULL)
		insert_dup(slicer, next, cur_data, slices[0]);

	g_assert(slices[1] != NULL);
	func(slicer, next, cur_data, new_data, slices[1]);

	if (slices[2] != NULL)
		insert_dup(slicer, next, cur_data, slices[2]);

	slicer_entry_free(entry, slicer);
	return 1;
}

static void slicer_apply_change(struct slicer *slicer, gpointer data,
				gpointer begin, gpointer end, op_func_t func)
{
	struct slice slice = { begin, end };
	GList *list, *prev, *next;

	g_return_if_fail(slicer != NULL);
	g_return_if_fail(data != NULL);

	if (begin_end_cmp(slicer->sops, begin, end) > 0)
		return;

	list = g_tree_lookup(slicer->tree, &slice);
	g_assert(list != NULL);

	prev = list->prev;
	while (list != NULL) {
		next = list->next;
		if (change_data(slicer, list, data, &slice, func) == 0)
			break;
		list = next;
	}

	list = prev;
	while (list != NULL) {
		prev = list->prev;
		if (change_data(slicer, list, data, &slice, func) == 0)
			break;
		list = prev;
	}
}

void slicer_insert(struct slicer *slicer, gpointer data, gpointer begin, gpointer end)
{
	slicer_apply_change(slicer, data, begin, end, insert_add);
}

void slicer_delete(struct slicer *slicer, gpointer data, gpointer begin, gpointer end)
{
	slicer_apply_change(slicer, data, begin, end, insert_del);
}

void slicer_foreach(struct slicer *slicer, s_user_func_t func, gpointer arg)
{
	struct slicer_entry *entry;
	struct slice slice;
	GList *list;

	list = slicer->queue->head;
	for (; list != NULL; list = list->next) {
		entry = (struct slicer_entry *) list->data;

		if (entry->data != NULL) {
			slice = entry->slice;
			func(entry->data, &slice, arg);
		}
	}
}

void slicer_foreach_all(struct slicer *slicer, s_user_func_t func, gpointer arg)
{
	struct slicer_entry *entry;
	struct slice slice;
	GList *list;

	list = slicer->queue->head;
	for (; list != NULL; list = list->next) {
		entry = (struct slicer_entry *) list->data;

		slice = entry->slice;
		func(entry->data, &slice, arg);
	}
}

void slicer_iter_init(SlicerIter *iter, Slicer *slicer)
{
	if (slicer == NULL)
		iter->p = NULL;
	else
		iter->p = slicer->queue->head;
}

gboolean slicer_iter_next(SlicerIter *iter, gpointer *data, struct slice *slice)
{
	struct slicer_entry *entry;
	GList *list;

	if (iter->p == NULL)
		return FALSE;

	list = (GList *) iter->p;
	entry = (struct slicer_entry *) list->data;

	*data = entry->data;
	if (slice != NULL) {
		slice->begin = entry->slice.begin;
		slice->end = entry->slice.end;
	}

	iter->p = list->next;
	return TRUE;
}

gint slicer_count(struct slicer *slicer)
{
	return slicer->count;
}

gint slice_cmp_point(struct slice *slice, gpointer point, struct slice_ops *sops)
{
	if (slice->begin != NULL && sops->cmp(point, slice->begin) < 0)
		return 1;
	if (slice->end != NULL && sops->cmp(slice->end, point) < 0)
		return -1;
	return 0;
}

static gint point_cmp_func(struct slice *slice, struct slicer_search_data *ssd)
{
	return - slice_cmp_point(slice, ssd->point, ssd->sops);
}

gpointer slicer_find(struct slicer *slicer, gpointer point)
{
	struct slicer_search_data ssd;
	GList *list;

	ssd.point = point;
	ssd.sops = slicer->sops;

	list = g_tree_search(slicer->tree, (GCompareFunc) point_cmp_func, &ssd);
	if (list == NULL)
		return NULL;

	return ((struct slicer_entry *) list->data)->data;
}

GQueue *slicer_search(struct slicer *slicer, gpointer begin, gpointer end)
{
	struct slice slice = { begin, end };
	struct slicer_entry *entry;
	GList *list, *prev;
	GQueue *queue;

	queue = g_queue_new();

	list = g_tree_lookup(slicer->tree, &slice);
	g_assert(list != NULL);

	entry = (struct slicer_entry *) list->data;
	if (entry->data != NULL)
		g_queue_push_head(queue, entry->data);
	prev = list->prev;
	list = list->next;
	while (list != NULL) {
		entry = (struct slicer_entry *) list->data;
		if (slice_cmp(&slice, &entry->slice, slicer->sops))
			break;
		if (entry->data != NULL)
			g_queue_push_tail(queue, entry->data);
		list = list->next;
	}

	list = prev;
	while (list != NULL) {
		entry = (struct slicer_entry *) list->data;
		if (slice_cmp(&slice, &entry->slice, slicer->sops))
			break;
		if (entry->data != NULL)
			g_queue_push_head(queue, entry->data);
		list = list->prev;
	}

	if (g_queue_get_length(queue) == 0) {
		g_queue_free(queue);
		queue = NULL;
	}

	return queue;
}

static void slicer_glue(Slicer *slicer, GList *from, GList *to)
{
	struct slicer_entry *entry;
	gpointer begin, end;
	GList *next;

	if (from == to)
		return;

	begin = ((struct slicer_entry *) from->data)->slice.begin;
	end = ((struct slicer_entry *) to->data)->slice.end;

	begin = slicer->sops->dup(begin);
	end = slicer->sops->dup(end);

	to = to->next;
	for (; from != to; from = next) {
		next = from->next;

		entry = (struct slicer_entry *) from->data;

		g_tree_remove(slicer->tree, &entry->slice);
		g_queue_delete_link(slicer->queue, from);
		slicer_entry_free(entry, slicer);
	}

	entry = slicer_entry_new(NULL, begin, end);
	insert_before(slicer, to, entry);
}

void slicer_glue_null(Slicer *slicer)
{
	struct slicer_entry *entry;
	GList *from, *to;
	GList *list;

	if (slicer->sdops->is_null == NULL)
		return;

	from = to = NULL;

	for (list = slicer->queue->head; list != NULL; list = list->next) {
		entry = (struct slicer_entry *) list->data;

		if (slicer->sdops->is_null(entry->data)) {
			if (from != NULL)
				to = list;
			else {
				from = list;
				to = NULL;
			}
		} else if (to == NULL)
			from = NULL;
		else {
			slicer_glue(slicer, from, to);
			from = to = NULL;
		}
	}

	if (to != NULL)
		slicer_glue(slicer, from, to);
}

