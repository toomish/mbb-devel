#ifndef MBB_GROUP_H
#define MBB_GROUP_H

#include <glib.h>

typedef struct mbb_group MbbGroup;
struct mbb_user;

struct mbb_group {
	gint id;
	gchar *name;

	GHashTable *users;
};

struct mbb_group *mbb_group_new(gint id, gchar *name);
void mbb_group_free(struct mbb_group *group);
void mbb_group_join(struct mbb_group *group);
void mbb_group_detach(struct mbb_group *group);
void mbb_group_remove(struct mbb_group *group);

gboolean mbb_group_mod_name(struct mbb_group *group, gchar *newname);

struct mbb_group *mbb_group_get_by_id(gint id);
struct mbb_group *mbb_group_get_by_name(gchar *name);

gboolean mbb_group_has_user(struct mbb_group *group, gint user_id);
void mbb_group_add_user(struct mbb_group *group, struct mbb_user *user);
void mbb_group_del_user(struct mbb_group *group, struct mbb_user *user);

void mbb_group_foreach(GFunc func, gpointer user_data);

#endif
