/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <string.h>

#include "xmlparser.h"
#include "variant.h"
#include "trash.h"

struct xml_parser {
	GMarkupParseContext *ctxt;
	GSList *tag_list;
	Trash trash;

	gboolean (*user_func)(XmlTag *tag, gpointer data);
	gpointer user_data;
};

static void parser_start_elem(GMarkupParseContext *ctxt G_GNUC_UNUSED,
			      const gchar *elem_name,
			      const gchar **attr_names,
			      const gchar **attr_values,
			      gpointer user_data,
			      GError **error G_GNUC_UNUSED)
{
	XmlParser *parser;
	XmlTag *tag;
	char *s;
	int n;

	parser = (XmlParser *) user_data;

	s = g_strdup(elem_name);
	trash_push(&parser->trash, s, NULL);
	tag = xml_tag_new(s, NULL, NULL);
	for (n = 0; attr_names[n] != NULL; n++) {
		s = g_strdup(attr_names[n]);

		trash_push(&parser->trash, s, NULL);
		xml_tag_set_attr(
			tag, s,
			variant_new_string((gchar *) attr_values[n])
		);
	}

	if (parser->tag_list != NULL) {
		XmlTag *prev_tag;

		prev_tag = parser->tag_list->data;
		xml_tag_add_child(prev_tag, tag);
	}

	parser->tag_list = g_slist_prepend(parser->tag_list, tag);
}

static gsize strip_non_graph(const gchar **text, gsize *text_len)
{
	gunichar uc;
	const gchar *s, *t, *p;
	gsize len;

	s = *text;
	len = *text_len;

	while (len) {
		uc = g_utf8_get_char(s);
		if (g_unichar_isgraph(uc))
			break;
		p = g_utf8_next_char(s);
		len -= p - s;
		s = p;
	}

	if (len) {
		t = s + len;

		while (len) {
			p = g_utf8_prev_char(t);
			uc = g_utf8_get_char(p);
			if (g_unichar_isgraph(uc))
				break;
			len -= t - p;
			t = p;
		}

		if (len) {
			*text = s;
			*text_len = len;
		}
	}

	return len;
}

static void parser_text(GMarkupParseContext *ctxt G_GNUC_UNUSED,
			const gchar *text,
			gsize text_len,
			gpointer user_data,
			GError **error G_GNUC_UNUSED)
{
	XmlParser *parser;
	Variant *var;
	XmlTag *tag;
	char *body;

	if (strip_non_graph(&text, &text_len) == 0)
		return;

	parser = (XmlParser *) user_data;
	tag = (XmlTag *) parser->tag_list->data;
	var = xml_tag_get_body(tag);

	if (var == NULL) {
		body = g_new(gchar, text_len + 1);
		memcpy(body, text, text_len);
		body[text_len] = '\0';
	} else {
		body = g_strdup_printf(
			"%s%.*s", variant_get_string(var), (int) text_len, text
		);
		variant_free(var);
	}

	xml_tag_set_body(tag, variant_new_static_string(body));
	trash_push(&parser->trash, body, NULL);
}

static void parser_end_elem(GMarkupParseContext *ctxt G_GNUC_UNUSED,
			    const gchar *elem_name,
			    gpointer user_data,
			    GError **error)
{
	XmlParser *parser;
	XmlTag *tag;
	GSList *list;

	parser = (XmlParser *) user_data;
	list = parser->tag_list;
	tag = (XmlTag *) list->data;

	if (strcmp(tag->name, elem_name)) {
		g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
			    "unexpected closed tag '%s', must be '%s'",
			    elem_name, tag->name);
		return;
	}

	parser->tag_list = g_slist_delete_link(list, list);
	if (parser->tag_list == NULL) {
		if (parser->user_func(tag, parser->user_data)) {
			xml_tag_free(tag);
			trash_empty(&parser->trash);
		}
	}
}

static GMarkupParser markup_parser = {
	.start_element = parser_start_elem,
	.end_element = parser_end_elem,
	.text = parser_text
};

XmlParser *xml_parser_new(gboolean (*func)(XmlTag *, gpointer), gpointer data)
{
	XmlParser *parser;

	parser = g_new(XmlParser, 1);
	parser->tag_list = NULL;
	parser->trash = (Trash) TRASH_INITIALIZER;
	parser->user_func = func;
	parser->user_data = data;

	parser->ctxt = g_markup_parse_context_new(&markup_parser, 0, parser, NULL);

	return parser;
}

gboolean xml_parser_parse(XmlParser *parser, gchar *text, gssize text_len,
			  GError **error)
{
	return g_markup_parse_context_parse(parser->ctxt, text, text_len, error);
}

void xml_parser_free(XmlParser *parser)
{
	if (parser->tag_list != NULL) {
		XmlTag *tag;
		GSList *list;

		list = parser->tag_list;
		tag = (XmlTag *) g_slist_last(list)->data;
		xml_tag_free(tag);
		g_slist_free(list);
	}

	trash_empty(&parser->trash);
	g_markup_parse_context_free(parser->ctxt);
	g_free(parser);
}

