/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbsettings.h"
#include "mbbvar.h"
#include "trash.h"

#include "debug.h"

#include <string.h>

#include <glib.h>

#define SVAR(name) &settings.name

struct mbb_settings settings = {
	.host = "0.0.0.0",
	.port = "3000"
};

struct mbb_settings_entry {
	gchar *group;
	gchar *key;

	gchar **ph;
};

static struct mbb_settings_entry settings_entries[] = {
	{ "general", "host", SVAR(host) },
	{ "general", "port", SVAR(port) },
	{ "general", "http-port", SVAR(http_port) },
	{ "general", "dbtype", SVAR(dbtype) },

	{ "db", "login", SVAR(db_user) },
	{ "db", "secret", SVAR(db_pass) },
	{ "db", "host", SVAR(db_host) },
	{ "db", "database", SVAR(db_name) },

	{ NULL, NULL, NULL }
};

static void load_vars(GKeyFile *key_file)
{
	gchar **key, **pp;
	gchar *value;
	GError *error = NULL;

	pp = g_key_file_get_keys(key_file, "var", NULL, NULL);
	if (pp == NULL)
		return;

	for (key = pp; *key != NULL; key++) {
		value = g_key_file_get_value(key_file, "var", *key, NULL);
		if (value == NULL)
			continue;

		if (! mbb_var_assign(*key, value, &error)) {
			msg_warn("var %s: %s", *key, error->message);
			g_error_free(error);
			error = NULL;
		}

		g_free(value);
	}

	g_strfreev(pp);
}

static void load_cache(GKeyFile *key_file)
{
	gchar **key, **pp;
	gchar *value;

	pp = g_key_file_get_keys(key_file, "cache", NULL, NULL);
	if (pp == NULL)
		return;

	for (key = pp; *key != NULL; key++) {
		value = g_key_file_get_value(key_file, "cache", *key, NULL);
		if (value == NULL)
			continue;

		mbb_var_cache_add(g_strdup(*key), value);
	}

	g_strfreev(pp);
}

static void get_modules(GKeyFile *key_file)
{
	gchar **list, **pp;
	gchar *name;

	list = g_key_file_get_string_list(key_file, "general", "modules", NULL, NULL);
	if (list == NULL)
		return;

	for (pp = list; *pp != NULL; pp++) {
		name = g_strdup(g_strstrip(*pp));
		settings.modules = g_slist_append(settings.modules, name);
	}

	g_strfreev(list);
}

gboolean readselfconfig(gchar *path, GError **error)
{
	struct mbb_settings_entry *entry;
	GKeyFile *key_file;

	key_file = g_key_file_new();
	g_key_file_set_list_separator(key_file, ',');

	if (! g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, error)) {
		g_key_file_free(key_file);
		return FALSE;
	}

	load_vars(key_file);
	load_cache(key_file);
	get_modules(key_file);

	for (entry = settings_entries; entry->group != NULL; entry++) {
		gchar *value;

		value = g_key_file_get_value(
			key_file, entry->group,
			entry->key, NULL
		);

		if (value != NULL)
			*entry->ph = g_strdup(value);
	}

	g_key_file_free(key_file);
	g_free(path);

	return TRUE;
}

