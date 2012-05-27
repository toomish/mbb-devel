/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <signal.h>
#include <string.h>
#include <err.h>

#include <glib.h>

#include "mbbsettings.h"
#include "mbbserver.h"
#include "mbbmodule.h"
#include "mbbdbload.h"
#include "mbbinit.h"
#include "mbbdb.h"

#include "debug.h"
#include "query.h"

#define CONFIG_NAME "mbbrc"

#define DB_LOAD(name) \
	if (mbb_db_load_##name(&error) == FALSE) \
		err_quit("mbb_db_load_" #name ": %s", error->message)

static gchar *opt_config_file = NULL;

static GOptionEntry mbb_options[] = {
	{ "config", 'c', 0, G_OPTION_ARG_STRING, &opt_config_file, "filename of the config-file", NULL },
	{ NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};


static gboolean mbb_db_init(GError **error)
{
	static struct mbb_db_auth auth;

	auth.login = settings.db_user;
	auth.secret = settings.db_pass;
	auth.database = settings.db_name;
	auth.hostaddr = settings.db_host;

	return mbb_db_open(&auth, error);
}

static void sighandler(int signo G_GNUC_UNUSED)
{
	return;
}

static void init_signals(void)
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &act, NULL) < 0)
		err_sys("sigaction");

	act.sa_handler = sighandler;
	if (sigaction(SIGUSR1, &act, NULL) < 0)
		err_sys("sigaction");
}

static void load_modules(GSList *list)
{
	gchar *name;

	for (; list != NULL; list = list->next) {
		GError *error = NULL;

		name = list->data;

		if (mbb_module_load(name, &error))
			msg_warn("load %s: ok", name);
		else {
			msg_warn("load %s failed: %s", name, error->message);
			g_error_free(error);
		}
	}
}

int main(int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;

	g_thread_init(NULL);

	opt_config_file = g_build_filename(
		DEFAULT_CONF_DIR, CONFIG_NAME, NULL
	);
	mbb_options[0].arg_description = opt_config_file;

	context = g_option_context_new("- mbb server");
	g_option_context_add_main_entries(context, mbb_options, NULL);

	if (! g_option_context_parse(context, &argc, &argv, &error))
		errx(1, "%s", error->message);
	g_option_context_free(context);

	init_signals();

	mbb_init();

	if (readselfconfig(opt_config_file, &error) == FALSE) {
		msg_warn("readselfconfig %s: %s",
			opt_config_file, error->message
		);
		g_error_free(error);
		error = NULL;
	}

	if (settings.dbtype == NULL)
		err_quit("unknown database type");

	if (mbb_db_choose(settings.dbtype) == FALSE)
		err_quit("no such db %s", settings.dbtype);

	if (mbb_db_init(&error) == FALSE)
		err_quit("mbb_db_init: %s", error->message);

	query_set_escape(mbb_db_escape);

	DB_LOAD(users);
	DB_LOAD(groups);
	DB_LOAD(groups_pool);
	DB_LOAD(consumers);
	DB_LOAD(units);
	DB_LOAD(unit_inet_pool);
	DB_LOAD(gateways);
	DB_LOAD(operators);
	DB_LOAD(gwlinks);

	load_modules(settings.modules);

	mbb_server(settings.host, settings.port, settings.http_port);

	return 0;
}

