#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "xmltag.h"

typedef void (*XTPrintFunc)(gpointer, const gchar *fmt, ...);
typedef gpointer (*XTActionFunc)(XmlTag *tag, gpointer data);

struct process_data {
	XTPrintFunc func;
	gpointer data;
	gboolean escape;
};

struct walk_data {
	GSList *list;
	XTActionFunc func;
	gpointer data;
};

#define PD_APPLY(pd, ...) pd->func(pd->data, __VA_ARGS__)

void xml_tag_set_attr(XmlTag *tag, gchar *opt_name, Variant *var)
{
	g_return_if_fail(opt_name != NULL && var != NULL);

	if (tag->attrs == NULL) {
		tag->attrs = g_hash_table_new_full(
			g_str_hash, g_str_equal,
			NULL, (GDestroyNotify) variant_free
		);
	}

	g_hash_table_replace(tag->attrs, opt_name, var);
}

Variant *xml_tag_get_attr(XmlTag *tag, gchar *opt_name)
{
	if (tag->attrs == NULL)
		return NULL;

	return g_hash_table_lookup(tag->attrs, opt_name);
}

XmlTag *xml_tag_new(gchar *name, ...)
{
	XmlTag *tag;
	va_list ap;
	gchar *arg;

	tag = g_new(XmlTag, 1);
	tag->name = name;
	tag->attrs = NULL;
	tag->childs = NULL;
	tag->next = NULL;

	va_start(ap, name);
	while ((arg = va_arg(ap, gchar *)) != NULL) {
		Variant *value;

		value = va_arg(ap, Variant *);
		xml_tag_set_attr(tag, arg, value);
	}

	tag->body = va_arg(ap, Variant *);
	va_end(ap);

	return tag;
}

XmlTag *xml_tag_get_child(XmlTag *tag, gchar *name)
{
	if (tag->childs == NULL)
		return NULL;

	return g_hash_table_lookup(tag->childs, name);
}

void xml_tag_set_body(XmlTag *tag, Variant *var)
{
	tag->body = var;
}

Variant *xml_tag_get_body(XmlTag *tag)
{
	return tag->body;
}

void xml_tag_free(XmlTag *tag)
{
	XmlTag *next;

	for (; tag != NULL; tag = next) {
		next = tag->next;

		if (tag->body != NULL)
			variant_free(tag->body);
		if (tag->attrs != NULL)
			g_hash_table_destroy(tag->attrs);
		if (tag->childs != NULL)
			g_hash_table_destroy(tag->childs);
		g_free(tag);
	}
}

XmlTag *xml_tag_add_child(XmlTag *tag, XmlTag *child)
{
	if (tag->childs == NULL) {
		tag->childs = g_hash_table_new_full(
			g_str_hash, g_str_equal,
			NULL, (GDestroyNotify) xml_tag_free
		);

		g_hash_table_insert(tag->childs, child->name, child);
	} else {
		XmlTag *prev;

		prev = g_hash_table_lookup(tag->childs, child->name);
		if (prev == NULL)
			g_hash_table_insert(tag->childs, child->name, child);
		else {
			child->next = prev->next;
			prev->next = child;
		}
	}

	return child;
}

void xml_tag_reorder(XmlTag *tag)
{
	XmlTag *next;
	XmlTag *xt;

	if (tag == NULL)
		return;

	if ((xt = tag->next) != NULL && (next = xt->next) != NULL) {
		xt->next = NULL;

		for (xt = next; xt != NULL; xt = next) {
			next = xt->next;
			xt->next = tag->next;
			tag->next = xt;
		}
	}
}

void xml_tag_reorder_all(XmlTag *tag)
{
	GHashTableIter iter;
	XmlTag *xt;

	xml_tag_reorder(tag);

	if (tag->childs == NULL)
		return;

	g_hash_table_iter_init(&iter, tag->childs);
	while (g_hash_table_iter_next(&iter, NULL, (gpointer *) &xt))
		xml_tag_reorder_all(xt);
}

static XmlTag *xml_tag_sort_merge(XmlTag *xt1, XmlTag *xt2, GFunc func,
				  gpointer data)
{
	XmlTag tag, *xt;
	gint cmp;

	xt = &tag;

	while (xt1 != NULL && xt2 != NULL) {
		cmp = ((GCompareDataFunc) func) (xt1, xt2, data);

		if (cmp <= 0) {
			xt = xt->next = xt1;
			xt1 = xt1->next;
		} else {
			xt = xt->next = xt2;
			xt2 = xt2->next;
		}
	}

	xt->next = xt1 != NULL ? xt1 : xt2;

	return tag.next;
}

