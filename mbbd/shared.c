#include "shared.h"

struct shared {
	gpointer data;
	volatile gint ref_count;
	GDestroyNotify destroy;
};

Shared *shared_new_full(gpointer data, GDestroyNotify destroy, gint ref_count)
{
	struct shared *shar;

	shar = g_new(struct shared, 1);
	shar->data = data;
	if (destroy == NULL)
		shar->destroy = g_free;
	else
		shar->destroy = destroy;
	shar->ref_count = ref_count;

	return (Shared *) shar;
}

Shared *shared_ref(Shared *shar)
{
	struct shared *p;

	p = (struct shared *) shar;
	g_atomic_int_inc(&p->ref_count);

	return shar;
}

void shared_unref(Shared *shar)
{
	struct shared *p;

	p = (struct shared *) shar;
	if (g_atomic_int_dec_and_test(&p->ref_count)) {
		p->destroy(p->data);
		g_free(p);
	}
}

void shared_free(Shared *shar)
{
	struct shared *p;

	p = (struct shared *) shar;
	p->destroy(p->data);
	g_free(p);
}

