/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <string.h>

#include "nettalker.h"
#include "methods.h"
#include "handler.h"

#include "xmltag.h"

GHashTable *get_methods(void)
{
	GHashTable *ht;
	Variant *var;
	XmlTag *tag;
	XmlTag *xt;

	tag = talk_half_say("<request name='mbb-self-show-methods'/>");

	var = xml_tag_get_attr(tag, "result");
	if (var == NULL || strcmp(variant_get_string(var), "ok")) {
		talk_half_say(NULL);
		return NULL;
	}

	ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	xt = xml_tag_get_child(tag, "method");
	for (; xt != NULL; xt = xt->next) {
		var = xml_tag_get_attr(xt, "name");
		if (var != NULL)
			g_hash_table_insert(
				ht, g_strdup(variant_get_string(var)),
				GINT_TO_POINTER(1)
			);
	}

	talk_half_say(NULL);
	return ht;
}