static XmlTag *xml_tag_sort_real(XmlTag *tag, GFunc func, gpointer data)
{
	XmlTag *xt1, *xt2;

	if (tag == NULL)
		return NULL;
	if (tag->next == NULL)
		return tag;

	xt1 = tag;
	xt2 = tag->next;

	while ((xt2 = xt2->next) != NULL) {
		if ((xt2 = xt2->next) == NULL)
			break;
		xt1 = xt1->next;
	}

	xt2 = xt1->next;
	xt1->next = NULL;

	xt1 = xml_tag_sort_real(tag, func, data);
	xt2 = xml_tag_sort_real(xt2, func, data);

	return xml_tag_sort_merge(xt1, xt2, func, data);
}

void xml_tag_sort(XmlTag *tag, gchar *name, GCompareFunc func)
{
	xml_tag_sort_with_data(tag, name, (GCompareDataFunc) func, NULL);
}

void xml_tag_sort_with_data(XmlTag *tag, gchar *name, GCompareDataFunc func,
			    gpointer data)
{
	XmlTag *xt;

	xt = xml_tag_get_child(tag, name);

	if (xt != NULL) {
		XmlTag *old = xt;

		xt = xml_tag_sort_real(xt, (GFunc) func, data);
		if (old != xt) {
			g_hash_table_steal(tag->childs, name);
			g_hash_table_insert(tag->childs, name, xt);
		}
	}
}

static gint xml_tag_attr_cmp(XmlTag *xt1, XmlTag *xt2, gchar *attr)
{
	Variant *v1, *v2;

	v1 = xml_tag_get_attr(xt1, attr);
	v2 = xml_tag_get_attr(xt2, attr);

	if (v1 == NULL)
		return - (v2 != NULL);
	if (v2 == NULL)
		return 1;

	return g_strcmp0(variant_get_string(v1), variant_get_string(v2));
}

void xml_tag_sort_by_attr(XmlTag *tag, gchar *name, gchar *attr)
{
	xml_tag_sort_with_data(tag, name,
		(GCompareDataFunc) xml_tag_attr_cmp, attr
	);
}

static void xml_tag_process_attr(gchar *opt_name, Variant *var,
				 struct process_data *pd)
{
	gchar *value;

	value = variant_to_string(var);
	if (value != NULL) {
		if (pd->escape) {
			gchar *tmp;

			tmp = g_markup_escape_text(value, -1);
			g_free(value);
			value = tmp;
		}

		PD_APPLY(pd, " %s='%s'", opt_name, value);
		g_free(value);
	}
}

static void xml_tag_process(XmlTag *tag, struct process_data *pd);

static void foreach_child(gpointer key, gpointer value, gpointer data)
{
	(void) key;

	xml_tag_process(value, data);
}

static void xml_tag_process(XmlTag *tag, struct process_data *pd)
{
	for (; tag != NULL; tag = tag->next) {
		PD_APPLY(pd, "<%s", tag->name);
		if (tag->attrs != NULL)
			g_hash_table_foreach(
				tag->attrs, (GHFunc) xml_tag_process_attr, pd
			);

		if (tag->body == NULL && tag->childs == NULL) {
			PD_APPLY(pd, "/>"); /* \n */
			continue;
		}

		PD_APPLY(pd, ">");

		if (tag->body != NULL) {
			gchar *value;

			value = variant_to_string(tag->body);
			if (value != NULL) {
				/*
				if (tag->childs != NULL)
					PD_APPLY(pd, "\n");
				*/

				if (pd->escape) {
					gchar *tmp;

					tmp = g_markup_escape_text(value, -1);
					g_free(value);
					value = tmp;
				}

				PD_APPLY(pd, "%s", value);
				g_free(value);
			}
		}

		if (tag->childs != NULL) {
			/* PD_APPLY(pd, "\n"); */
			g_hash_table_foreach(tag->childs, foreach_child, pd);
		}

		PD_APPLY(pd, "</%s>", tag->name); /* \n */
	}
}

void xml_tag_print(XmlTag *tag, FILE *fp)
{
	struct process_data pd;

	pd.func = (XTPrintFunc) fprintf;
	pd.data = fp;
	pd.escape = FALSE;

	xml_tag_process(tag, &pd);
}

