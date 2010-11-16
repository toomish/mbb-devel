#include "mbbxmlmsg.h"
#include "mbbthread.h"
#include "mbbplock.h"
#include "mbblock.h"
#include "mbbuser.h"
#include "mbbauth.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbbvar.h"
#include "mbbxtv.h"

#include "varconv.h"
#include "variant.h"
#include "xmltag.h"
#include "macros.h"

#include <glib/gprintf.h>

#include <string.h>

#define AUTH_KEY_LEN 4
#define AUTH_KEY_STRLEN ((AUTH_KEY_LEN << 3) + 1)

struct key_data {
	struct mbb_user *user;
	gchar *peer;
	GTimer *timer;
};

static GHashTable *ht = NULL;

static guint authkey_lifetime = 60;

static gboolean auth_key_remove(gpointer key, gpointer value, gpointer data)
{
	struct key_data *kd;

	(void) key;
	(void) data;

	kd = (struct key_data *) value;
	if (g_timer_elapsed(kd->timer, NULL) >= authkey_lifetime)
		return TRUE;

	return FALSE;
}

static inline void auth_key_ht_cleanup(void)
{
	if (ht != NULL)
		g_hash_table_foreach_remove(ht, auth_key_remove, NULL);
}

static inline void auth_key_ht_cleanup_safe(void)
{
	mbb_plock_writer_lock();
	auth_key_ht_cleanup();
	mbb_plock_writer_unlock();
}

MbbUser *mbb_auth_key_get_user(gchar *key, gchar *dummy G_GNUC_UNUSED)
{
	struct mbb_user *user = NULL;
	struct key_data *kd;
	gchar *peer;

	auth_key_ht_cleanup_safe();

	peer = mbb_thread_get_peer();

	mbb_plock_reader_lock();

	if (ht != NULL) {
		kd = (struct key_data *) g_hash_table_lookup(ht, key);

		if (kd != NULL && ! g_strcmp0(peer, kd->peer)) {
			user = kd->user;
			g_timer_start(kd->timer);
		}
	}

	mbb_plock_reader_unlock();

	return user;
}

static gchar *gen_key(gchar *buf)
{
	GRand *rand;
	gchar *p;
	guint n;

	p = buf;
	rand = g_rand_new();
	for (n = 0; n < AUTH_KEY_LEN; n++)
		p += g_sprintf(p, "%08x", g_rand_int(rand));
	*p = '\0';

	g_rand_free(rand);
	return buf;
}

static void key_data_free(struct key_data *kd)
{
	mbb_user_unref(kd->user);

	g_free(kd->peer);
	g_timer_destroy(kd->timer);
	g_free(kd);
}

static inline void auth_key_ht_init(void)
{
	ht = g_hash_table_new_full(
		g_str_hash, g_str_equal,
		g_free, (GDestroyNotify) key_data_free 
	);
}

static void get_auth_key(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	gchar buf[AUTH_KEY_STRLEN];
	struct key_data *kd;
	Variant *var;
	gchar *key;

	kd = g_new(struct key_data, 1);
	kd->user = mbb_user_ref(mbb_thread_get_user());
	kd->peer = g_strdup(mbb_thread_get_peer());
	kd->timer = g_timer_new();

	key = gen_key(buf);

	mbb_plock_writer_lock();

	if (ht == NULL)
		auth_key_ht_init();
	else {
		auth_key_ht_cleanup();

		while (g_hash_table_lookup(ht, key) != NULL)
			key = gen_key(buf);
	}

	key = g_strdup(key);
	g_hash_table_insert(ht, key, kd);
	var = variant_new_string(key);

	mbb_plock_writer_unlock();

	*ans = mbb_xml_msg_ok();
	xml_tag_new_child(*ans, "key", "value", var);
}

static void gather_keys(gpointer key, gpointer value, gpointer data)
{
	struct key_data *kd;
	gchar *username;
	gint ltime;

	kd = (struct key_data *) value;
	ltime = authkey_lifetime - g_timer_elapsed(kd->timer, NULL);

	mbb_user_lock(kd->user);
	username = g_strdup(kd->user->name);
	mbb_user_unlock(kd->user);

	xml_tag_new_child((XmlTag *) data, "key",
		"value", variant_new_string((gchar *) key),
		"user", variant_new_alloc_string(username),
		"peer", variant_new_string(kd->peer),
		"lifetime", variant_new_int(ltime)
	);
}

static void show_auth_keys(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	*ans = mbb_xml_msg_ok();

	auth_key_ht_cleanup_safe();

	mbb_plock_reader_lock();

	if (ht != NULL)
		g_hash_table_foreach(ht, gather_keys, *ans);

	mbb_plock_reader_unlock();
}

static void drop_auth_key(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_KEY_VALUE);

	struct key_data *kd = NULL;
	gchar *key;

	MBB_XTV_CALL(&key);

	mbb_plock_writer_lock();

	on_final { mbb_plock_writer_unlock(); }

	auth_key_ht_cleanup();

	if (ht != NULL)
		kd = g_hash_table_lookup(ht, key);

	if (kd == NULL) final
		*ans = mbb_xml_msg_error("unknown key");

	g_hash_table_remove(ht, key);

	mbb_plock_writer_unlock();
}

MBB_FUNC_REGISTER_STRUCT("mbb-get-auth-key", get_auth_key, MBB_CAP_ALL);
MBB_FUNC_REGISTER_STRUCT("mbb-show-auth-keys", show_auth_keys, MBB_CAP_WHEEL);
MBB_FUNC_REGISTER_STRUCT("mbb-drop-auth-key", drop_auth_key, MBB_CAP_WHEEL);

MBB_VAR_DEF(aklt_def) {
	.op_read = var_str_uint,
	.op_write = var_conv_uint,
	.cap_read = MBB_CAP_ALL,
	.cap_write = MBB_CAP_WHEEL
};

static void init_vars(void)
{
	mbb_base_var_register("auth.key.lifetime", &aklt_def, &authkey_lifetime);
	mbb_auth_method_register("key", mbb_auth_key_get_user);
}

static void __init init(void)
{
	static struct mbb_init_struct entries[] = {
		MBB_INIT_VARS,

		MBB_INIT_FUNC_STRUCT(get_auth_key),
		MBB_INIT_FUNC_STRUCT(show_auth_keys),
		MBB_INIT_FUNC_STRUCT(drop_auth_key)
	};

	mbb_init_pushv(entries, NELEM(entries));
}

