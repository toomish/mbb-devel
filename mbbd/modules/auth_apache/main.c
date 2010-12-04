/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbmodule.h"
#include "mbbauth.h"
#include "mbbuser.h"
#include "mbblog.h"
#include "mbbvar.h"

#include "macros.h"

#include "attr/interface.h"

#include <apr_md5.h>

#define AUTH_ATTR_NAME "auth.apache.hash"

static gpointer auth_method_key = NULL;
static gboolean auth_strict = TRUE;

static struct attr_interface *attr_lib;

static inline gint mbb_attr_read(gchar *group, gint id, struct attrvec *av,
				 gsize nelem, GError **error)
{
	return attr_lib->attr_readv(group, id, av, nelem, error);
}

static gboolean get_attr_value(gint id, gchar **value)
{
	GError *error = NULL;
	struct attrvec av;

	av.name = AUTH_ATTR_NAME;
	av.value = NULL;

	if (mbb_attr_read("user", id, &av, 1, &error) < 0) {
		mbb_log("mbb_attr_read user:%s, failed: %s",
			av.name, error->message);
		g_error_free(error);
		return FALSE;
	}

	if (av.value == NULL)
		return FALSE;

	*value = av.value;

	return TRUE;
}

static MbbUser *auth_apache(gchar *login, gchar *secret)
{
	apr_status_t status;
	MbbUser *user;
	gchar *hash;

	if (login == NULL || secret == NULL)
		return NULL;

	if ((user = mbb_user_get_by_name(login)) == NULL)
		return NULL;

	if (get_attr_value(user->id, &hash) == FALSE) {
		if (auth_strict)
			return NULL;

		return mbb_auth_plain(login, secret);
	}

	status = apr_password_validate(secret, hash);
	g_free(hash);

	if (status == APR_SUCCESS)
		return user;

	return NULL;
}

MBB_VAR_DEF(var_def) {
	.op_read = var_str_bool,
	.op_write = var_conv_bool,
	.cap_read = MBB_CAP_ALL,
	.cap_write = MBB_CAP_ADMIN
};

static void load_module(void)
{
	if ((attr_lib = mbb_module_import("attr.so")) == NULL) final
		mbb_log_self("import module attr.so failed");

	auth_method_key = mbb_auth_method_register("apache", auth_apache);
	if (auth_method_key == NULL) final
		mbb_log_self("failed to register auth method");

	mbb_module_add_base_var("auth.apache.strict", &var_def, &auth_strict);
}

static void unload_module(void)
{
	mbb_auth_method_unregister(auth_method_key);
}

MBB_DEFINE_MODULE("apache authorization method")
