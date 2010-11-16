#include "trash.h"

typedef struct {
	gpointer data;
	GDestroyNotify destroy;
} TrashEntry;

Trash *trash_new(void)
{
	Trash *trash;

	trash = g_new(Trash, 1);
	trash->list = NULL;

	return trash;
}

void trash_push(Trash *trash, gpointer data, GDestroyNotify destroy)
{
	TrashEntry *entry;

	entry = g_new(TrashEntry, 1);
	entry->data = data;
	entry->destroy = destroy;

	trash->list = g_slist_prepend(trash->list, entry);
}

void trash_empty(Trash *trash)
{
	TrashEntry *entry;
	GSList *list;

	for (list = trash->list; list != NULL; list = list->next) {
		entry = (TrashEntry *) list->data;

		if (entry->destroy == NULL)
			g_free(entry->data);
		else
			entry->destroy(entry->data);
		g_free(entry);
	}

	g_slist_free(trash->list);
	trash->list = NULL;
}

void trash_free(Trash *trash)
{
	trash_empty(trash);
	g_free(trash);
}

void trash_release_data(Trash *trash)
{
	g_slist_foreach(trash->list, (GFunc) g_free, NULL);
	g_slist_free(trash->list);
	trash->list = NULL;
}

