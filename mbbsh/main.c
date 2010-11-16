#define _GNU_SOURCE
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include <setjmp.h>

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <glib.h>
#include <glib/gprintf.h>

#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#include "macros.h"
#include "debug.h"

#include "nettalker.h"
#include "handler.h"
#include "methods.h"
#include "luaenv.h"
#include "auth.h"

typedef void (*func_entry_t)(gint argc, gchar **argv);

static lua_env_t lua_env = NULL;

static sigjmp_buf jmp_env;

static const gchar *opt_host = "127.0.0.1";
static const gchar *opt_serv = NULL;
static const gchar *opt_user;
static const gchar *opt_pass = NULL;
static const gchar *opt_key = NULL;
static const gchar *opt_dir = NULL;
static const gchar *opt_file = NULL;
static gboolean opt_nosh = FALSE;
static gboolean opt_follow = FALSE;
static gboolean caught_sigint = FALSE;
static gboolean info_mode = FALSE;

GQueue lua_cmd_queue = G_QUEUE_INIT;

static GNode *main_tree = NULL;
static GNode *head_node;

static void internal_cd(gint argc, gchar **argv);
static void internal_clear(gint argc, gchar **argv);
static void internal_ls(gint argc, gchar **argv);
static void internal_su(gint argc, gchar **argv);
static void internal_help(gint argc, gchar **argv);
static void internal_exec(gint argc, gchar **argv);
static void internal_echo(gint argc, gchar **argv);
static void internal_reload(gint argc, gchar **argv);
static void internal_info(gint argc, gchar **argv);

static struct {
	gchar *name;
	func_entry_t func;
	gchar *info;
} local_cmd_table[] = {
	{ "cd", internal_cd, "change command level" },
	{ "ls", internal_ls, "list commands on current level" },
	{ "clear", internal_clear, "execute clear command" },
	{ "echo", internal_echo, "display a line of text" },
	{ "reload", internal_reload, "reload self methods" },
	{ "su", internal_su, "substitute user" },
	{ "exec", internal_exec, "execute command" },
	{ "info", internal_info, "set info mode on/off" },
	{ "help", internal_help, "show internal commands" }
};

enum opt_code { OPT_DEF, OPT_SOURCE, OPT_INFO };

static void build_func_tree(GHashTable *methods)
{
	struct lua_cmd_entry *entry;
	GNode *parent;
	GNode *node;
	gchar **pp;
	GList *list;

	if (lua_cmd_queue.length == 0)
		return;

	main_tree = g_node_new(NULL);
	for (list = lua_cmd_queue.head; list != NULL; list = list->next) {
		entry = (struct lua_cmd_entry *) list->data;
		parent = main_tree;
		pp = entry->cmdv;

		if (entry->method != NULL) {
			if (g_hash_table_lookup(methods, entry->method) == NULL)
				continue;
		}

		while (*pp != NULL) {
			node = g_node_first_child(parent);
			if (node == NULL || strcmp(*pp, node->data))
				node = g_node_prepend(parent, g_node_new(*pp));
			parent = node;

			pp++;
		}

		g_node_prepend(node, g_node_new(entry));
	}
}

static int lua_cmd_entry_cmp(const void *a, const void *b)
{
	gchar **pa;
	gchar **pb;
	gint n;

	pa = ((struct lua_cmd_entry *) a)->cmdv;
	pb = ((struct lua_cmd_entry *) b)->cmdv;

	for (;;) {
		if (*pa == NULL && *pb == NULL)
			return 0;
		if (*pa == NULL)
			return 1;
		if (*pb == NULL)
			return -1;

		n = strcmp(*pa, *pb);
		if (n != 0)
			break;

		pa++;
		pb++;
	}

	return -n;
}

static char *generator(const char *text, int state)
{
	static GNode *node;
	static int len;
	char *data;

	if (!state) {
		node = g_node_first_child(head_node);
		len = strlen(text);
	}

	if (node == NULL || G_NODE_IS_LEAF(node))
		return NULL;

	while (node != NULL) {
		data = (char *) node->data;
		node = g_node_next_sibling(node);

		if (!strncmp(text, data, len))
			return g_strdup(data);
	}

	return NULL;
}

