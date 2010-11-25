/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbgroup.h"
#include "mbbuser.h"

#include "datalink.h"
#include "macros.h"

#define G_HASH_TABLE_HAS_KEY(ht, key) \
	g_hash_table_lookup_extended(ht, GINT_TO_POINTER(key), NULL, NULL)

enum {
	MBB_GROUP_SELF,
	MBB_GROUP_ID,
	MBB_GROUP_NAME
};

static struct data_link_entry dl_entries[] = {
	DATA_LINK_SELF_ENTRY,
	{ G_STRUCT_OFFSET(struct mbb_group, id), g_int_hash, g_int_equal },
	{ G_STRUCT_OFFSET(struct mbb_group, name), g_pstr_hash, g_pstr_equal }
};

static DataLink *dlink = NULL;

static inline void dlink_init(void)
{
	dlink = data_link_new_full(
		dl_entries, NELEM(dl_entries), (GDestroyNotify) mbb_group_free
	);
}

struct mbb_group *mbb_group_new(gint id, gchar *name)
{
	struct mbb_group *group;

	group = g_new(struct mbb_group, 1);
	group->id = id;
	group->name = g_strdup(name);
	group->users = NULL;

	return group;
}

void mbb_group_free(struct mbb_group *group)
{
	if (group->users != NULL)
		g_hash_table_destroy(group->users);
	g_free(group->name);
	g_free(group);
}

void mbb_group_join(struct mbb_group *group)
{
	if (dlink == NULL)
		dlink_init();

	data_link_insert(dlink, group);
}

void mbb_group_detach(struct mbb_group *group)
{
	if (dlink != NULL)
		data_link_steal(dlink, MBB_GROUP_SELF, group);
}

void mbb_group_remove(struct mbb_group *group)
{
	mbb_group_detach(group);
	mbb_group_free(group);
}

gboolean mbb_group_mod_name(struct mbb_group *group, gchar *newname)
{
	if (dlink == NULL || mbb_group_get_by_name(newname))
		return FALSE;

	data_link_steal(dlink, MBB_GROUP_SELF, group);
	g_free(group->name);
	group->name = g_strdup(newname);
	data_link_insert(dlink, group);

	return TRUE;
}

struct mbb_group *mbb_group_get_by_id(gint id)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_GROUP_ID, &id);
}

struct mbb_group *mbb_group_get_by_name(gchar *name)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_GROUP_NAME, &name);
}

gboolean mbb_group_has_user(struct mbb_group *group, gint user_id)
{
	if (group->users == NULL)
		return FALSE;
	else
		return G_HASH_TABLE_HAS_KEY(group->users, user_id);
}

void mbb_group_add_user(struct mbb_group *group, struct mbb_user *user)
{
	if (group->users == NULL)
		group->users = g_hash_table_new(NULL, NULL);
	else if (G_HASH_TABLE_HAS_KEY(group->users, user->id))
		return;

	g_hash_table_insert(group->users, GINT_TO_POINTER(user->id), user);
	user->cap_mask |= 1 << group->id;
}

void mbb_group_del_user(struct mbb_group *group, struct mbb_user *user)
{
	if (group->users == NULL)
		return;

	if (g_hash_table_remove(group->users, GINT_TO_POINTER(user->id)))
		user->cap_mask &= ~ (mbb_cap_t) (1 << group->id);
}

void mbb_group_foreach(GFunc func, gpointer user_data)
{
	data_link_foreach(dlink, func, user_data);
}

