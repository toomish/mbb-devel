#include "mbbgroup.h"
#include "mbbuser.h"

#include "datalink.h"
#include "macros.h"
#include "debug.h"

enum {
	MBB_USER_SELF,
	MBB_USER_ID,
	MBB_USER_NAME
};

static struct data_link_entry dl_entries[] = {
	DATA_LINK_SELF_ENTRY,
	{ G_STRUCT_OFFSET(struct mbb_user, id), g_int_hash, g_int_equal },
	{ G_STRUCT_OFFSET(struct mbb_user, name), g_pstr_hash, g_pstr_equal }
};

static DataLink *dlink = NULL;

static inline void dlink_init(void)
{
	dlink = data_link_new_full(
		dl_entries, NELEM(dl_entries), (GDestroyNotify) mbb_user_free
	);
}

struct mbb_user *mbb_user_new(gint id, gchar *name, gchar *secret)
{
	struct mbb_user *user;

	user = g_new(struct mbb_user, 1);
	user->id = id;
	user->name = g_strdup(name);
	user->cap_mask = 0;
	user->secret = g_strdup(secret);

	MBB_OBJ_LOCK_INIT(user);
	MBB_OBJ_REF_INIT(user);

	user->childs = NULL;

	return user;
}

void mbb_user_free(struct mbb_user *user)
{
	msg_warn("free user %s", user->name);

	g_slist_free(user->childs);
	g_free(user->name);
	g_free(user->secret);
	g_free(user);
}

void mbb_user_join(struct mbb_user *user)
{
	if (dlink == NULL)
		dlink_init();

	data_link_insert(dlink, user);
}

void mbb_user_detach(struct mbb_user *user)
{
	MbbGroup *group;
	mbb_cap_t mask;
	guint n;

	if (dlink == NULL)
		return;

	mask = user->cap_mask;
	for (n = 0; mask != 0; n++, mask >>= 1) {
		if (mask & 1) {
			group = mbb_group_get_by_id(n);
			if (group != NULL)
				mbb_group_del_user(group, user);
		}
	}

	data_link_steal(dlink, MBB_USER_SELF, user);
}

struct mbb_user *mbb_user_get_by_id(gint id)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_USER_ID, &id);
}

struct mbb_user *mbb_user_get_by_name(gchar *name)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_USER_NAME, &name);
}

void mbb_user_remove(struct mbb_user *user)
{
	user->cap_mask = 0;

	mbb_user_detach(user);
	mbb_user_unref(user);
}

void mbb_user_add_consumer(struct mbb_user *user, struct mbb_consumer *con)
{
	if (g_slist_find(user->childs, con) == NULL)
		user->childs = g_slist_prepend(user->childs, con);
}

void mbb_user_del_consumer(struct mbb_user *user, struct mbb_consumer *con)
{
	user->childs = g_slist_remove(user->childs, con);
}

gboolean mbb_user_has_consumer(struct mbb_user *user, struct mbb_consumer *con)
{
	return g_slist_find(user->childs, con) != NULL;
}

gboolean mbb_user_mod_name(struct mbb_user *user, gchar *newname)
{
	gchar *tmp;

	if (dlink == NULL || mbb_user_get_by_name(newname))
		return FALSE;

	data_link_steal(dlink, MBB_USER_SELF, user);
	tmp = user->name;

	mbb_user_lock(user);
	user->name = g_strdup(newname);
	mbb_user_unlock(user);

	g_free(tmp);
	data_link_insert(dlink, user);

	return TRUE;
}

void mbb_user_mod_pass(struct mbb_user *user, gchar *newpass)
{
	g_free(user->secret);
	user->secret = g_strdup(newpass);
}

GSList *mbb_user_get_groups(struct mbb_user *user)
{
	mbb_cap_t mask;
	GSList *list;
	guint n;

	list = NULL;
	mask = user->cap_mask;
	for (n = 0; mask != 0; n++, mask >>= 1)
		if (mask & 1)
			list = g_slist_prepend(list, GINT_TO_POINTER(n));

	return list;
}

void mbb_user_foreach(GFunc func, gpointer user_data)
{
	data_link_foreach(dlink, func, user_data);
}

void mbb_user_forregex(Regex re, GFunc func, gpointer user_data)
{
	data_link_forregex(dlink, re, MBB_USER_NAME, func, user_data);
}

