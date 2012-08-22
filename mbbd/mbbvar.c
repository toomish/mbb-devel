/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbsession.h"
#include "mbbthread.h"
#include "mbbxmlmsg.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbbxtv.h"
#include "mbbvar.h"
#include "mbblog.h"

#include "mbbplock.h"

#include "variant.h"
#include "macros.h"
#include "xmltag.h"
#include "debug.h"
#include "re.h"

struct mbb_base_var {
	gpointer ptr;
	GStaticRWLock rwlock;
};

struct mbb_var_interface {
	void (*var_init)(struct mbb_var *, gpointer data);
	gchar *(*var_read)(struct mbb_var *var);
	gboolean (*var_write)(struct mbb_var *var, gchar *arg);
	void (*var_free)(struct mbb_var *var);
};

struct mbb_var {
	struct mbb_var_interface *vi;
	struct mbb_var_def *def;

	MbbModule *mod;
	gchar *name;
	gchar data[];
};

#define VAR_PRIV(var) ((gpointer) var->data)

static GHashTable *ht = NULL;
static GHashTable *ht_cache_global = NULL;

static gchar *mbb_var_read(struct mbb_var *var);

gpointer g_ptr_strdup(gpointer ptr)
{
	gchar **tmp;

	tmp = g_new(gchar *, 1);
	*tmp = g_strdup(*(gchar **) ptr);

	return tmp;
}

gpointer g_ptr_vardup(gpointer ptr)
{
	gchar **tmp;

	tmp = g_new(gchar *, 1);
	*tmp = mbb_var_read(ptr);

	return tmp;
}

void g_ptr_strfree(gpointer ptr)
{
	g_free(*(gchar **) ptr);
	g_free(ptr);
}

gpointer g_ptr_booldup(gpointer ptr)
{
	return g_memdup(ptr, sizeof(gboolean));
}

gchar *mbb_var_get_name(struct mbb_var *var)
{
	return var->name;
}

gpointer mbb_var_get_priv(struct mbb_var *var)
{
	return VAR_PRIV(var);
}

gboolean mbb_var_has_mod(struct mbb_var *var)
{
	return var->mod != NULL;
}

mbb_cap_t mbb_var_get_cap_read(struct mbb_var *var)
{
	return var->def->cap_read;
}

mbb_cap_t mbb_var_get_cap_write(struct mbb_var *var)
{
	return var->def->cap_write;
}

static void mbb_var_set_module(struct mbb_var *var, MbbModule *mod)
{
	var->mod = mod;
}

static inline gboolean mbb_var_is_alive(struct mbb_var *var)
{
	return var->mod == NULL ? TRUE : mbb_module_isrun(var->mod);
}

static gboolean mbb_var_can_read(struct mbb_var *var, mbb_cap_t cap)
{
	if (var->def->op_read == NULL)
		return FALSE;

	return mbb_cap_check(cap, var->def->cap_read);
}

static gboolean mbb_var_is_readonly(struct mbb_var *var)
{
	return var->def->op_write == NULL;
}

static gboolean mbb_var_can_write(struct mbb_var *var, mbb_cap_t cap)
{
	if (mbb_var_is_readonly(var))
		return FALSE;

	return mbb_cap_check(cap, var->def->cap_write);
}

static gchar *mbb_var_read(struct mbb_var *var)
{
	return var->vi->var_read(var);
}

static gboolean mbb_var_write(struct mbb_var *var, gchar *arg)
{
	return var->vi->var_write(var, arg);
}

void mbb_var_free(struct mbb_var *var)
{
	g_free(var->name);
	if (var->vi->var_free != NULL)
		var->vi->var_free(var);
	g_free(var);
}

static void mbb_base_var_init(struct mbb_var *var, gpointer data)
{
	struct mbb_base_var *priv = VAR_PRIV(var);

	priv->ptr = data;
	g_static_rw_lock_init(&priv->rwlock);
}

static gchar *mbb_base_var_read(struct mbb_var *var)
{
	struct mbb_base_var *priv = VAR_PRIV(var);
	gchar *str;

	g_static_rw_lock_reader_lock(&priv->rwlock);
	str = var->def->op_read(priv->ptr);
	g_static_rw_lock_reader_unlock(&priv->rwlock);

	return str;
}

