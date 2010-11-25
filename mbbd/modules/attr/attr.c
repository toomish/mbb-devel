/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "attr.h"

#include "mbbconsumer.h"
#include "mbboperator.h"
#include "mbbgateway.h"
#include "mbbunit.h"
#include "mbbuser.h"

#define DEF_ATTR_GROUP(name) { \
	#name, -1, \
	(struct mbb_obj *(*)(gint)) mbb_##name##_get_by_id, \
	(struct mbb_obj *(*)(gchar *)) mbb_##name##_get_by_name, \
	NULL \
}

struct attr_group attr_group_table[] = {
	DEF_ATTR_GROUP(user),
	DEF_ATTR_GROUP(unit),
	DEF_ATTR_GROUP(consumer),
	DEF_ATTR_GROUP(gateway),
	DEF_ATTR_GROUP(operator),
	{ NULL, -1, NULL, NULL, NULL }
};

static GHashTable *ht_attr_id;
static GHashTable *ht_attr_key;

gchar *attr_group_get_obj_name(struct attr_group *ag, gint id)
{
	struct mbb_obj *mo;

	mo = ag->obj_get_by_id(id);

	if (mo == NULL)
		return NULL;

	return mo->name;
}

gint attr_group_get_obj_id(struct attr_group *ag, gchar *name)
{
	struct mbb_obj *mo;

	mo = ag->obj_get_by_name(name);

	if (mo == NULL)
		return -1;

	return mo->id;
}

void attr_free_all(void)
{
	if (ht_attr_key != NULL) {
		g_hash_table_destroy(ht_attr_key);
		g_hash_table_destroy(ht_attr_id);

		ht_attr_key = ht_attr_id = NULL;
	}
}

static struct attr_key *attr_key_new(gchar *group, gchar *name)
{
	struct attr_key *key;

	key = g_new(struct attr_key, 1);
	key->group = group;
	key->name = name;

	return key;
}

static guint attr_key_hash(struct attr_key *key)
{
	return g_str_hash(key->group) + g_str_hash(key->name);
}

static gboolean attr_key_equal(struct attr_key *ka, struct attr_key *kb)
{
	if (g_str_equal(ka->group, kb->group) == FALSE)
		return FALSE;

	return g_str_equal(ka->name, kb->name);
}

static void attr_free(struct attr *attr)
{
	g_free(attr->name);
	g_free(attr);
}

static inline void ht_attr_init(void)
{
	ht_attr_key = g_hash_table_new_full(
		(GHashFunc) attr_key_hash, (GEqualFunc) attr_key_equal,
		g_free, NULL
	);

	ht_attr_id = g_hash_table_new_full(
		g_direct_hash, g_direct_equal,
		NULL, (GDestroyNotify) attr_free
	);
}

struct attr *attr_get(gchar *group, gchar *name)
{
	struct attr_key key;

	if (ht_attr_key == NULL)
		return NULL;

	key.group = group;
	key.name = name;

	return g_hash_table_lookup(ht_attr_key, &key);
}

struct attr *attr_get_by_id(gint id)
{
	if (ht_attr_id == NULL)
		return NULL;

	return g_hash_table_lookup(ht_attr_id, GINT_TO_POINTER(id));
}

void attr_add(struct attr_group *ag, gint id, gchar *name)
{
	struct attr_key *key;
	struct attr *attr;

	if (ht_attr_key == NULL)
		ht_attr_init();

	attr = g_new(struct attr, 1);
	attr->group = ag;
	attr->id = id;
	attr->name = g_strdup(name);

	key = attr_key_new(ag->name, attr->name);
	g_hash_table_insert(ht_attr_key, key, attr);
	g_hash_table_insert(ht_attr_id, GINT_TO_POINTER(id), attr);
	ag->attr_list = g_slist_prepend(ag->attr_list, attr);
}

void attr_del(struct attr *attr)
{
	struct attr_group *ag;
	struct attr_key key;

	ag = attr->group;

	key.group = ag->name;
	key.name = attr->name;

	ag->attr_list = g_slist_remove(ag->attr_list, attr);
	g_hash_table_remove(ht_attr_key, &key);
	g_hash_table_remove(ht_attr_id, GINT_TO_POINTER(attr->id));
}

void attr_rename(struct attr *attr, gchar *name)
{
	struct attr_group *ag;
	struct attr_key *key;
	struct attr_key old;

	ag = attr->group;

	old.group = ag->name;
	old.name = attr->name;

	g_hash_table_remove(ht_attr_key, &old);
	g_free(attr->name);

	attr->name = g_strdup(name);
	key = attr_key_new(ag->name, attr->name);
	g_hash_table_insert(ht_attr_key, key, attr);
}

