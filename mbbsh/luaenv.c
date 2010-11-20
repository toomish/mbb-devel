#include <unistd.h>
#include <string.h>
#include <time.h>

#include "handler.h"
#include "caution.h"
#include "luaenv.h"
#include "header.h"

#include "strconv.h"
#include "xmltag.h"

#define TIME_FMT "%Y-%m-%d %H:%M:%S"

static gint c_dofile(lua_State *ls);
static gint c_cmd_register(lua_State *ls);
static gint c_xml_tag_next(lua_State *ls);
static gint c_xml_tag_get_attr(lua_State *ls);
static gint c_xml_tag_get_body(lua_State *ls);
static gint c_xml_tag_get_child(lua_State *ls);
static gint c_markup_escape(lua_State *ls);
static gint c_server_request(lua_State *ls);
static gint c_time_format(lua_State *ls);
static gint c_caution(lua_State *ls);
static gint c_readpass(lua_State *ls);
static gint c_getpass(lua_State *ls);

GQuark lua_env_error_quark(void)
{
	return g_quark_from_static_string("lua-env-error-quark");
}

lua_env_t lua_env_new(gchar *dir)
{
	lua_State *ls; 
	ls = luaL_newstate();
	if (ls != NULL) {
		luaL_openlibs(ls);

		lua_pushcfunction(ls, c_dofile);
		lua_setglobal(ls, "dofile");

		lua_pushstring(ls, dir);
		lua_setfield(ls, LUA_REGISTRYINDEX, "__lua_init_dir");

		lua_register(ls, "cmd_register", c_cmd_register);
		lua_register(ls, "xml_tag_next", c_xml_tag_next);
		lua_register(ls, "xml_tag_get_attr", c_xml_tag_get_attr);
		lua_register(ls, "xml_tag_get_body", c_xml_tag_get_body);
		lua_register(ls, "xml_tag_get_child", c_xml_tag_get_child);
		lua_register(ls, "markup_escape", c_markup_escape);
		lua_register(ls, "server_request", c_server_request);
		lua_register(ls, "timefmt", c_time_format);
		lua_register(ls, "caution", c_caution);
		lua_register(ls, "readpass", c_readpass);
		lua_register(ls, "getpass", c_getpass);
	}

	return ls;
}

static void lua_env_set_error(lua_State *ls, gint ecode, GError **error)
{
	const gchar *msg;

	switch (ecode) {
	case LUA_ERRSYNTAX:
		msg = "syntax error";
		ecode = LUA_ENV_ERROR_SYNTAX;
		break;
	case LUA_ERRMEM:
		msg = "memory error";
		ecode = LUA_ENV_ERROR_MEM;
		break;
	case LUA_ERRFILE:
		msg = "unable to readfile";
		ecode = LUA_ENV_ERROR_FILE;
		break;
	case LUA_ERRRUN:
		msg = "runtime error";
		ecode = LUA_ENV_ERROR_RUN;
		break;
	default:
		msg = "unknown error";
		ecode = LUA_ENV_ERROR_UNKNOWN;
	}

	if (ecode != LUA_ENV_ERROR_MEM && lua_isstring(ls, -1))
		msg = lua_tostring(ls, -1);

	g_set_error(error, LUA_ENV_ERROR, ecode, msg);
}

gboolean lua_env_init(lua_State *ls, GError **error)
{
	gint ecode;

	lua_getglobal(ls, "dofile");
	lua_pushstring(ls, "init");

	ecode = lua_pcall(ls, 1, LUA_MULTRET, 0);
	if (ecode) {
		lua_env_set_error(ls, ecode, error);
		return FALSE;
	}

	return TRUE;
}

gboolean lua_env_dofile(lua_State *ls, const gchar *filename, GError **error)
{
	gint ecode;

	ecode = luaL_dofile(ls, filename);
	if (ecode) {
		lua_env_set_error(ls, ecode, error);
		return FALSE;
	}

	return TRUE;
}

static void lua_env_get_handler(lua_State *ls, gchar *handler)
{
	if (handler != NULL)
		lua_getglobal(ls, handler);
	else {
		lua_getglobal(ls, "mbb");
		lua_getfield(ls, -1, "request");
		lua_remove(ls, -2);
	}
}

