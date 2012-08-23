/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbmodule.h"
#include "mbbxmlmsg.h"
#include "mbbgroup.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbblock.h"
#include "mbbcap.h"
#include "mbbvar.h"
#include "mbbxtv.h"
#include "mbblog.h"

#include "mbbplock.h"

#include "datalink.h"
#include "varconv.h"
#include "macros.h"
#include "xmltag.h"

#include <string.h>
#include <time.h>
#include <dlfcn.h>
#include <glib.h>

enum {
	MBB_MODULE_SELF,
	MBB_MODULE_NAME,
	MBB_MODULE_HANDLE,
	MBB_MODULE_INFO
};

struct mbb_module {
	struct mbb_module_info *info;
	void *handle;
	gchar *name;

	volatile gint use_count;
	volatile gboolean run;
	time_t start;

	struct mbb_func_struct *functions;
	GSList *vars;

	gsize total;
	gsize nreg;

	gpointer *export;
	GSList *deps;

	GStaticRWLock rwlock;
};

static struct data_link_entry dl_entries[] = {
	DATA_LINK_SELF_ENTRY,
	{ G_STRUCT_OFFSET(struct mbb_module, name), g_pstr_hash, g_pstr_equal },
	{ G_STRUCT_OFFSET(struct mbb_module, handle), g_ptr_hash, g_ptr_equal },
	{ G_STRUCT_OFFSET(struct mbb_module, info), g_ptr_hash, g_ptr_equal }
};

static DataLink *dlink = NULL;

static gchar *module_dir__ = NULL;

static struct mbb_var *md_var = NULL;

static MbbModuleInfo *current_module_info;
static GStaticPrivate mod_key = G_STATIC_PRIVATE_INIT;

static void mbb_module_free(MbbModule *module);

GQuark mbb_module_error_quark(void)
{
	return g_quark_from_static_string("mbb-module-error-quark");
}

static inline void dlink_init(void)
{
	dlink = data_link_new_full(
		dl_entries, NELEM(dl_entries), (GDestroyNotify) mbb_module_free
	);
}

gchar *mbb_module_get_name(MbbModule *mod)
{
	return mod->name;
}

MbbModule *mbb_module_current(void)
{
	MbbModuleInfo *mod_info;
	
	mod_info = current_module_info;
	if (mod_info != NULL)
		return mod_info->module;

	return NULL;
}

static MbbModule *mbb_module_new(gchar *name, void *handle, MbbModuleInfo *info)
{
	MbbModule *module;

	module = g_new0(MbbModule, 1);
	module->handle = handle;
	module->info = info;
	module->name = g_strdup(name);

	g_static_rw_lock_init(&module->rwlock);
	time(&module->start);

	return module;
}

MbbModule *mbb_module_last_used(void)
{
	return g_static_private_get(&mod_key);
}

static gboolean _mbb_module_use(MbbModule *mod, gboolean setkey)
{
	if (mod == NULL)
		return TRUE;

	g_static_rw_lock_reader_lock(&mod->rwlock);
	if (mod->run == FALSE) {
		g_static_rw_lock_reader_unlock(&mod->rwlock);
		return FALSE;
	}

	if (setkey)
		g_static_private_set(&mod_key, mod, NULL);

	g_atomic_int_inc(&mod->use_count);
	return TRUE;
}

gboolean mbb_module_use(MbbModule *mod)
{
	return _mbb_module_use(mod, TRUE);
}

static void _mbb_module_unuse(MbbModule *mod, gboolean delkey)
{
	if (mod != NULL) {
		if (delkey)
			g_static_private_set(&mod_key, NULL, NULL);

		(void) g_atomic_int_dec_and_test(&mod->use_count);
		g_static_rw_lock_reader_unlock(&mod->rwlock);
	}
}

void mbb_module_unuse(MbbModule *mod)
{
	_mbb_module_unuse(mod, TRUE);
}

void mbb_module_push_info(MbbModuleInfo *mod_info)
{
	current_module_info = mod_info;

	if (mod_info != NULL)
		mod_info->module = NULL;
}

gboolean mbb_module_isrun(MbbModule *mod)
{
	return mod->run;
}

static inline MbbModule *mbb_module_get_by_name(gchar *name)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_MODULE_NAME, &name);
}

static inline MbbModule *mbb_module_get_by_handle(gpointer *handle)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_MODULE_HANDLE, &handle);
}

