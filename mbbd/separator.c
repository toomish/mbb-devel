#include "separator.h"

Separator *separator_new(void)
{
	Separator *sep;

	sep = g_new(Separator, 1);
	g_queue_init(&sep->queue);

	return sep;
}

void separator_init(Separator *sep)
{
	g_queue_init(&sep->queue);
}

static gint entry_cmp(MbbInetPoolEntry *e1, MbbInetPoolEntry *e2)
{
	gint d;

	if ((d = (gint) e1->nice - (gint) e2->nice))
		return d;

	if ((d = (gint) e2->flag - (gint) e1->flag))
		return d;

	return 0;
}

void separator_add(Separator *sep, MbbInetPoolEntry *entry)
{
	GList *list;

	for (list = sep->queue.head; list != NULL; list = list->next)
		if (entry_cmp(entry, list->data) < 0) {
			if (list->prev == NULL)
				g_queue_push_head(&sep->queue, entry);
			else
				g_queue_insert_after(
					&sep->queue, list->prev, entry
				);
			break;
		}

	if (list == NULL)
		g_queue_push_tail(&sep->queue, entry);
}

void separator_del(Separator *sep, MbbInetPoolEntry *entry)
{
	g_queue_remove(&sep->queue, entry);
}

gboolean separator_has(Separator *sep, MbbInetPoolEntry *entry)
{
	return g_queue_find(&sep->queue, entry) != NULL;
}

gint separator_count(Separator *sep)
{
	return g_queue_get_length(&sep->queue);
}

Separator *separator_copy(Separator *dst, Separator *src, gpointer excl)
{
	GList *list;

	if (dst == NULL)
		dst = separator_new();

	for (list = src->queue.tail; list != NULL; list = list->prev)
		if (list->data != excl)
			g_queue_push_head(&dst->queue, list->data);

	return dst;
}

void separator_move(Separator *dst, Separator *src)
{
	separator_clear(dst);
	dst->queue = src->queue;
}

void separator_clear(Separator *sep)
{
	g_queue_clear(&sep->queue);
}

void separator_free(Separator *sep)
{
	g_queue_clear(&sep->queue);
	g_free(sep);
}

