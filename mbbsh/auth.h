#ifndef AUTH_H
#define AUTH_H

#include <glib.h>

gboolean auth_plain(const gchar *user, const gchar *pass);
gboolean auth_key(const gchar *key);

#endif