static inline MbbModule *mbb_module_get_by_info(MbbModuleInfo *info)
{
	if (dlink == NULL)
		return NULL;

	return data_link_lookup(dlink, MBB_MODULE_INFO, &info);
}

static void mbb_module_free(MbbModule *module)
{
	g_static_rw_lock_free(&module->rwlock);
	g_slist_free(module->vars);
	g_slist_free(module->deps);
	g_free(module->name);
	g_free(module);
}

static inline void mbb_module_add(MbbModule *module)
{
	if (dlink == NULL)
		dlink_init();

	data_link_insert(dlink, module);
}

static inline void mbb_module_del(MbbModule *module)
{
	if (dlink != NULL)
		data_link_remove(dlink, MBB_MODULE_SELF, module);
}

static void mbb_module_reg_data(MbbModule *mod)
{
	GSList *list;

	if (mod->functions != NULL)
		mod->nreg += mbb_func_mregister(mod->functions, mod);

	for (list = mod->vars; list != NULL; list = list->next) {
		if (mbb_var_mregister(list->data, mod))
			mod->nreg++;
	}

	if (mod->total == mod->nreg)
		mod->run = TRUE;
}

static void mbb_module_unreg_data(MbbModule *mod)
{
	GSList *list;

	if (mod->functions != NULL)
		mbb_func_unregister_all(mod->functions);

	for (list = mod->vars; list != NULL; list = list->next) {
		struct mbb_var *var = list->data;

		if (mbb_var_has_mod(var))
			mbb_var_unregister(var);

		mbb_var_free(var);
	}

	for (list = mod->deps; list != NULL; list = list->next)
		_mbb_module_unuse(list->data, FALSE);
}

void mbb_module_add_functions(struct mbb_func_struct *fs)
{
	MbbModule *module = mbb_module_current();

	if (module != NULL && module->functions == NULL) {
		module->functions = fs;

		for (; fs->name != NULL; fs++)
			module->total++;
	}
}

struct mbb_var *mbb_module_add_var(struct mbb_var *var)
{
	MbbModule *module = mbb_module_current();

	if (module != NULL) {
		module->vars = g_slist_prepend(module->vars, var);
		module->total++;
	}

	return var;
}

void mbb_module_export(gpointer data)
{
	MbbModule *module = mbb_module_current();

	if (module != NULL) {
		if (module->export != NULL)
			mbb_log_self("mbb_module_export extra call");
		else
			module->export = data;
	}
}

gpointer mbb_module_import(gchar *name)
{
	MbbModule *cur = mbb_module_current();
	MbbModule *mod;

	if (cur == NULL)
		return NULL;

	mod = mbb_module_get_by_name(name);
	if (mod == NULL)
		return NULL;

	if (mod->export != NULL) {
		cur->deps = g_slist_prepend(cur->deps, mod);
		_mbb_module_use(mod, FALSE);
	}

	return mod->export;
}

static inline gboolean has_mod_suffix(const gchar *name)
{
	return g_str_has_suffix(name, ".so");
}

static gboolean check_mod_name(gchar *name, GError **error)
{
	if (strchr(name, '/') != NULL) {
		g_set_error(error, MBB_MODULE_ERROR, MBB_MODULE_ERROR_UNKNOWN,
			"not permitted char '/'");
		return FALSE;
	}

	if (! has_mod_suffix(name)) {
		g_set_error(error, MBB_MODULE_ERROR, MBB_MODULE_ERROR_UNKNOWN,
			"missed '.so' prefix");
		return FALSE;
	}

	return TRUE;
}

static gchar *get_path_for_name(gchar *name, GError **error)
{
	gchar **mod_dir;
	gchar *path;

	mod_dir = mbb_session_var_get_data(md_var);
	if (mod_dir == NULL)
		mod_dir = &module_dir__;

	if (g_ptr_str_is_null(mod_dir)) {
		g_set_error(error, MBB_MODULE_ERROR, MBB_MODULE_ERROR_UNKNOWN,
			"session var '%s' is null", mbb_var_get_name(md_var)
		);
		return NULL;
	}

	path = g_strdup_printf("%s%s", *mod_dir, name);

	return path;
}

static inline void mbb_dlclose(void *handle, gchar *name)
{
	mbb_log_debug("dlclose %s", name);
	if (dlclose(handle) < 0)
		mbb_log_debug("dlclose %s failed: %s", name, dlerror());
}

