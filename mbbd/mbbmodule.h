/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_MODULE_H
#define MBB_MODULE_H

#include <glib.h>

struct mbb_func_struct;
struct mbb_var_struct;

typedef struct mbb_module MbbModule;
typedef struct mbb_module_info MbbModuleInfo;

typedef enum {
	MBB_MODULE_ERROR_UNKNOWN,
	MBB_MODULE_ERROR_EXISTS,
	MBB_MODULE_ERROR_LOADED,
	MBB_MODULE_ERROR_OPEN,
	MBB_MODULE_ERROR_FOREIGN,
	MBB_MODULE_ERROR_INVALID,
	MBB_MODULE_ERROR_EMPTY,
	MBB_MODULE_ERROR_USED
} MbbModuleError;

#define MBB_MODULE_ERROR (mbb_module_error_quark())

GQuark mbb_module_error_quark(void);

struct mbb_module_info {
	struct mbb_module *module;
	gchar *desc;

	void (*load)(void);
	void (*unload)(void);
};

gchar *mbb_module_get_name(MbbModule *mod);

MbbModule *mbb_module_current(void);
void mbb_module_push_info(MbbModuleInfo *mod_info);

#define mbb_module_add_base_var(...) \
	mbb_module_add_var(mbb_base_var_new(__VA_ARGS__))
#define mbb_module_add_session_var(...) \
	mbb_module_add_var(mbb_session_var_new(__VA_ARGS__))

gboolean mbb_module_load(gchar *name, GError **error);
gboolean mbb_module_unload(gchar *name, GError **error);

MbbModule *mbb_module_last_used(void);
gboolean mbb_module_use(MbbModule *mod);
void mbb_module_unuse(MbbModule *mod);

void mbb_module_add_func(struct mbb_func_struct *fs);
void mbb_module_add_funcv(struct mbb_func_struct *fs, gsize count);
struct mbb_var *mbb_module_add_var(struct mbb_var *var);

void mbb_module_export(gpointer data);
gpointer mbb_module_import(gchar *name);

gboolean mbb_module_isrun(MbbModule *mod);

#define MBB_DEFINE_MODULE(description) \
static struct mbb_module_info module_info__ = { \
	.desc = description, \
	.load = load_module, \
	.unload = unload_module \
}; \
\
static void __attribute__ ((constructor)) __reg_module(void) \
{ \
	mbb_module_push_info(&module_info__); \
}

#endif
