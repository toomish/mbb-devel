#include <string.h>

#include "mbbmodule.h"
#include "mbbplock.h"
#include "mbbauth.h"

#include "mbbxmlmsg.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbblog.h"

#include "xmltag.h"

struct mbb_auth_method {
	gchar *name;
	MbbModule *mod;
	mbb_auth_func_t func;
};

static GHashTable *ht = NULL;

static struct mbb_auth_method *mbb_auth_method_new(gchar *name, mbb_auth_func_t func)
{
	struct mbb_auth_method *am;

	am = g_new(struct mbb_auth_method, 1);
	am->name = name;
	am->mod = mbb_module_current();
	am->func = func;

	return am;
}

static gboolean mbb_auth_method_is_alive(struct mbb_auth_method *am)
{
	return am->mod == NULL ? TRUE : mbb_module_isrun(am->mod);
}

static inline void ht_init(void)
{
	ht = g_hash_table_new_full(
		g_str_hash, g_str_equal, NULL, (GDestroyNotify) g_free
	);
}

MbbUser *mbb_auth_plain(gchar *login, gchar *secret)
{
	MbbUser *user;

	if (secret == NULL)
		return NULL;

	if ((user = mbb_user_get_by_name(login)) == NULL)
		return NULL;

	if (user->secret == NULL)
		return NULL;

	if (! strcmp(user->secret, secret))
		return user;

	return NULL;
}

MbbUser *mbb_auth(gchar *login, gchar *secret, gchar *type)
{
	MbbUser *user = NULL;

	if (type == NULL)
		type = "plain";

	mbb_plock_reader_lock();
	if (ht != NULL) {
		struct mbb_auth_method *am;

		am = g_hash_table_lookup(ht, type);
		if (am != NULL && mbb_module_use(am->mod)) {
			user = am->func(login, secret);
			mbb_module_unuse(am->mod);
		}
	}
	mbb_plock_reader_unlock();

	return user;
}

gpointer mbb_auth_method_register(gchar *name, mbb_auth_func_t func)
{
	struct mbb_auth_method *am;

	mbb_plock_writer_lock();

	if (ht == NULL)
		ht_init();
	else {
		if (g_hash_table_lookup(ht, name) != NULL) {
			mbb_plock_writer_unlock();
			return NULL;
		}
	}

	am = mbb_auth_method_new(name, func);
	g_hash_table_insert(ht, name, am);

	mbb_plock_writer_unlock();

	mbb_log("register auth method '%s'", name);

	return am;
}

void mbb_auth_method_unregister(gpointer method_key)
{
	struct mbb_auth_method *am = method_key;
	gboolean isremoved = FALSE;
	gchar *name;

	if (am == NULL)
		return;

	name = am->name;
	mbb_plock_writer_lock();
	if (ht != NULL)
		isremoved = g_hash_table_remove(ht, name);
	mbb_plock_writer_unlock();

	if (isremoved)
		mbb_log("unregister auth method '%s'", name);
}

static void auth_method_list(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	*ans = mbb_xml_msg_ok();

	mbb_plock_reader_lock();
	if (ht != NULL) {
		struct mbb_auth_method *am;
		GHashTableIter iter;

		g_hash_table_iter_init(&iter, ht);
		while (g_hash_table_iter_next(&iter, NULL, (gpointer) &am)) {
			XmlTag *xt;

			if (mbb_auth_method_is_alive(am) == FALSE)
				continue;

			xt = xml_tag_new_child(*ans, "method",
				"name", variant_new_string(am->name)
			);

			if (am->mod != NULL) {
				gchar *name = mbb_module_get_name(am->mod);

				xml_tag_set_attr(xt, "module",
					variant_new_string(name)
				);
			}
		}
	}

	mbb_plock_reader_unlock();
}

MBB_FUNC_REGISTER_STRUCT("mbb-server-auth-list", auth_method_list, MBB_CAP_ADMIN);

static void init_vars(void)
{
	mbb_auth_method_register("plain", mbb_auth_plain);
}

static void __init init(void)
{
	static struct mbb_init_struct entry[] = {
		MBB_INIT_VARS,
		MBB_INIT_FUNC_STRUCT(auth_method_list)
	};

	mbb_init_pushv(entry, NELEM(entry));
}