static gboolean check_mod_loaded(void *handle, gchar *name, GError **error)
{
	MbbModuleInfo *mod_info = current_module_info;
	MbbModule *mod;

	if (mod_info == NULL)
		g_set_error(error, MBB_MODULE_ERROR, MBB_MODULE_ERROR_FOREIGN,
			"foreign module");
	else if (mod_info->load == NULL)
		g_set_error(error, MBB_MODULE_ERROR, MBB_MODULE_ERROR_INVALID,
			"invalid module: load function is NULL");
	else if ((mod = mbb_module_get_by_handle(handle)) != NULL)
		g_set_error(error, MBB_MODULE_ERROR, MBB_MODULE_ERROR_LOADED,
			"module %s loaded as %s", name, mod->name);
	else if ((mod = mbb_module_get_by_info(mod_info)) != NULL)
		g_set_error(error, MBB_MODULE_ERROR, MBB_MODULE_ERROR_LOADED,
			"module %s has the same data with %s", name, mod->name);
	else
		return TRUE;

	return FALSE;
}

gboolean mbb_module_load(gchar *name, GError **error)
{
	MbbModuleInfo *mod_info;
	MbbModule *module;
	void *handle;
	gchar *path = NULL;

	if (! check_mod_name(name, error))
		return FALSE;

	if ((path = get_path_for_name(name, error)) == NULL)
		return FALSE;

	mbb_plock_writer_lock();

	if (mbb_module_get_by_name(name) != NULL) {
		mbb_plock_writer_unlock();
		g_set_error(error,
			MBB_MODULE_ERROR, MBB_MODULE_ERROR_EXISTS,
			"module name %s exists", name);
		g_free(path);
		return FALSE;
	}

	handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	g_free(path);
	if (handle == NULL) {
		g_set_error(error,
			MBB_MODULE_ERROR, MBB_MODULE_ERROR_OPEN, "%s", dlerror());
		mbb_plock_writer_unlock();
		return FALSE;
	}

	mbb_log_debug("dlopen %s", name);

	if (! check_mod_loaded(handle, name, error)) {
		mbb_module_push_info(NULL);
		mbb_dlclose(handle, name);
		mbb_plock_writer_unlock();
		return FALSE;
	}

	mod_info = current_module_info;
	module = mbb_module_new(name, handle, mod_info);
	mod_info->module = module;
	mod_info->load();
	mbb_module_push_info(NULL);

	if (module->total == 0) {
		if (mod_info->unload != NULL)
			mod_info->unload();

		mbb_dlclose(handle, name);
		mbb_module_free(module);
		mbb_plock_writer_unlock();
		g_set_error(error, MBB_MODULE_ERROR, MBB_MODULE_ERROR_EMPTY,
			"void module");
		return FALSE;
	}

	g_static_rw_lock_writer_lock(&module->rwlock);
	mbb_module_reg_data(module);
	mbb_module_add(module);
	g_static_rw_lock_writer_unlock(&module->rwlock);

	mbb_plock_writer_unlock();

	return TRUE;
}

gboolean mbb_module_unload(gchar *name, GError **error)
{
	MbbModule *mod;

	if (! check_mod_name(name, error))
		return FALSE;

	mbb_plock_writer_lock();

	if ((mod = mbb_module_get_by_name(name)) == NULL) {
		mbb_plock_writer_unlock();
		g_set_error(error, MBB_MODULE_ERROR, MBB_MODULE_ERROR_UNKNOWN,
			"unknown module %s", name);
		return FALSE;
	}

	if (! g_static_rw_lock_writer_trylock(&mod->rwlock)) {
		mbb_plock_writer_unlock();
		g_set_error(error, MBB_MODULE_ERROR, MBB_MODULE_ERROR_USED,
			"module referenced %d times", mod->use_count);
		return FALSE;
	}

	mod->run = FALSE;
	g_static_rw_lock_writer_unlock(&mod->rwlock);

	mbb_module_unreg_data(mod);
	if (mod->info->unload != NULL)
		mod->info->unload();

	mbb_dlclose(mod->handle, mod->name);
	mbb_module_del(mod);

	mbb_plock_writer_unlock();

	return TRUE;
}

