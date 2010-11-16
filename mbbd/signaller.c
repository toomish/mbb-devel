#include "signaller.h"

#include <pthread.h>

struct signaller {
	gint signum;
	pthread_t thread;
	GMutex *mutex;
	gboolean blocked;
};

Signaller *signaller_new(gint signum)
{
	Signaller *s;

	s = g_new(Signaller, 1);
	s->signum = signum;
	s->thread = pthread_self();
	s->mutex = g_mutex_new();
	s->blocked = FALSE;

	return s;
}

void signaller_block(Signaller *s)
{
	g_mutex_lock(s->mutex);
	s->blocked = TRUE;
	g_mutex_unlock(s->mutex);
}

void signaller_unblock(Signaller *s)
{
	g_mutex_lock(s->mutex);
	s->blocked = FALSE;
	g_mutex_unlock(s->mutex);
}

void signaller_raise(Signaller *s)
{
	g_mutex_lock(s->mutex);
	
	if (s->blocked == FALSE)
		pthread_kill(s->thread, s->signum);

	g_mutex_unlock(s->mutex);
}

void signaller_free(Signaller *s)
{
	g_mutex_free(s->mutex);
	g_free(s);
}

