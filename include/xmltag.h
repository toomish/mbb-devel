/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef XML_H
#define XML_H

#include <stdio.h>

#include <glib.h>

#include "variant.h"
#include "macros.h"

typedef struct xml_tag XmlTag;
typedef struct xml_tag_var XmlTagVar;

struct xml_tag {
	gchar *name;
	GHashTable *attrs;
	Variant *body;
	GHashTable *childs;

	struct xml_tag *next;
};

struct xml_tag_var {
	gchar *path;
	gchar *name;
	gboolean (*conv)(gchar *arg, gpointer p);
};

#define xml_tag_source() \
	get_xml_tag_source(__FILE__, (gchar *) __FUNCTION__, __LINE__)

#define xml_tag_newc(...) xml_tag_new(__VA_ARGS__, NULL, NULL)
#define xml_tag_scan(tag, pp, argp, ...) xml_tag_get_values(tag, pp, argp, __VA_ARGS__, NULL)

#define xml_tag_new_child(tag, ...) xml_tag_add_child(tag, xml_tag_newc(__VA_ARGS__))

XmlTag *xml_tag_new(gchar *name, ...) __sentinel(1);

void xml_tag_set_attr(XmlTag *tag, gchar *opt_name, Variant *var);
Variant *xml_tag_get_attr(XmlTag *tag, gchar *opt_name);

XmlTag *xml_tag_get_child(XmlTag *tag, gchar *name);

void xml_tag_set_body(XmlTag *tag, Variant *var);
Variant *xml_tag_get_body(XmlTag *tag);

void xml_tag_free(XmlTag *tag);

XmlTag *xml_tag_add_child(XmlTag *tag, XmlTag *child);
void xml_tag_reorder(XmlTag *tag);
void xml_tag_reorder_all(XmlTag *tag);

void xml_tag_sort(XmlTag *tag, gchar *name, GCompareFunc func);
void xml_tag_sort_with_data(XmlTag *tag, gchar *name, GCompareDataFunc func,
			    gpointer data);
void xml_tag_sort_by_attr(XmlTag *tag, gchar *name, gchar *attr);

void xml_tag_print(XmlTag *tag, FILE *fp);
gchar *xml_tag_to_string(XmlTag *tag);
gchar *xml_tag_to_string_escape(XmlTag *tag);
gchar *xml_tag_to_json(XmlTag *tag);

XmlTag *get_xml_tag_source(gchar *file, gchar *func, gint line);

XmlTag *xml_tag_path(XmlTag *tag, gchar *path);
Variant *xml_tag_path_attr(XmlTag *tag, gchar *path, gchar *attr);
Variant *xml_tag_path_body(XmlTag *tag, gchar *path);

GSList *xml_tag_path_list(XmlTag *tag, gchar *path);
GSList *xml_tag_path_attr_list(XmlTag *tag, gchar *path, gchar *attr);
GSList *xml_tag_path_body_list(XmlTag *tag, gchar *path);

gpointer xml_tag_epath(XmlTag *tag, gchar *epath);

gint xml_tag_get_values(XmlTag *tag, XmlTagVar **pp, gchar **argp, ...);
gint xml_tag_get_valuesv(XmlTag *tag, struct xml_tag_var **pp, gchar **argp, va_list ap);

#endif