static void read_dir_entries(GDir *dir, GString *path, XmlTag *tag)
{
	gchar *entry;
	gsize len;

	len = path->len;

	if (len && path->str[len - 1] != '/') {
		g_string_append_c(path, '/');
		len++;
	}

	while ((entry = (gchar *) g_dir_read_name(dir)) != NULL) {
		if (! has_mod_suffix(entry))
			continue;

		g_string_truncate(path, len);
		g_string_append(path, entry);

		if (g_file_test(path->str, G_FILE_TEST_IS_REGULAR))
			xml_tag_add_child(tag, xml_tag_newc(
				"module", "name", variant_new_string(entry)
			));
	}
}

static void module_list(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	GError *error = NULL;
	gchar **mod_dir;
	GString *path;
	GDir *dir;

	mod_dir = mbb_session_var_get_data(md_var);
	if (g_ptr_str_is_null(mod_dir)) final
		*ans = mbb_xml_msg(MBB_MSG_VAR_NOT_INIT, mbb_var_get_name(md_var));

	dir = g_dir_open(*mod_dir, 0, &error);

	if (dir != NULL)
		path = g_string_new(*mod_dir);
	else final
		*ans = mbb_xml_msg_from_error(error);

	*ans = mbb_xml_msg_ok();
	read_dir_entries(dir, path, *ans);

	g_string_free(path, TRUE);
	g_dir_close(dir);
}

static void module_load(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_MODULE_NAME);

	GError *error = NULL;
	gchar *name;

	MBB_XTV_CALL(&name);

	if (! mbb_module_load(name, &error))
		*ans = mbb_xml_msg_from_error(error);
}

static void module_unload(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_MODULE_NAME);

	GError *error = NULL;
	gchar *name;

	MBB_XTV_CALL(&name);

	if (! mbb_module_unload(name, &error))
		*ans = mbb_xml_msg_from_error(error);
}

static void gather_module(MbbModule *mod, XmlTag *tag)
{
	gchar *state;

	g_static_rw_lock_reader_lock(&mod->rwlock);

	state = mod->run ? "run" : "stop";

	xml_tag_add_child(tag, xml_tag_newc(
		"module",
		"name", variant_new_string(mod->name),
		"state", variant_new_static_string(state),
		"start", variant_new_long(mod->start),
		"total", variant_new_int(mod->total),
		"nreg", variant_new_int(mod->nreg),
		"nuse", variant_new_int(mod->use_count)
	));

	g_static_rw_lock_reader_unlock(&mod->rwlock);
}

static void show_modules(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	mbb_plock_reader_lock();

	*ans = mbb_xml_msg_ok();
	data_link_foreach(dlink, (GFunc) gather_module, *ans);

	mbb_plock_reader_unlock();
}

static inline gchar *bool2str(gboolean val)
{
	return val ? "true" : "false";
}

static inline void add_group_tags(mbb_cap_t mask, XmlTag *tag)
{
	MbbGroup *group;
	gint id;

	mbb_cap_for_each(id, mask) {
		if ((group = mbb_group_get_by_id(id)) != NULL)
			xml_tag_add_child(tag, xml_tag_newc(
				"group", "name", variant_new_string(group->name)
			));
	}

	if (id == MBB_CAP_DUMMY_ID)
		xml_tag_add_child(tag, xml_tag_newc(
			"group", "name", variant_new_static_string("dummy")
		));
}

static void module_info(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_MODULE_NAME);

	gboolean with_groups;
	MbbModule *mod;
	GSList *list;
	gchar *name;
	XmlTag *xt;

	MBB_XTV_CALL(&name);

	if (! check_mod_name(name, NULL)) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_MODULE);

	with_groups = xml_tag_get_attr(tag, "with_groups") != NULL;

	mbb_plock_reader_lock();

	on_final { mbb_plock_reader_unlock(); }

	mod = mbb_module_get_by_name(name);
	if (mod == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_MODULE);

	*ans = mbb_xml_msg_ok();

	if (mod->info->desc != NULL) {
		xml_tag_add_child(*ans, xml_tag_new(
			"desc", NULL, variant_new_string(mod->info->desc)
		));
	}

	if (with_groups) mbb_lock_reader_lock();

	if (mod->functions != NULL) {
		struct mbb_func_struct *func = mod->functions;

		for (; func->name != NULL; func++) {
			gboolean isreg = func->module != NULL;

			xt = xml_tag_new_child(*ans, "func",
				"name", variant_new_string(func->name),
				"reg", variant_new_static_string(bool2str(isreg))
			);

			if (with_groups)
				add_group_tags(func->cap_mask, xt);
		}
	}

	for (list = mod->vars; list != NULL; list = list->next) {
		struct mbb_var *var = list->data;

		xt = xml_tag_new_child(*ans, "var",
			"name", variant_new_string(mbb_var_get_name(var)),
			"reg", variant_new_static_string(
				bool2str(mbb_var_has_mod(var))
			)
		);

		if (with_groups) {
			add_group_tags(mbb_var_get_cap_read(var),
				xml_tag_new_child(xt, "read"));
			add_group_tags(mbb_var_get_cap_write(var),
				xml_tag_new_child(xt, "write"));
		}
	}

	if (with_groups) mbb_lock_reader_unlock();

	mbb_plock_reader_unlock();
}