static gchar *xml_tag_to_string_common(XmlTag *tag, gboolean escape)
{
	struct process_data pd;
	GString *string;

	string = g_string_new(NULL);
	pd.func = (XTPrintFunc) g_string_append_printf;
	pd.data = string;
	pd.escape = escape;

	xml_tag_process(tag, &pd);

	return g_string_free(string, FALSE);
}

gchar *xml_tag_to_string(XmlTag *tag)
{
	return xml_tag_to_string_common(tag, FALSE);
}

gchar *xml_tag_to_string_escape(XmlTag *tag)
{
	return xml_tag_to_string_common(tag, TRUE);
}

static void json_escape(gchar *str, GString *string)
{
	static gchar *escape_chars  = "\\/\"\b\f\n\r\t";
	static gchar *escape_pchars = "\\/\"bfnrt";

	gunichar uc;
	gchar *p;

	g_string_append_c(string, '\"');
	for (p = str; *p != '\0'; p = g_utf8_next_char(p)) {
		uc = g_utf8_get_char(p);

		if (uc >= 0x80)
			g_string_append_unichar(string, uc);
		else {
			gchar *s;
			gsize off;

			s = strchr(escape_chars, uc);
			if (s != NULL) {
				off = s - escape_chars;
				g_string_append_printf(
					string, "\\%c", escape_pchars[off]
				);
			} else {
				if (uc < ' ')
					g_string_append_printf(string, "\\u%x", uc);
				else
					g_string_append_unichar(string, uc);
			}
		}
	}

	g_string_append_c(string, '\"');
}

static void xml_tag_to_json_add_attr(gchar *key, Variant *var, GString *string)
{
	gchar *value;

	value = variant_to_string(var);
	if (value == NULL)
		return;

	if (string->str[string->len - 1] != '{')
		g_string_append_c(string, ',');

	json_escape(key, string);
	g_string_append_c(string, ':');
	json_escape(value, string);

	g_free(value);
}

static void xml_tag_to_json_add_tag(XmlTag *tag, GString *string);

static void xml_tag_to_json_add(gchar *name, XmlTag *tag, GString *string)
{
	/* gboolean array = FALSE; */

	if (string->str[string->len - 1] != '{')
		g_string_append_c(string, ',');

	json_escape(name, string);
	g_string_append_c(string, ':');

	/*
	if (tag->next != NULL) {
		g_string_append_c(string, '[');
		array = TRUE;
	}
	*/

	g_string_append_c(string, '[');

	for (; tag != NULL; tag = tag->next) {
		xml_tag_to_json_add_tag(tag, string);

		if (tag->next != NULL)
			g_string_append_c(string, ',');
	}

	g_string_append_c(string, ']');

	/*
	if (array)
		g_string_append_c(string, ']');
	*/
}

static void xml_tag_to_json_add_tag(XmlTag *tag, GString *string)
{
	g_string_append_c(string, '{');

	if (tag->attrs != NULL)
		g_hash_table_foreach(tag->attrs,
			(GHFunc) xml_tag_to_json_add_attr, string
		);

	if (tag->childs != NULL)
		g_hash_table_foreach(tag->childs,
			(GHFunc) xml_tag_to_json_add, string
		);

	g_string_append_c(string, '}');
}

gchar *xml_tag_to_json(XmlTag *tag)
{
	GString *string;

	string = g_string_new(NULL);
	xml_tag_to_json_add_tag(tag, string);

	return g_string_free(string, FALSE);
}

XmlTag *get_xml_tag_source(gchar *file, gchar *func, gint line)
{
	XmlTag *tag;

	tag = xml_tag_new("source", 
		"file", variant_new_static_string(file),
		"func", variant_new_static_string(func),
		"line", variant_new_int(line),
		NULL, NULL
	);

	return tag;
}

XmlTag *xml_tag_path(XmlTag *tag, gchar *path)
{
	XmlTag *child;
	gchar **names, **pp;

	if (path == NULL)
		return tag;

	child = tag;
	names = g_strsplit(path, "/", -1);
	for (pp = names; *pp != NULL; pp++) {
		child = xml_tag_get_child(child, *pp);
		if (child == NULL)
			break;
	}

	g_strfreev(names);
	return child;
}

