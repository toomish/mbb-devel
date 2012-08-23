/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbinit.h"

static struct mbb_init_struct *init_struct_head = NULL;
static gboolean initialized = FALSE;

void mbb_init_push(struct mbb_init_struct *init_struct, gsize count)
{
	for (; count > 0; init_struct++, count--) {
		init_struct->next = init_struct_head;
		init_struct_head = init_struct;
	}
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

