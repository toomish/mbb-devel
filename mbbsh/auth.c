#include <string.h>

#include "variant.h"
#include "xmltag.h"
#include "debug.h"

#include "auth.h"
#include "handler.h"

void auth_response(XmlTag *tag, gpointer data)
{
	Variant *var;

	if (strcmp(tag->name, "auth"))
		err_quit("invalid response %s", tag->name);

	var = xml_tag_get_attr(tag, "result");
	if (var == NULL)
		err_quit("invalid response: attr missed");

	if (!strcmp(variant_get_string(var), "ok"))
		*(gboolean *) data = TRUE;
	else
		*(gboolean *) data = FALSE;
}

gboolean auth_plain(const gchar *user, const gchar *pass)
{
	gboolean authorized;
	gchar *request;

	request = g_markup_printf_escaped(
		"<auth login='%s' secret='%s'/>",
		user, pass
	);

	talk_say(request, auth_response, &authorized);

	return authorized;
}

gboolean auth_key(const gchar *key)
{
	gboolean authorized;
	gchar *request;

	request = g_markup_printf_escaped(
		"<auth login='%s' type='key'/>", key
	);

	talk_say(request, auth_response, &authorized);

	return authorized;
}

