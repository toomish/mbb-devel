#ifndef SEPARATOR_H
#define SEPARATOR_H

#include "mbbinetpool.h"

#define SEPARATOR_INIT { G_QUEUE_INIT }

typedef struct separator Separator;

struct separator {
	GQueue queue;
};

Separator *separator_new(void);
void separator_init(Separator *sep);

void separator_add(Separator *sep, MbbInetPoolEntry *entry);
void separator_del(Separator *sep, MbbInetPoolEntry *entry);

gboolean separator_has(Separator *sep, MbbInetPoolEntry *entry);
gint separator_count(Separator *sep);

Separator *separator_copy(Separator *dst, Separator *src, gpointer excl);
void separator_move(Separator *dst, Separator *src);

void separator_clear(Separator *sep);
void separator_free(Separator *sep);

#endif