static char **completion(const char *text, int start, int end G_GNUC_UNUSED)
{
	char **matches;
	GNode *node;

	matches = (char **) NULL;

	if (start == 0)
		node = main_tree;
	else {
		gchar *line;
		gchar **strv;
		gchar **pp;
		gint cnt;

		line = g_new(gchar, start + 1);
		strncpy(line, rl_line_buffer, start);
		line[start] = '\0';

		strv = g_strsplit(line, " ", 0);
		g_free(line);

		cnt = 0;
		node = main_tree;
		for (pp = strv; *pp != NULL; pp++) {
			if (**pp == '\0')
				continue;

			cnt++;
			node = g_node_first_child(node);
			if (G_NODE_IS_LEAF(node)) {
				node = NULL;
				break;
			}

			while (node != NULL) {
				if (!strcmp(*pp, node->data))
					break;
				node = g_node_next_sibling(node);
			}

			if (node == NULL)
				break;
		}

		g_strfreev(strv);
		if (cnt == 0)
			node = main_tree;
	}

	if (node != NULL) {
		head_node = node;
		matches = rl_completion_matches(text, generator);
	}

	rl_attempted_completion_over = 1;
	return matches;
}

static void initialize_readline(void)
{
	static int initialized = 0;

	if (! initialized) {
		rl_readline_name = g_get_prgname();
		rl_attempted_completion_function = completion;
		initialized++;
	}
}

static struct lua_cmd_entry *lua_cmd_find(gchar **argv, gint *offp)
{
	gchar **argp;
	GNode *node;

	if (main_tree == NULL)
		return NULL;

	node = main_tree;
	argp = argv;
	for (;;) {
		node = g_node_first_child(node);
		if (G_NODE_IS_LEAF(node)) {
			struct lua_cmd_entry *entry;

			entry = (struct lua_cmd_entry *) node->data;
			*offp = argp - argv - 1;
			return entry;
		}

		if (*argp == NULL)
			return NULL;

		for (; node != NULL; node = g_node_next_sibling(node))
			if (! strcmp(*argp, node->data))
				break;

		if (node == NULL)
			return NULL;
		argp++;
	}
}

static void ls_print_tree(GNode *node, gboolean deep)
{
	__extension__
	void node_walk(GNode *node, guint off, gboolean deep)
	{
		node = g_node_first_child(node);
		for (; node != NULL; node = g_node_next_sibling(node)) {
			if (G_NODE_IS_LEAF(node))
				continue;

			g_print("%*s%s\n", off, "", (gchar *) node->data);

			if (deep) node_walk(node, off + 2, deep);
		}
	}

	node_walk(node, 0, deep);
}

static void internal_ls(gint argc, gchar **argv)
{
	gboolean rec = FALSE;

	if (argc > 1) {
		if (argc > 2) {
			g_print("ERR: too many args\n");
			return;
		}

		if (! strcmp(argv[1], "-R"))
			rec = TRUE;
		else {
			g_print("ERR: invalid arg '%s'\n", argv[1]);
			return;
		}
	}

	ls_print_tree(main_tree, rec);
}

static GNode *change_node(GNode *node, gchar *dir)
{
	if (! strcmp(dir, "/"))
		node = g_node_get_root(node);
	else if (! strcmp(dir, "..")) {
		if (node->parent != NULL)
			node = node->parent;
	} else {
		node = g_node_first_child(node);
		for (; node != NULL; node = g_node_next_sibling(node)) {
			if (G_NODE_IS_LEAF(node))
				continue;

			if (! g_strcmp0(dir, node->data))
				break;
		}

		if (node != NULL && G_NODE_IS_LEAF(g_node_first_child(node)))
			node = NULL;
	}

	return node;
}

static GNode *node_path(GNode *node, gchar *path)
{
	if (*path == '/') {
		node = change_node(node, "/");
		while (*++path == '/');
	}

	if (*path != '\0') {
		gchar **dirs;
		gchar **pp;

		dirs = g_strsplit(path, "/", 0);
		for (pp = dirs; *pp != NULL && node != NULL; pp++)
			if (**pp != '\0')
				node = change_node(node, *pp);

		g_strfreev(dirs);
	}

	return node;
}

static void internal_exec(gint argc, gchar **argv)
{
	pid_t pid;

	if (argc < 1)
		return;

	pid = fork();
	if (pid < 0)
		msg_err("fork");
	else if (pid == 0) {
		argv++;
		execvp(argv[0], argv);
		err_sys("execvp %s", argv[0]);
	}

	if (wait(NULL) < 0)
		msg_err("wait");
}

static void internal_clear(gint argc G_GNUC_UNUSED, gchar **argv G_GNUC_UNUSED)
{
	pid_t pid;

	pid = fork();
	if (pid < 0)
		msg_err("fork");
	else if (pid == 0) {
		execlp("clear", "clear", NULL);
		err_sys("execlp");
	}

	if (wait(NULL) < 0)
		msg_err("wait");
}

