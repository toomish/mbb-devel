/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_SESSION_H
#define MBB_SESSION_H

#include "mbbuser.h"
#include "mbbcap.h"

#include <glib.h>
#include <time.h>

typedef enum {
	MBB_SESSION_XML,
	MBB_SESSION_HTTP
} mbb_session_type;

struct mbb_var;

struct mbb_session {
	MbbUser *user;

	gchar *peer;
	guint port;

	mbb_session_type type;
	time_t start;
	time_t mtime;

	gboolean killed;
	gchar *kill_msg;

	GHashTable *vars;
	GHashTable *cached_vars;
};

void mbb_session_add_var(struct mbb_var *var);
void mbb_session_del_var(struct mbb_var *var);

guint mbb_session_new(struct mbb_session *ss, gchar *peer, guint port, mbb_session_type type);
void mbb_session_touch(struct mbb_session *ss);

gboolean mbb_session_has(gint sid);
void mbb_session_quit(guint sid);
gboolean mbb_session_auth(struct mbb_session *ss, gchar *login, gchar *secret,
			  gchar *type);

struct mbb_session *current_session(void);
gboolean mbb_session_is_http(void);

GHashTable *mbb_session_get_cached(gboolean create);

#endif
