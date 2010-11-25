/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_CONSUMER_H
#define MBB_CONSUMER_H

#include <glib.h>
#include <time.h>

#include "re.h"

struct mbb_unit;
struct mbb_user;

typedef struct mbb_consumer MbbConsumer;

struct mbb_consumer {
	gint id;
	gchar *name;

	GSList *units;

	time_t start;
	time_t end;

	struct mbb_user *user;
};

/* struct mbb_consumer *mbb_consumer_new(gint id, gchar *name); */
struct mbb_consumer *mbb_consumer_new(gint id, gchar *name, time_t start, time_t end);
void mbb_consumer_free(struct mbb_consumer *con);
void mbb_consumer_join(struct mbb_consumer *con);
void mbb_consumer_detach(struct mbb_consumer *con);
void mbb_consumer_remove(struct mbb_consumer *con);

gboolean mbb_consumer_mod_name(struct mbb_consumer *con, gchar *newname);
gboolean mbb_consumer_mod_time(struct mbb_consumer *con, time_t start, time_t end);

struct mbb_consumer *mbb_consumer_get_by_id(gint id);
struct mbb_consumer *mbb_consumer_get_by_name(gchar *name);

void mbb_consumer_foreach(GFunc func, gpointer user_data);
void mbb_consumer_forregex(Regex re, GFunc func, gpointer user_data);

void mbb_consumer_add_unit(struct mbb_consumer *con, struct mbb_unit *unit);
void mbb_consumer_del_unit(struct mbb_consumer *con, struct mbb_unit *unit);
gboolean mbb_consumer_has_unit(struct mbb_consumer *con, struct mbb_unit *unit);

gboolean mbb_consumer_mapped(struct mbb_consumer *con);

#endif