static void internal_cd(gint argc, gchar **argv)
{
	GNode *node;

	if (argc == 1) {
		main_tree = change_node(main_tree, "/");
		return;
	}

	node = node_path(main_tree, argv[1]);

	if (node == NULL)
		g_print("ERR: cannot cd to %s\n", argv[1]);
	else
		main_tree = node;
}

static void internal_echo(gint argc, gchar **argv)
{
	gint n;

	if (argc == 1)
		return;

	g_print("%s", argv[1]);
	for (n = 2; n < argc; n++)
		g_print(" %s", argv[n]);

	g_print("\n");
}

static gboolean strtobool(gchar *s, gboolean *ptr)
{
	if (! strcmp(s, "on") || ! strcmp(s, "true"))
		*ptr = TRUE;
	else if (! strcmp(s, "off") || ! strcmp(s, "false"))
		*ptr = FALSE;
	else
		return FALSE;

	return TRUE;
}

static inline gchar *booltostr(gboolean val)
{
	return val ? "on" : "off";
}

static void internal_info(gint argc, gchar **argv)
{
	if (argc == 1) {
		g_print("%s\n", booltostr(info_mode));
		return;
	}

	if (strtobool(argv[1], &info_mode) == FALSE)
		g_print("ERR: invalid argument\n");
}

static gchar *node_getcwd(void)
{
	static gchar buf[512];
	GSList *node_list;
	GSList *list;
	GNode *node;
	gchar *p;

	node_list = NULL;
	node = main_tree;
	if (node == NULL || node->parent == NULL)
		return NULL;

	for (node = main_tree; node->parent != NULL; node = node->parent)
		node_list = g_slist_prepend(node_list, node->data);

	p = buf;
	for (list = node_list; list != NULL; list = list->next)
		p += g_sprintf(p, "/%s", (gchar *) list->data);
	g_slist_free(node_list);

	return buf;
}

static void reload_func_tree(void)
{
	GHashTable *methods;
	gchar *cwd;

	methods = get_methods();
	if (methods == NULL)
		err_quit("get_methods failed");

	if (main_tree == NULL)
		cwd = NULL;
	else {
		cwd = node_getcwd();
		g_node_destroy(g_node_get_root(main_tree));
	}

	build_func_tree(methods);
	g_hash_table_destroy(methods);

	if (cwd != NULL) {
		GNode *node;

		node = node_path(main_tree, cwd);
		if (node != NULL)
			main_tree = node;
	}
}

static void internal_reload(gint argc, gchar **argv G_GNUC_UNUSED)
{
	if (argc > 1) {
		g_print("What do you want to reload ?\n");
		return;
	}

	reload_func_tree();
}

static void internal_su(gint argc, gchar **argv)
{
	gchar *user = "root";
	gchar *pass;

	if (argc > 2) {
		g_print("ERR: too many args\n");
		return;
	}

	if (argc > 1)
		user = argv[1];

	pass = getpass("password: ");
	if (pass == NULL)
		msg_err("getpass");
	else if (auth_plain(user, pass) == FALSE)
		g_print("ERR: auth failed\n");
}

static void internal_help(gint argc G_GNUC_UNUSED, gchar **argv G_GNUC_UNUSED)
{
	guint n;

	if (argc > 1)
		return;

	for (n = 0; n < NELEM(local_cmd_table); n++)
		g_print("%s - %s\n", local_cmd_table[n].name, local_cmd_table[n].info);
}

static func_entry_t local_cmd_find(gchar *cmd)
{
	guint n;

	for (n = 0; n < NELEM(local_cmd_table); n++)
		if (! strcmp(cmd, local_cmd_table[n].name))
			return local_cmd_table[n].func;

	return NULL;
}

static void print_lines(const gchar *file, gint line, gint last)
{
	gchar *buf = NULL;
	gsize len = 0;
	FILE *fp;
	gint n;

	fp = fopen(file, "r");
	if (fp == NULL) {
		g_print("ERR: fopen %s: %s\n", file, g_strerror(errno));
		return;
	}

	for (n = 1; getline(&buf, &len, fp) >= 0; n++) {
		if (n >= line)
			g_print("%s", buf);
		if (n == last)
			break;
	}

	fclose(fp);
	free(buf);
}

static void print_info(gchar *handler, gchar *method, struct lua_func_info *fi)
{
	if (handler == NULL)
		handler = "(default)";

	if (method == NULL)
		method = "(none)";

	printf("lua handler: %s\n", handler);
	printf("mbb method: %s\n", method);
	printf("source file: %s\n", fi->source);
	printf("line defined: %d\n", fi->line);
}

