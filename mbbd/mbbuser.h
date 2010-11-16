#ifndef MBB_USER_H
#define MBB_USER_H

#include <glib.h>

#include "mbbmacro.h"
#include "mbbcap.h"
#include "re.h"

typedef struct mbb_user MbbUser;
struct mbb_consumer;

struct mbb_user {
	gint id;
	gchar *name;

	gchar *secret;
	mbb_cap_t cap_mask;

	MBB_OBJ_LOCK;
	MBB_OBJ_REF;

	GSList *childs;
};

struct mbb_user *mbb_user_new(gint id, gchar *name, gchar *secret);
void mbb_user_free(struct mbb_user *user);
void mbb_user_join(struct mbb_user *user);
void mbb_user_detach(struct mbb_user *user);
void mbb_user_remove(struct mbb_user *user);

MBB_OBJ_DEFINE_LOCK(user)
MBB_OBJ_DEFINE_REF(user)

gboolean mbb_user_mod_name(struct mbb_user *user, gchar *newname);
void mbb_user_mod_pass(struct mbb_user *user, gchar *newpass);

struct mbb_user *mbb_user_get_by_id(gint id);
struct mbb_user *mbb_user_get_by_name(gchar *name);

void mbb_user_add_consumer(struct mbb_user *user, struct mbb_consumer *con);
void mbb_user_del_consumer(struct mbb_user *user, struct mbb_consumer *con);
gboolean mbb_user_has_consumer(struct mbb_user *user, struct mbb_consumer *con);

GSList *mbb_user_get_groups(struct mbb_user *user);
void mbb_user_foreach(GFunc func, gpointer user_data);
void mbb_user_forregex(Regex re, GFunc func, gpointer user_data);

#endif
