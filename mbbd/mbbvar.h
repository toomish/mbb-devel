#ifndef MBB_VAR_H
#define MBB_VAR_H

#include <glib.h>

#include "mbbmodule.h"
#include "mbbcap.h"

#include "varconv.h"

#define SS_(v) "@." v

#define MBB_VAR_ERROR (mbb_var_error_quark())

#define MBB_VAR_DEF(name) static struct mbb_var_def name =
#define MBB_SESSION_VAR_DEF(name) static struct mbb_session_var name =

#define mbb_base_var_register(...) \
	mbb_var_register(mbb_base_var_new(__VA_ARGS__))
#define mbb_session_var_register(...) \
	mbb_var_register(mbb_session_var_new(__VA_ARGS__))

typedef enum {
	MBB_VAR_ERROR_UNKNOWN = 1,
	MBB_VAR_ERROR_READONLY,
	MBB_VAR_ERROR_INVALID,
	MBB_VAR_ERROR_SESSION
} MbbVarError;

GQuark mbb_var_error_quark(void);

struct mbb_var;

struct mbb_var_def {
	gchar *(*op_read)(gpointer p);
	gboolean (*op_write)(gchar *arg, gpointer p);

	mbb_cap_t cap_read;
	mbb_cap_t cap_write;
};

struct mbb_session_var {
	gpointer data;
	gpointer (*op_new)(gpointer);
	void (*op_free)(gpointer);
};

gpointer g_ptr_strdup(gpointer ptr);
gpointer g_ptr_vardup(gpointer ptr);
void g_ptr_strfree(gpointer ptr);
gpointer g_ptr_booldup(gpointer ptr);

static inline gboolean g_ptr_str_is_null(gchar **ptr)
{
	return ptr == NULL || *ptr == NULL;
}

gchar *mbb_var_get_name(struct mbb_var *var);
gpointer mbb_var_get_priv(struct mbb_var *var);
gboolean mbb_var_has_mod(struct mbb_var *var);

mbb_cap_t mbb_var_get_cap_read(struct mbb_var *var);
mbb_cap_t mbb_var_get_cap_write(struct mbb_var *var);
void mbb_var_free(struct mbb_var *var);

struct mbb_var *mbb_base_var_new(gchar *name, struct mbb_var_def *def,
				 gpointer ptr);
struct mbb_var *mbb_session_var_new(gchar *name, struct mbb_var_def *def,
				    struct mbb_session_var *priv);

void mbb_base_var_lock(struct mbb_var *var);
void mbb_base_var_unlock(struct mbb_var *var);

gpointer mbb_session_var_get_data(struct mbb_var *var);

struct mbb_var *mbb_var_register(struct mbb_var *var);
gboolean mbb_var_mregister(struct mbb_var *var, MbbModule *mod);
void mbb_var_unregister(struct mbb_var *var);

gboolean mbb_var_assign(gchar *varname, gchar *value, GError **error);

void mbb_var_cache_add(gchar *name, gchar *value);
gchar *mbb_var_cache_get(gchar *name);

void mbb_var_cache_save(struct mbb_var *var);
void mbb_var_cache_load(struct mbb_var *var);

#endif