static gboolean maybe_info_wanted(gchar *opt, struct lua_cmd_entry *entry)
{
	struct lua_func_info fi;
	GError *error = NULL;
	enum opt_code ocode;

	if (! strcmp(opt, "--def"))
		ocode = OPT_DEF;
	else if (! strcmp(opt, "--source"))
		ocode = OPT_SOURCE;
	else if (! strcmp(opt, "--info"))
		ocode = OPT_INFO;
	else
		return FALSE;

	if (lua_env_info(lua_env, entry->handler, &fi, &error) == FALSE) {
		g_print("ERR: %s\n", error->message);
		g_error_free(error);

		return TRUE;
	}

	if (ocode == OPT_INFO)
		print_info(entry->handler, entry->method, &fi);
	else {
		if (ocode == OPT_DEF)
			fi.lastline = fi.line;

		print_lines(fi.source, fi.line, fi.lastline);
	}

	return TRUE;
}

static void exec_lua_cmd(gint argc, gchar **argv, struct lua_cmd_entry *entry)
{
	struct cmd_params par;
	GError *error = NULL;

	if (info_mode && argc && maybe_info_wanted(argv[0], entry))
		return;

	par.argc = argc;
	par.argv = argv;
	par.entry = entry;

	if (entry->arg_min >= 0) do {
		if (argc < entry->arg_min)
			g_print("ERR: not enough args\n");
		else if (entry->arg_max == 0 && entry->arg_min == 0 && argc)
			g_print("ERR: this function not accept args\n");
		else if (entry->arg_max != 0 && argc > entry->arg_max)
			g_print("ERR: too many args\n");
		else
			break;
		return;
	} while (0);

	if (lua_env_call(lua_env, &par, &error) == FALSE) {
		g_print("ERR: %s\n", error->message);
		g_error_free(error);
	}
}

static gboolean parse_line(gchar *line)
{
	gint off;
	gint argc;
	gchar **argv;
	GError *error;
	gboolean ret;

	ret = TRUE;
	error = NULL;
	if (g_shell_parse_argv(line, &argc, &argv, &error) == FALSE) {
		if (g_error_matches(error, G_SHELL_ERROR, G_SHELL_ERROR_EMPTY_STRING))
			ret = FALSE;
		else
			g_print("ERR: %s\n", error->message);

		g_error_free(error);
	} else {
		func_entry_t func;

		off = 0;

		func = local_cmd_find(*argv);
		if (func != NULL)
			func(argc, argv);
		else {
			struct lua_cmd_entry *entry;

			entry = lua_cmd_find(argv, &off);

			if (entry == NULL)
				g_print("ERR: no such command\n");
			else {
				off += 1;
				exec_lua_cmd(argc - off, argv + off, entry);
			}
		}

		g_strfreev(argv);
	}

	return ret;
}

static void readline_sigint_handler(int signo)
{
	if (signo == SIGINT)
		siglongjmp(jmp_env, 1);
}

static void readline_loop(char *prompt)
{
	struct sigaction act, oact;

	gchar *line;
	gchar buf[512];
	gchar *p;

	using_history();
	initialize_readline();

	memset(&act, 0, sizeof(act));
	act.sa_handler = readline_sigint_handler;
	if (sigaction(SIGINT, &act, &oact) < 0)
		err_sys("sigaction");

	if (sigsetjmp(jmp_env, 1))
		g_print("\n");

	for (;;) {
		p = node_getcwd();
		if (p == NULL)
			g_stpcpy(buf, prompt);
		else
			g_sprintf(buf, "[%s]%s", p, prompt);

		if ((line = readline(buf)) == NULL)
			break;

		if (parse_line(line))
			add_history(line);
		free(line);

	}

	if (sigaction(SIGINT, &oact, NULL) < 0)
		err_sys("sigaction");

	g_print("\n");
}

static void read_loop(void)
{
	gchar *line = NULL;
	gsize len = 0;

	while (getline(&line, &len, stdin) >= 0)
		if (len) parse_line(line);

	g_free(line);
}

static void sigint_handler(int signo)
{
	if (signo == SIGINT)
		caught_sigint = TRUE;
}

static void init_signals(gboolean catch_int)
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &act, NULL) < 0)
		err_sys("sigaction");

	if (catch_int) {
		act.sa_handler = sigint_handler;
		if (sigaction(SIGINT, &act, NULL) < 0)
			err_sys("sigaction");
	}
}

void lua_cmd_push(struct lua_cmd_entry *entry)
{
	g_queue_push_tail(&lua_cmd_queue, entry);
}