static gboolean mbb_base_var_write(struct mbb_var *var, gchar *arg)
{
	struct mbb_base_var *priv = VAR_PRIV(var);
	gboolean ret;

	g_static_rw_lock_writer_lock(&priv->rwlock);
	ret = var->def->op_write(arg, priv->ptr);
	g_static_rw_lock_writer_unlock(&priv->rwlock);

	return ret;
}

static void mbb_base_var_free(struct mbb_var *var)
{
	struct mbb_base_var *priv = VAR_PRIV(var);

	g_static_rw_lock_free(&priv->rwlock);
}

static void mbb_session_var_init(struct mbb_var *var, gpointer data)
{
	struct mbb_session_var *priv = VAR_PRIV(var);

	*priv = *(struct mbb_session_var *) data;
	mbb_session_add_var(var);
}

static gchar *mbb_session_var_read(struct mbb_var *var)
{
	gpointer ptr;

	ptr = mbb_session_var_get_data(var);

	return var->def->op_read(ptr);
}

static gboolean mbb_session_var_write(struct mbb_var *var, gchar *arg)
{
	gpointer ptr;

	ptr = mbb_session_var_get_data(var);

	return var->def->op_write(arg, ptr);
}

static struct mbb_var_interface mbb_base_var_interface = {
	.var_init = mbb_base_var_init,
	.var_read = mbb_base_var_read,
	.var_write = mbb_base_var_write,
	.var_free = mbb_base_var_free
};

static struct mbb_var_interface mbb_session_var_interface = {
	.var_init = mbb_session_var_init,
	.var_read = mbb_session_var_read,
	.var_write = mbb_session_var_write,
	.var_free = mbb_session_del_var
};

static gboolean mbb_var_is_base(struct mbb_var *var)
{
	return var->vi == &mbb_base_var_interface;
}

static gboolean mbb_var_is_session(struct mbb_var *var)
{
	return var->vi == &mbb_session_var_interface;
}

static struct mbb_var *mbb_var_new(gchar *name, struct mbb_var_interface *vi,
				   struct mbb_var_def *def, gsize size)
{
	struct mbb_var *var;

	var = g_malloc(sizeof(struct mbb_var) + size);
	var->vi = vi;
	var->def = def;
	var->name = g_strdup(name);
	var->mod = NULL;

	return var;
}

static void mbb_var_init(struct mbb_var *var, gpointer data)
{
	if (var->vi->var_init != NULL)
		var->vi->var_init(var, data);
}

struct mbb_var *mbb_base_var_new(gchar *name, struct mbb_var_def *def, gpointer ptr)
{
	struct mbb_var *var;

	var = mbb_var_new(name, &mbb_base_var_interface, def,
		sizeof(struct mbb_base_var)
	);

	mbb_var_init(var, ptr);

	return var;
}

void mbb_base_var_lock(struct mbb_var *var)
{
	struct mbb_base_var *priv = VAR_PRIV(var);

	g_static_rw_lock_reader_lock(&priv->rwlock);
}

void mbb_base_var_unlock(struct mbb_var *var)
{
	struct mbb_base_var *priv = VAR_PRIV(var);

	g_static_rw_lock_reader_unlock(&priv->rwlock);
}

struct mbb_var *mbb_session_var_new(gchar *name, struct mbb_var_def *def,
				    struct mbb_session_var *priv)
{
	struct mbb_var *var;

	var = mbb_var_new(name, &mbb_session_var_interface, def,
		sizeof(struct mbb_session_var)
	);

	mbb_var_init(var, priv);

	return var;
}

GQuark mbb_var_error_quark(void)
{
	return g_quark_from_static_string("mbb-var-error-quark");
}

struct mbb_var *mbb_var_register(struct mbb_var *var)
{
	if (ht == NULL)
		ht = g_hash_table_new(g_str_hash, g_str_equal);

	if (g_hash_table_lookup(ht, var->name) != NULL)
		msg_warn("duplicate var '%s'", var->name);
	else
		g_hash_table_insert(ht, var->name, var);

	return var;
}