gboolean lua_env_call(lua_State *ls, struct cmd_params *p, GError **error)
{
	struct lua_cmd_entry *entry;
	gint with_method;
	gint ecode;
	gint n;

	entry = p->entry;

	lua_env_get_handler(ls, entry->handler);

	with_method = 0;
	if (entry->method != NULL) {
		lua_getglobal(ls, "mbb");
		lua_getfield(ls, -1, "tag");
		lua_remove(ls, -2);
		lua_pushstring(ls, entry->method);

		ecode = lua_pcall(ls, 1, 1, 0);

		if (ecode) {
			lua_env_set_error(ls, ecode, error);
			return FALSE;
		}

		with_method = 1;
	}

	for (n = 0; n < p->argc; n++)
		lua_pushstring(ls, p->argv[n]);

	ecode = lua_pcall(ls, n + with_method, 0, 0);
	talk_half_say(NULL);

	if (ecode) {
		lua_env_set_error(ls, ecode, error);
		return FALSE;
	}

	return TRUE;
}

gboolean lua_env_info(lua_State *ls, gchar *func, struct lua_func_info *fi,
		      GError **error)
{
	lua_Debug ar;

	lua_env_get_handler(ls, func);
	if (lua_getinfo(ls, ">S", &ar) == 0) {
		lua_env_set_error(ls, 0, error);
		return FALSE;
	}

	if (strcmp(ar.what, "Lua")) {
		g_set_error(error, LUA_ENV_ERROR, LUA_ENV_ERROR_RUN,
			"not Lua source: %s", ar.what);
		return FALSE;
	}

	fi->source = ar.source + 1;
	fi->line = ar.linedefined;
	fi->lastline = ar.lastlinedefined;

	return TRUE;
}

void lua_env_close(lua_State *ls)
{
	lua_close(ls);
}

static void lua_push_xml_tag(lua_State *ls, XmlTag *tag)
{
	lua_pushlightuserdata(ls, tag);
	lua_getglobal(ls, LUA_XML_TAG_META);
	lua_setmetatable(ls, -2);
}

static void lua_push_variant(lua_State *ls, Variant *var)
{
	if (var == NULL)
		lua_pushnil(ls);
	else {
		gchar *str = variant_get_string(var);

		if (str == NULL)
			lua_pushnil(ls);
		else
			lua_pushstring(ls, str);
	}
}

static inline void arg_check(lua_State *ls, gint min, gint max)
{
	gint nargs;

	nargs = lua_gettop(ls);

	if (nargs < min)
		luaL_error(ls, "not enough args");

	if (max != 0 && nargs > max)
		luaL_error(ls, "too many args");
}

static gint c_dofile(lua_State *ls)
{
	const gchar *file;
	const gchar *dir;
	gchar *path;
	gint ecode;

	arg_check(ls, 1, 1);

	file = luaL_checkstring(ls, 1);
	if (strchr(file, '/') != NULL)
		luaL_error(ls, "invalid arg");

	lua_getfield(ls, LUA_REGISTRYINDEX, "__lua_init_dir");
	dir = luaL_checkstring(ls, -1);
	lua_pop(ls, 1);

	path = g_strdup_printf("%s/%s.lua", dir, file);
	ecode = luaL_dofile(ls, path);
	g_free(path);

	if (ecode != 0)
		lua_error(ls);

	return 0;
}

static gint c_cmd_register(lua_State *ls)
{
	struct lua_cmd_entry entry;
	const gchar *handler;
	const gchar *method;
	const gchar *str;
	gint min, max;
	gint nargs;
	
	nargs = lua_gettop(ls);
	arg_check(ls, 2, 5);

	min = max = 0;

	if (nargs >= 4) {
		if (lua_isnil(ls, 4))
			min = -1;
		else {
			min = luaL_checkinteger(ls, 4);
			luaL_argcheck(ls, min >= 0, 4, "negative value");
		}

		max = min;
		if (nargs > 4) {
			if (min < 0)
				luaL_error(ls, "argument #5 is unnecessary if #4 = nil");

			if (lua_isnil(ls, 5))
				max = 0;
			else {
				gint n;

				n = luaL_checkinteger(ls, 5);
				luaL_argcheck(ls, n >= 0, 5, "negative value");

				max += n;
			}
		}
	}

	str = luaL_checkstring(ls, 1);

	if (lua_isnil(ls, 2))
		method = NULL;
	else
		method = luaL_checkstring(ls, 2);

	if (lua_isnoneornil(ls, 3))
		handler = NULL;
	else {
		handler = luaL_checkstring(ls, 3);
		lua_getglobal(ls, handler);
		if (lua_isfunction(ls, -1) == 0) {
			lua_pop(ls, 1);
			luaL_error(ls, "argument #3 is not a lua function");
		}
	}

	entry.cmdv = g_strsplit(str, " ", 0);
	entry.method = g_strdup(method);
	entry.handler = g_strdup(handler);
	entry.arg_min = min;
	entry.arg_max = max;

	lua_cmd_push(g_memdup(&entry, sizeof(entry)));

	return 0;
}

