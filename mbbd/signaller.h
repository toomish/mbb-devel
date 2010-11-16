#ifndef SIGNALLER_H
#define SIGNALLER_H

#include <signal.h>

#include <glib.h>

typedef struct signaller Signaller;

Signaller *signaller_new(gint signum);
void signaller_block(Signaller *s);
void signaller_unblock(Signaller *s);
void signaller_raise(Signaller *s);
void signaller_free(Signaller *s);

#endif
