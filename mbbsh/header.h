#ifndef HEADER_H
#define HEADER_H

struct lua_cmd_entry {
	gchar **cmdv;
	gchar *method;
	gchar *handler;

	gint arg_min;
	gint arg_max;
};

struct cmd_params {
	gint argc;
	gchar **argv;
	struct lua_cmd_entry *entry;
};

void lua_cmd_push(struct lua_cmd_entry *entry);

#endif
