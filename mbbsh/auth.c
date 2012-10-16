/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <string.h>
#include <err.h>

#include "variant.h"
#include "xmltag.h"
#include "debug.h"

#include "auth.h"
#include "handler.h"

static gchar *salt = NULL;

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
	g_free(request);

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
	g_free(request);

	return authorized;
}

static inline void salt_free(void)
{
	if (salt != NULL) {
		g_free(salt);
		salt = NULL;
	}
}

void salt_handler(XmlTag *tag)
{
	Variant *var;

	var = xml_tag_get_body(tag);
	if (var != NULL) {
		gchar *str = variant_to_string(var);

		g_free(salt);
		salt = str;
	}
}

gboolean auth_salt(const gchar *user, const gchar *pass)
{
	GChecksum *checksum;
	gboolean authorized;
	gchar *request;

	salt_free();

	talk_say("<auth login='' type='salt'/>", auth_response, &authorized);
	if (salt == NULL) {
		warnx("auth salt is not supported");
		return FALSE;
	}

	checksum = g_checksum_new(G_CHECKSUM_MD5);
	if (checksum == NULL)
		err_quit("checksum type MD5 is not supported");

	g_checksum_update(checksum, (guchar *) pass, -1);
	g_checksum_update(checksum, (guchar *) salt, -1);

	request = g_markup_printf_escaped(
		"<auth login='%s' secret='%s' type='salt'/>",
		user, g_checksum_get_string(checksum)
	);
	g_checksum_free(checksum);

	talk_say(request, auth_response, &authorized);

	g_free(request);
	salt_free();

	return authorized;
}