static gboolean read_env_args(const gchar **host, const gchar **serv)
{
	gchar *var;
	gchar *s;

	var = getenv("MBB_HOST");
	if (var == NULL)
		return FALSE;

	s = strrchr(var, ':');
	if (s == NULL)
		goto fail;

	if (s == var || s[1] == '\0')
		goto fail;

	*host = g_strndup(var, s - var);
	*serv = g_strdup(s + 1);

	return TRUE;

fail:
	msg_warn("invalid MBB_HOST variable");
	return FALSE;
}

int main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
	GOptionEntry option_entries[] = {
		{ "host", 'h', 0, G_OPTION_ARG_STRING, &opt_host, "remote hostname", "127.0.0.1" },
		{ "serv", 's', 0, G_OPTION_ARG_STRING, &opt_serv, "service", NULL },
		{ "user", 'u', 0, G_OPTION_ARG_STRING, &opt_user, "login", NULL },
		{ "pass", 'p', 0, G_OPTION_ARG_STRING, &opt_pass, "password", NULL },
		{ "key", 'k', 0, G_OPTION_ARG_STRING, &opt_key, "auth key", NULL },
		{ "init", 'i', 0, G_OPTION_ARG_STRING, &opt_dir, "init.lua directory", NULL },
		{ "load", 'l', 0, G_OPTION_ARG_STRING, &opt_file, "lua file to execute", NULL },
		{ "nosh", 'n', 0, G_OPTION_ARG_NONE, &opt_nosh, "no shell, scripts only", NULL },
		{ "follow", 'f', 0, G_OPTION_ARG_NONE, &opt_follow, "show output when stdin is closed", NULL },
		{ NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
	};

	GOptionContext *context;
	GError *error;
	gboolean isterm;

	isterm = isatty(STDIN_FILENO);
	if (isterm < 0)
		err_sys("isatty");

	g_thread_init(NULL);

	opt_user = g_get_user_name();
	option_entries[2].arg_description = opt_user;
	opt_dir = g_strdup_printf("%s/mbblua", g_get_user_config_dir());
	option_entries[5].arg_description = opt_dir;

	if (read_env_args(&opt_host, &opt_serv)) {
		option_entries[0].arg_description = opt_host;
		option_entries[1].arg_description = opt_serv;
	}

	error = NULL;
	context = g_option_context_new("- mbb lua shell");
	g_option_context_add_main_entries(context, option_entries, NULL);

	if (!g_option_context_parse(context, &argc, &argv, &error))
		errx(1, "%s", error->message);
	g_option_context_free(context);

	if (opt_follow && isterm)
		errx(1, "--follow option only for non term input");

	if (opt_nosh && opt_file == NULL)
		errx(1, "--nosh option only with --file option");

	if (opt_serv == NULL)
		errx(1, "serv option missed");

	init_signals(opt_follow);

	lua_env = lua_env_new((gchar *) opt_dir);
	if (lua_env == NULL)
		errx(1, "lua_env_new failed");

	if (lua_env_init(lua_env, &error) == FALSE)
		errx(1, "lua_env_init: %s", error->message);

	talk_register_handler("response", sync_handler);
	talk_register_handler("auth", sync_handler);
	talk_register_handler("log", log_handler);

	if (talk_init((gchar *) opt_host, (gchar *) opt_serv) == FALSE)
		errx(1, "talk failed");

	if (opt_key == NULL) {
		if (opt_pass == NULL) {
			opt_pass = getpass("password: ");
			if (opt_pass == NULL)
				err(1, "getpass");
		}

		if (auth_plain(opt_user, opt_pass) == FALSE)
			errx(1, "auth failed");
	} else if (auth_key(opt_key) == FALSE)
		errx(1, "auth failed");

	if (opt_file != NULL) {
		if (! lua_env_dofile(lua_env, opt_file, &error)) {
			lua_env_close(lua_env);
			talk_fini();

			errx(1, "lua_env_dofile: %s", error->message);
		}
	}

	if (opt_nosh) {
		lua_env_close(lua_env);
		talk_fini();

		return 0;
	}

	g_queue_sort(&lua_cmd_queue, (GCompareDataFunc) lua_cmd_entry_cmp, NULL);

	reload_func_tree();

	if (isterm)
		readline_loop("> ");
	else
		read_loop();

	if (opt_follow) {
		for (; ! caught_sigint;)
			pause();

		g_printerr("\nexit by signal\n");
	}

	talk_fini();
	lua_env_close(lua_env);

	return 0;
}