gboolean mbb_var_mregister(struct mbb_var *var, MbbModule *mod)
{
	mbb_plock_writer_lock();

	if (ht == NULL)
		ht = g_hash_table_new(g_str_hash, g_str_equal);
	else if (g_hash_table_lookup(ht, var->name) != NULL) {
		mbb_plock_writer_unlock();
		mbb_log_self("register var '%s' failed", var->name);
		return FALSE;
	}

	mbb_var_set_module(var, mod);

	if (mbb_var_is_base(var) && mbb_var_can_write(var, MBB_CAP_ROOT)) {
		gchar *value = NULL;

		if (! g_str_has_prefix(var->name, MBB_SESSION_VAR_PREFIX)) {
			gchar *name;

			name = g_strdup_printf(
				"%s%s", MBB_SESSION_VAR_PREFIX, var->name
			);

			value = mbb_var_cache_get(name);
			g_free(name);
		}

		if (value == NULL)
			value = mbb_var_cache_get(var->name);

		if (value != NULL) {
			if (! mbb_base_var_write(var, value))
				mbb_log_self("var %s: invalid cache value", var->name);
		}
	}

	g_hash_table_insert(ht, var->name, var);

	mbb_plock_writer_unlock();

	return TRUE;
}

static struct mbb_var *mbb_var_get_by_name(gchar *name)
{
	struct mbb_var *var;

	if (ht == NULL)
		return NULL;

	if ((var = g_hash_table_lookup(ht, name)) == NULL)
		return NULL;

	if (mbb_var_is_alive(var) == FALSE)
		return NULL;

	return var;
}

void mbb_var_unregister(struct mbb_var *var)
{
	mbb_plock_writer_lock();

	if (ht != NULL)
		g_hash_table_remove(ht, var->name);

	mbb_plock_writer_unlock();
}

gboolean mbb_var_assign(gchar *varname, gchar *value, GError **error)
{
	struct mbb_var *var;

	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	if ((var = mbb_var_get_by_name(varname)) == NULL) {
		g_set_error(error,
			MBB_VAR_ERROR, MBB_VAR_ERROR_UNKNOWN, "unknown"
		);
		return FALSE;
	}

	if (mbb_var_is_readonly(var)) {
		g_set_error(error,
			MBB_VAR_ERROR, MBB_VAR_ERROR_READONLY, "readonly"
		);
		return FALSE;
	}

	if (mbb_var_is_session(var)) {
		g_set_error(error,
			MBB_VAR_ERROR, MBB_VAR_ERROR_SESSION, "sessional"
		);
		return FALSE;
	}

	if (mbb_var_write(var, value) == FALSE) {
		g_set_error(error,
			MBB_VAR_ERROR, MBB_VAR_ERROR_INVALID, "invalid value"
		);
		return FALSE;
	}

	return TRUE;
}

static void gather_var_name(gpointer key G_GNUC_UNUSED, gpointer value, gpointer data)
{
	struct mbb_var *var;
	XmlTag *tag;

	var = (struct mbb_var *) value;
	tag = (XmlTag *) data;

	if (mbb_var_is_alive(var)) {
		XmlTag *xt;

		xt = xml_tag_new_child(tag, "var",
			"name", variant_new_string(var->name)
		);

		if (var->mod != NULL) {
			gchar *tmp = mbb_module_get_name(var->mod);
			xml_tag_set_attr(xt, "module", variant_new_string(tmp));
		}
	}
}

static void var_list(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	*ans = mbb_xml_msg_ok();

	mbb_plock_reader_lock();
	if (ht != NULL)
		g_hash_table_foreach(ht, gather_var_name, *ans);
	mbb_plock_reader_unlock();
}

static void var_show(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_VAR_NAME);

	struct mbb_var *var;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_plock_reader_lock();

	on_final { mbb_plock_reader_unlock(); }

	if ((var = mbb_var_get_by_name(name)) == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_VAR);

	if (! mbb_var_can_read(var, mbb_thread_get_cap())) final
		*ans = mbb_xml_msg(MBB_MSG_NOT_CAPABLE);

	*ans = mbb_xml_msg_ok();
	xml_tag_new_child(*ans, "var",
		"value", variant_new_alloc_string(mbb_var_read(var))
	);

	mbb_plock_reader_unlock();
}

