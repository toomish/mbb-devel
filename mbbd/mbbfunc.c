/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbthread.h"
#include "mbbxmlmsg.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbblog.h"

#include "mbbplock.h"

static GHashTable *ht = NULL;

void mbb_func_register_all(struct mbb_func_struct *func_struct)
{
	if (ht == NULL)
		ht = g_hash_table_new(g_str_hash, g_str_equal);

	for (; func_struct->name != NULL; func_struct++)
		g_hash_table_insert(ht, func_struct->name, func_struct);
}

guint mbb_func_mregister(struct mbb_func_struct *func_struct, MbbModule *mod)
{
	guint count = 0;

	mbb_plock_writer_lock();

	if (ht == NULL)
		ht = g_hash_table_new(g_str_hash, g_str_equal);

	for (; func_struct->name != NULL; func_struct++) {
		if (g_hash_table_lookup(ht, func_struct->name) != NULL) {
			mbb_log_self("register func '%s' failed", func_struct->name);
			continue;
		}

		func_struct->module = mod;
		g_hash_table_insert(ht, func_struct->name, func_struct);
		count++;
	}

	mbb_plock_writer_unlock();

	return count;
}

void mbb_func_unregister(struct mbb_func_struct *func_struct)
{
	mbb_plock_writer_lock();

	if (ht != NULL)
		g_hash_table_remove(ht, func_struct->name);

	mbb_plock_writer_unlock();
}

void mbb_func_unregister_all(struct mbb_func_struct *func_struct)
{
	mbb_plock_writer_lock();

	if (ht != NULL) {
		for (; func_struct->name != NULL; func_struct++) {
			if (func_struct->module != NULL) {
				g_hash_table_remove(ht, func_struct->name);
				func_struct->module = NULL;
			}
		}
	}

	mbb_plock_writer_unlock();
}

static struct mbb_func_struct *mbb_func_search(gchar *name, mbb_cap_t mask)
{
	struct mbb_func_struct *fs;

	if (ht == NULL)
		return NULL;

	fs = g_hash_table_lookup(ht, name);
	if (fs == NULL)
		return NULL;

	if (! mbb_cap_check(mask, fs->cap_mask))
		return NULL;

	if (! mbb_module_use(fs->module))
		return NULL;

	return fs;
}

GSList *mbb_func_get_methods(mbb_cap_t mask)
{
	struct mbb_func_struct *fs;
	GHashTableIter iter;
	GSList *list = NULL;

	g_hash_table_iter_init(&iter, ht);

	mbb_plock_reader_lock();
	while (g_hash_table_iter_next(&iter, NULL, (gpointer *) &fs)) {
		if (fs->module != NULL && ! mbb_module_isrun(fs->module))
			continue;

		if (mbb_cap_check(mask, fs->cap_mask))
			list = g_slist_prepend(list, fs->name);
	}

	mbb_plock_reader_unlock();

	return list;
}

gboolean mbb_func_call(gchar *name, XmlTag *tag, XmlTag **ans)
{
	struct mbb_func_struct *fs;
	mbb_cap_t cap;

	cap = mbb_thread_get_cap();

	mbb_plock_reader_lock();
	fs = mbb_func_search(name, cap);
	mbb_plock_reader_unlock();

	if (fs == NULL) {
		mbb_log("missed call %s", name);
		return FALSE;
	}

	mbb_log("call %s", name);

	*ans = NULL;
	fs->func(tag, ans);

	if (*ans != NULL) {
		gchar *msg;

		if (mbb_xml_msg_is_ok(*ans, &msg) == FALSE) {
			if (msg == NULL)
				mbb_log("%s failed", name);
			else
				mbb_log("%s failed: %s", name, msg);
		}
	}

	mbb_module_unuse(fs->module);

	return TRUE;
}

