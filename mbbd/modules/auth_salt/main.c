/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbmodule.h"
#include "mbbthread.h"
#include "mbbauth.h"
#include "mbbuser.h"
#include "mbblog.h"
#include "mbbvar.h"

#include "macros.h"
#include "debug.h"

#include <string.h>

#define AUTH_SALT_VAR MBB_SESSION_VAR_PREFIX "auth.salt.value"
#define SALTLEN 16

static gpointer auth_method_key = NULL;

static const GChecksumType checksum_type = G_CHECKSUM_MD5;
static gsize checksum_length;

static const gchar *auth_salt_type = "md5";

static inline gchar *salt_get(void)
{
	return mbb_var_cache_get(AUTH_SALT_VAR);
}

static inline void salt_set(gchar *salt)
{
	mbb_var_cache_add(g_strdup(AUTH_SALT_VAR), salt);
}

static gchar *salt_new(void)
{
	gchar *salt;
	GRand *rand;
	guint n;

	salt = g_malloc0(SALTLEN + 1);
	rand = g_rand_new();

	for (n = 0; n < SALTLEN; n++) {
		gchar ch;

		do {
			ch = (gchar) g_rand_int(rand);
		} while (g_ascii_isalnum(ch) == FALSE);

		salt[n] = ch;
	}

	g_rand_free(rand);

	salt_set(salt);

	return salt;
}

static MbbUser *auth_salt(gchar *login, gchar *secret)
{
	GChecksum *checksum;
	MbbUser *user;
	gchar *salt;

	salt = salt_get();
	if (secret == NULL || salt == NULL) {
		MbbMsgQueue *msg_queue;
		gchar *msg;

		if ((msg_queue = mbb_thread_get_msg_queue()) == NULL)
			return NULL;

		salt = salt_new();
		msg = g_strdup_printf("<salt>%s</salt>", salt);

		mbb_msg_queue_push_alloc(msg_queue, msg, strlen(msg));

		return NULL;
	}

	if (strlen(secret) != (gsize) checksum_length)
		return NULL;

	if ((user = mbb_user_get_by_name(login)) == NULL)
		return NULL;

	if ((checksum = g_checksum_new(checksum_type)) == NULL)
		return NULL;

	g_checksum_update(checksum, (guchar *) user->secret, -1);
	g_checksum_update(checksum, (guchar *) salt, -1);

	if (strcmp(secret, g_checksum_get_string(checksum)))
		user = NULL;

	g_checksum_free(checksum);

	return user;
}

MBB_VAR_DEF(var_def) {
	.op_read = var_str_str,
	.cap_read = MBB_CAP_ALL
};

static void load_module(void)
{
	gssize length = 2 * g_checksum_type_get_length(checksum_type);

	if (length < 0) final
		mbb_log_self("checksum algorithm %s not supported", auth_salt_type);

	checksum_length = length;

	auth_method_key = mbb_auth_method_register("salt", auth_salt);
	if (auth_method_key == NULL) final
		mbb_log_self("failed to register auth method");

	mbb_module_add_base_var("auth.salt.type", &var_def, &auth_salt_type);
}

static void unload_module(void)
{
	mbb_auth_method_unregister(auth_method_key);
}

MBB_DEFINE_MODULE("salt authorization method")