static void var_set(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_VAR_NAME, XTV_VAR_VALUE);

	struct mbb_var *var;
	gchar *value;
	gchar *name;

	MBB_XTV_CALL(&name, &value);

	mbb_plock_reader_lock();

	on_final { mbb_plock_reader_unlock(); }

	if ((var = mbb_var_get_by_name(name)) == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_VAR);

	if (! mbb_var_can_write(var, mbb_thread_get_cap())) final
		*ans = mbb_xml_msg(MBB_MSG_NOT_CAPABLE);

	if (mbb_var_write(var, value) == FALSE) final
		*ans = mbb_xml_msg(MBB_MSG_INVALID_VALUE);

	mbb_plock_reader_unlock();
}

static void show_vars(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	DEFINE_XTV(XTV_REGEX_VALUE_);

	GHashTableIter iter;
	struct mbb_var *var;
	mbb_cap_t cap;
	Regex re = NULL;

	MBB_XTV_CALL(&re);

	*ans = mbb_xml_msg_ok();

	mbb_plock_reader_lock();

	if (ht == NULL) final
		mbb_plock_reader_unlock();

	cap = mbb_thread_get_cap();
	g_hash_table_iter_init(&iter, ht);
	while (g_hash_table_iter_next(&iter, NULL, (gpointer *) &var)) {
		gchar *value;

		if (mbb_var_is_alive(var) == FALSE)
			continue;

		if (re != NULL && ! re_ismatch(re, var->name))
			continue;

		if (mbb_var_can_read(var, cap) == FALSE)
			continue;

		value = mbb_var_read(var);
		xml_tag_new_child(*ans, "var",
			"name", variant_new_string(var->name),
			"value", variant_new_alloc_string(value)
		);
	}

	mbb_plock_reader_unlock();

	re_free(re);
}

static GHashTable *mbb_var_get_cache_table(gchar *name, gboolean create)
{
	if (g_str_has_prefix(name, MBB_SESSION_VAR_PREFIX))
		return mbb_session_get_cached(create);

	if (ht_cache_global == NULL && create) {
		ht_cache_global = g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, g_free
		);
	}

	return ht_cache_global;
}

void mbb_var_cache_add(gchar *name, gchar *value)
{
	GHashTable *cache = mbb_var_get_cache_table(name, TRUE);

	if (cache != NULL) {
		g_hash_table_replace(cache, name, value);
	}
}

void mbb_var_cache_save(struct mbb_var *var)
{
	gchar *value;

	if ((value = mbb_var_read(var)) == NULL)
		return;

	mbb_plock_writer_lock();
	mbb_var_cache_add(g_strdup(var->name), value);
	mbb_plock_writer_unlock();
}

void mbb_var_cache_load(struct mbb_var *var)
{
	gchar *value;

	if (mbb_var_is_readonly(var))
		return;

	mbb_plock_reader_lock();

	if ((value = mbb_var_cache_get(var->name)) != NULL)
		mbb_var_write(var, value);

	mbb_plock_reader_unlock();
}

gchar *mbb_var_cache_get(gchar *name)
{
	GHashTable *cache = mbb_var_get_cache_table(name, FALSE);

	if (cache == NULL)
		return NULL;

	return g_hash_table_lookup(cache, name);
}

static void var_cache_add(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_VAR_NAME, XTV_VAR_VALUE_);

	gchar *value = NULL;
	gchar *name;

	MBB_XTV_CALL(&name, &value);

	mbb_plock_writer_lock();

	on_final { mbb_plock_writer_unlock(); }

	if (value != NULL)
		value = g_strdup(value);
	else {
		struct mbb_var *var;

		if ((var = mbb_var_get_by_name(name)) == NULL) final
			*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_VAR);

		value = mbb_var_read(var);
	}

	if (value != NULL)
		mbb_var_cache_add(g_strdup(name), value);

	mbb_plock_writer_unlock();
}

static void var_cache_load(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_VAR_NAME);

	struct mbb_var *var;
	gchar *value;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_plock_reader_lock();

	on_final { mbb_plock_reader_unlock(); }

	value = mbb_var_cache_get(name);
	if (value == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_VAR);

	if ((var = mbb_var_get_by_name(name)) == NULL) final
		*ans = mbb_xml_msg_error("similar var missed");

	if (mbb_var_is_readonly(var)) final
		*ans = mbb_xml_msg_error("similar var is readonly");

	if (mbb_var_write(var, value) == FALSE) final
		*ans = mbb_xml_msg(MBB_MSG_INVALID_VALUE);

	mbb_plock_reader_unlock();
}