static XmlTag *c_get_xml_tag(lua_State *ls)
{
	XmlTag *tag;

	luaL_argcheck(ls, lua_islightuserdata(ls, 1), 1, "not userdate");

	tag = lua_touserdata(ls, 1);
	if (tag == NULL)
		luaL_error(ls, "why XmlTag is null ?");

	return tag;
}

static gint c_xml_tag_next(lua_State *ls)
{
	XmlTag *tag;

	arg_check(ls, 1, 1);

	tag = c_get_xml_tag(ls);

	if (tag->next == NULL)
		lua_pushnil(ls);
	else
		lua_push_xml_tag(ls, tag->next);

	return 1;
}

static gint c_xml_tag_get_attr(lua_State *ls)
{
	XmlTag *tag;
	gchar *name;

	arg_check(ls, 2, 2);

	tag = c_get_xml_tag(ls);
	name = (gchar *) luaL_checkstring(ls, 2);
	lua_push_variant(ls, xml_tag_get_attr(tag, name));

	return 1;
}

static gint c_xml_tag_get_body(lua_State *ls)
{
	XmlTag *tag;

	arg_check(ls, 1, 1);

	tag = c_get_xml_tag(ls);
	lua_push_variant(ls, xml_tag_get_body(tag));

	return 1;
}

static gint c_xml_tag_get_child(lua_State *ls)
{
	XmlTag *tag;
	gchar *name;

	arg_check(ls, 2, 2);

	tag = c_get_xml_tag(ls);
	name = (gchar *) luaL_checkstring(ls, 2);
	tag = xml_tag_get_child(tag, name);

	if (tag == NULL)
		lua_pushnil(ls);
	else
		lua_push_xml_tag(ls, tag);

	return 1;
}

static gint c_markup_escape(lua_State *ls)
{
	const gchar *str;
	gchar *text;

	arg_check(ls, 1, 1);

	str = luaL_checkstring(ls, 1);
	text = g_markup_escape_text(str, -1);
	lua_pushstring(ls, text);
	g_free(text);

	return 1;
}

static gint c_server_request(lua_State *ls)
{
	XmlTag *tag;
	const gchar *str;
	gint ecode;

	arg_check(ls, 1, 1);

	lua_getglobal(ls, "tostring");
	lua_pushvalue(ls, 1);
	ecode = lua_pcall(ls, 1, 1, 0);

	if (ecode != 0)
		lua_error(ls);

	str = luaL_checkstring(ls, -1);
	if (str == NULL)
		luaL_argerror(ls, 1, "nil value");

	tag = talk_half_say((gchar *) str);
	lua_pop(ls, 1);
	lua_push_xml_tag(ls, tag);

	return 1;
}

static gint c_time_format(lua_State *ls)
{
	static gchar buf[64];
	time_t t;

	if (lua_isnumber(ls, 1)) {
		t = luaL_checkint(ls, 1);
		luaL_argcheck(ls, t >= 0, 1, "negative value");
	} else {
		GError *error = NULL;
		const gchar *str;

		str = luaL_checkstring(ls, 1);
		t = str_conv_to_uint64(str, &error);
		if (error != NULL) {
			g_error_free(error);
			luaL_argerror(ls, 1, "invalid number");
		}
	}

	if (t == 0)
		strcpy(buf, "inf");
	else
		strftime(buf, sizeof(buf), TIME_FMT, localtime(&t));

	lua_pushstring(ls, buf);
	return 1;
}

static gint c_caution(lua_State *ls)
{
	lua_pushboolean(ls, caution());
	return 1;
}

static gint c_readpass(lua_State *ls)
{
	gchar *pass;
	gchar *str;

	for (;;) {
		pass = getpass("new password: ");
		if (*pass == '\0') {
			str = NULL;
			break;
		}

		str = g_strdup(pass);
		pass = getpass("new password again: ");
		if (!strcmp(str, pass))
			break;

		g_free(str);
	}

	if (str == NULL)
		lua_pushnil(ls);
	else {
		lua_pushstring(ls, str);
		g_free(str);
	}

	return 1;
}

static gint c_getpass(lua_State *ls)
{
	const gchar *str;

	str = luaL_checkstring(ls, 1);
	lua_pushstring(ls, getpass(str));

	return 1;
}