static void module_do_run(XmlTag *tag, XmlTag **ans, gboolean run)
{
	DEFINE_XTV(XTV_MODULE_NAME);

	MbbModule *mod;
	gchar *name;

	MBB_XTV_CALL(&name);

	if (! check_mod_name(name, NULL)) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_MODULE);

	mbb_plock_reader_lock();

	on_final { mbb_plock_reader_unlock(); }

	mod = mbb_module_get_by_name(name);
	if (mod == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_MODULE);

	if (mod->run == run) final
		*ans = mbb_xml_msg_error(
			"module is already %s", run ? "run" : "stop"
		);

	mod->run = run;

	mbb_plock_reader_unlock();
}

static void module_run(XmlTag *tag, XmlTag **ans)
{
	module_do_run(tag, ans, TRUE);
}

static void module_stop(XmlTag *tag, XmlTag **ans)
{
	module_do_run(tag, ans, FALSE);
}

static void module_vars_do(XmlTag *tag, XmlTag **ans, void (*func)(struct mbb_var *))
{
	DEFINE_XTV(XTV_MODULE_NAME);

	MbbModule *mod;
	GSList *list;
	gchar *name;

	MBB_XTV_CALL(&name);

	if (! check_mod_name(name, NULL)) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_MODULE);

	mbb_plock_reader_lock();

	on_final { mbb_plock_reader_unlock(); }

	mod = mbb_module_get_by_name(name);
	if (mod == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_MODULE);

	for (list = mod->vars; list != NULL; list = list->next) {
		struct mbb_var *var = list->data;

		if (mbb_var_has_mod(var))
			func(var);
	}

	mbb_plock_reader_unlock();
}

static void module_vars_save(XmlTag *tag, XmlTag **ans)
{
	module_vars_do(tag, ans, mbb_var_cache_save);
}

static void module_vars_load(XmlTag *tag, XmlTag **ans)
{
	module_vars_do(tag, ans, mbb_var_cache_load);
}

MBB_INIT_FUNCTIONS_DO
	MBB_FUNC_STRUCT("mbb-module-list", module_list, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-module-load", module_load, MBB_CAP_ROOT),
	MBB_FUNC_STRUCT("mbb-module-unload", module_unload, MBB_CAP_ROOT),
	MBB_FUNC_STRUCT("mbb-show-modules", show_modules, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-module-info", module_info, MBB_CAP_ADMIN),
	MBB_FUNC_STRUCT("mbb-module-run", module_run, MBB_CAP_WHEEL),
	MBB_FUNC_STRUCT("mbb-module-stop", module_stop, MBB_CAP_WHEEL),

	MBB_FUNC_STRUCT("mbb-module-vars-save", module_vars_save, MBB_CAP_WHEEL),
	MBB_FUNC_STRUCT("mbb-module-vars-load", module_vars_load, MBB_CAP_WHEEL),
MBB_INIT_FUNCTIONS_END

MBB_VAR_DEF(md_def) {
	.op_read = var_str_str,
	.op_write = var_conv_dir,
	.cap_read = MBB_CAP_ALL,
	.cap_write = MBB_CAP_ROOT
};

MBB_SESSION_VAR_DEF(ss_md) {
	.op_new = g_ptr_strdup,
	.op_free = g_ptr_strfree,
	.data = &module_dir__
};

static void init_vars(void)
{
	mbb_base_var_register("module.dir", &md_def, &module_dir__);
	md_var = mbb_session_var_register(SS_("module.dir"), &md_def, &ss_md);
}

MBB_ON_INIT(MBB_INIT_VARS, MBB_INIT_FUNCTIONS)