static void var_cache_del(XmlTag *tag, XmlTag **ans G_GNUC_UNUSED)
{
	DEFINE_XTV(XTV_VAR_NAME);

	GHashTable *cache;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_plock_writer_lock();

	cache = mbb_var_get_cache_table(name, FALSE);

	if (cache != NULL)
		g_hash_table_remove(cache, name);

	mbb_plock_writer_unlock();
}

static void var_cache_show(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_VAR_NAME);

	gchar *value;
	gchar *name;

	MBB_XTV_CALL(&name);

	mbb_plock_reader_lock();

	on_final { mbb_plock_reader_unlock(); }

	value = mbb_var_cache_get(name);
	if (value == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_VAR);

	*ans = mbb_xml_msg_ok();
	xml_tag_new_child(*ans, "var", "value", variant_new_string(value));

	mbb_plock_reader_unlock();
}

static void var_cache_list_gather(gpointer key, gpointer value G_GNUC_UNUSED, gpointer data)
{
	xml_tag_new_child(data, "var", "name", variant_new_string(key));
}

static void var_cache_list(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	GHashTable *cache;

	*ans = mbb_xml_msg_ok();

	mbb_plock_reader_lock();

	if (ht_cache_global != NULL)
		g_hash_table_foreach(ht_cache_global, var_cache_list_gather, *ans);

	if ((cache = mbb_session_get_cached(FALSE)) != NULL)
		g_hash_table_foreach(cache, var_cache_list_gather, *ans);

	mbb_plock_reader_unlock();
}

static void show_cached_gather(gpointer key, gpointer value, gpointer data)
{
	xml_tag_new_child(data, "var",
		"name", variant_new_string(key),
		"value", variant_new_string(value)
	);
}

static void show_cached(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	GHashTable *cache;

	*ans = mbb_xml_msg_ok();

	mbb_plock_reader_lock();

	if (ht_cache_global != NULL)
		g_hash_table_foreach(ht_cache_global, show_cached_gather, *ans);

	if ((cache = mbb_session_get_cached(FALSE)) != NULL)
		g_hash_table_foreach(cache, show_cached_gather, *ans);

	mbb_plock_reader_unlock();
}

MBB_FUNC_REGISTER_STRUCT("mbb-var-list", var_list, MBB_CAP_ALL);
MBB_FUNC_REGISTER_STRUCT("mbb-var-show", var_show, MBB_CAP_ALL);
MBB_FUNC_REGISTER_STRUCT("mbb-var-set", var_set, MBB_CAP_ALL);
MBB_FUNC_REGISTER_STRUCT("mbb-show-vars", show_vars, MBB_CAP_ALL);

MBB_FUNC_REGISTER_STRUCT("mbb-var-cache-add", var_cache_add, MBB_CAP_WHEEL);
MBB_FUNC_REGISTER_STRUCT("mbb-var-cache-load", var_cache_load, MBB_CAP_WHEEL);
MBB_FUNC_REGISTER_STRUCT("mbb-var-cache-del", var_cache_del, MBB_CAP_WHEEL);
MBB_FUNC_REGISTER_STRUCT("mbb-var-cache-show", var_cache_show, MBB_CAP_ALL);
MBB_FUNC_REGISTER_STRUCT("mbb-var-cache-list", var_cache_list, MBB_CAP_ALL);
MBB_FUNC_REGISTER_STRUCT("mbb-show-cached", show_cached, MBB_CAP_ALL);

static void __init init(void)
{
	static struct mbb_init_struct entries[] = {
		MBB_INIT_FUNC_STRUCT(var_list),
		MBB_INIT_FUNC_STRUCT(var_show),
		MBB_INIT_FUNC_STRUCT(var_set),
		MBB_INIT_FUNC_STRUCT(show_vars),

		MBB_INIT_FUNC_STRUCT(var_cache_add),
		MBB_INIT_FUNC_STRUCT(var_cache_load),
		MBB_INIT_FUNC_STRUCT(var_cache_del),
		MBB_INIT_FUNC_STRUCT(var_cache_show),
		MBB_INIT_FUNC_STRUCT(var_cache_list),
		MBB_INIT_FUNC_STRUCT(show_cached)
	};

	mbb_init_pushv(entries, NELEM(entries));
}

