#ifndef LUA_ENV_H
#define LUA_ENV_H

#include <lauxlib.h>
#include <lualib.h>
#include <lua.h>

#include <glib.h>

#include "header.h"

#define LUA_XML_TAG_META "xml_tag_meta"
#define LUA_XML_META "xml_meta"

struct lua_func_info {
	const gchar *source;
	gint line;
	gint lastline;
};

typedef enum {
	LUA_ENV_ERROR_NONE,
	LUA_ENV_ERROR_SYNTAX,
	LUA_ENV_ERROR_MEM,
	LUA_ENV_ERROR_FILE,
	LUA_ENV_ERROR_RUN,
	LUA_ENV_ERROR_UNKNOWN
} LuaEnvError;

#define LUA_ENV_ERROR (lua_env_error_quark())

GQuark lua_env_error_quark(void);

typedef lua_State *lua_env_t;

lua_env_t lua_env_new(gchar *dir);
gboolean lua_env_init(lua_env_t lua_env, GError **error);
gboolean lua_env_dofile(lua_env_t lua_env, const gchar *filename, GError **error);
gboolean lua_env_call(lua_env_t lua_env, struct cmd_params *p, GError **error);
gboolean lua_env_info(lua_env_t lua_env, gchar *func, struct lua_func_info *fi,
		      GError **error);
void lua_env_close(lua_env_t lua_env);

#endif
