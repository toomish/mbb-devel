/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include <glib.h>

struct mbb_obj {
	gint id;
	gchar *name;
};

struct attr_group {
	gchar *name;
	gint id;

	struct mbb_obj *(*obj_get_by_id)(gint id);
	struct mbb_obj *(*obj_get_by_name)(gchar *name);

	GSList *attr_list;
};

struct attr {
	struct attr_group *group;

	gint id;
	gchar *name;
};

struct attr_key {
	gchar *group;
	gchar *name;
};

extern struct attr_group attr_group_table[];

gchar *attr_group_get_obj_name(struct attr_group *ag, gint id);
gint attr_group_get_obj_id(struct attr_group *ag, gchar *name);

void attr_add(struct attr_group *ag, gint id, gchar *name);
void attr_del(struct attr *attr);
void attr_rename(struct attr *attr, gchar *name);

struct attr *attr_get(gchar *group, gchar *name);
struct attr *attr_get_by_id(gint id);

void attr_free_all(void);

#endif