static void xml_tag_path_walk(XmlTag *tag, gchar **names, struct walk_data *wd)
{
	if (*names == NULL) {
		if (wd->func == NULL)
			wd->list = g_slist_prepend(wd->list, tag);
		else {
			gpointer ptr;

			ptr = wd->func(tag, wd->data);
			if (ptr != NULL)
				wd->list = g_slist_prepend(wd->list, ptr);
		}
	} else {
		XmlTag *child;

		child = xml_tag_get_child(tag, *names);
		for (; child != NULL; child = child->next)
			xml_tag_path_walk(child, names + 1, wd);
	}
}

static void xml_tag_path_go(XmlTag *tag, gchar *path, struct walk_data *wd)
{
	gchar **names;

	names = g_strsplit(path, "/", -1);
	xml_tag_path_walk(tag, names, wd);
	g_strfreev(names);
}

GSList *xml_tag_path_list(XmlTag *tag, gchar *path)
{
	struct walk_data wd;

	wd.list = NULL;
	wd.func = NULL;
	wd.data = NULL;

	xml_tag_path_go(tag, path, &wd);

	return wd.list;
}

Variant *xml_tag_path_attr(XmlTag *tag, gchar *path, gchar *attr)
{
	XmlTag *child;

	child = xml_tag_path(tag, path);
	if (child == NULL)
		return NULL;

	return xml_tag_get_attr(child, attr);
}

GSList *xml_tag_path_attr_list(XmlTag *tag, gchar *path, gchar *attr)
{
	struct walk_data wd;

	wd.list = NULL;
	wd.func = (XTActionFunc) xml_tag_get_attr;
	wd.data = attr;

	xml_tag_path_go(tag, path, &wd);

	return wd.list;
}

Variant *xml_tag_path_body(XmlTag *tag, gchar *path)
{
	XmlTag *child;

	child = xml_tag_path(tag, path);
	if (child == NULL)
		return NULL;
	return xml_tag_get_body(child);
}

GSList *xml_tag_path_body_list(XmlTag *tag, gchar *path)
{
	struct walk_data wd;

	wd.list = NULL;
	wd.func = (XTActionFunc) xml_tag_get_body;
	wd.data = NULL;

	xml_tag_path_go(tag, path, &wd);

	return wd.list;
}

gint xml_tag_get_valuesv(XmlTag *tag, struct xml_tag_var **pp, gchar **argp, va_list ap)
{
	struct xml_tag_var *xtv;
	gpointer ptr;
	gint no;

	no = 0;
	while ((ptr = va_arg(ap, gpointer)) != NULL) {
		gboolean optional;
		Variant *var;

		xtv = *pp++;
		optional = (gboolean) GPOINTER_TO_INT(*pp);
		pp++;

		if (xtv->name == NULL)
			var = xml_tag_path_body(tag, xtv->path);
		else
			var = xml_tag_path_attr(tag, xtv->path, xtv->name);

		if (var == NULL) {
			if (optional == FALSE) {
				va_end(ap);
				return no;
			}
		} else {
			gchar *str;

			str = variant_get_string(var);
			if (xtv->conv == NULL)
				*(gchar **) ptr = str;
			else if (xtv->conv(str, ptr) == FALSE) {
				if (argp != NULL)
					*argp = str;
				va_end(ap);
				return no;
			}
		}

		++no;
	}

	return -1;
}

gint xml_tag_get_values(XmlTag *tag, struct xml_tag_var **pp, gchar **argp, ...)
{
	va_list ap;
	gint ret;

	va_start(ap, argp);
	ret = xml_tag_get_valuesv(tag, pp, argp, ap);
	va_end(ap);

	return ret;
}

gpointer xml_tag_epath(XmlTag *tag, gchar *epath)
{
	gchar *path, *opt;
	gchar **fields;
	gboolean one;
	gpointer p;

	if (*epath != '*')
		one = TRUE;
	else {
		epath++;
		one = FALSE;
	}

	fields = g_strsplit(epath, ":", 2);
	path = fields[0];
	if (path == NULL) {
		g_strfreev(fields);
		return tag;
	}

	opt = fields[1];
	if (opt == NULL) {
		if (one)
			p = xml_tag_path(tag, path);
		else
			p = xml_tag_path_list(tag, path);
	} else if (! strcmp(opt, "")) {
		if (one)
			p = xml_tag_path_body(tag, path);
		else
			p = xml_tag_path_body_list(tag, path);
	} else if (one)
		p = xml_tag_path_attr(tag, path, opt);
	else
		p = xml_tag_path_attr_list(tag, path, opt);

	g_strfreev(fields);
	return p;
}

