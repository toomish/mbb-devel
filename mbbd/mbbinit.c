#include "mbbinit.h"

static struct mbb_init_struct *init_struct_head = NULL;
static gboolean initialized = FALSE;

void mbb_init_push(struct mbb_init_struct *init_struct)
{
	init_struct->next = init_struct_head;
	init_struct_head = init_struct;
}

void mbb_init_pushv(struct mbb_init_struct *p, gsize count)
{
	for (; count > 0; p++, count--)
		mbb_init_push(p);
}

void mbb_init(void)
{
	struct mbb_init_struct *is;

	for (is = init_struct_head; is != NULL; is = is->next)
		is->init_func(is->data);
	init_struct_head = NULL;

	initialized = TRUE;
}

gboolean mbb_initialized(void)
{
	return initialized;
}

