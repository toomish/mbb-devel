#include <string.h>
#include <errno.h>

#include "strconv.h"

GQuark str_conv_error_quark(void)
{
	return g_quark_from_static_string("str-conv-error-quark");
}

gint64 str_conv_to_int64(const gchar *str, GError **error)
{
	gchar *endptr;
	gint64 val;
	guint base = 10;

	errno = 0;
	val = g_ascii_strtoll(str, &endptr, base);
	if ((val == G_MAXINT64 || val == G_MININT64) && errno == ERANGE) {
		g_set_error(error, STR_CONV_ERROR,
			STR_CONV_ERROR_OVERFLOW,
			"number '%s' is too large", str);
		return 0;
	}

	if (val == 0) {
		if (errno == EINVAL) {
			g_set_error(error, STR_CONV_ERROR,
				STR_CONV_ERROR_INVALID_BASE,
				"base '%d' is invalid", base);
			return 0;
		}

		if (endptr == str) {
			g_set_error(error, STR_CONV_ERROR,
				STR_CONV_ERROR_FAIL,
				"number '%s' invalid", str);
			return 0;
		}
	}

	if (*endptr != '\0') {
		g_set_error(error, STR_CONV_ERROR,
			STR_CONV_ERROR_EXTRA_SYMBOLS,
			"extra symbols in number '%s'", str);
		return 0;
	}

	return val;
}

guint64 str_conv_to_uint64(const gchar *str, GError **error)
{
	gchar *endptr;
	guint64 val;
	guint base = 10;

	if (! g_ascii_isdigit(*str)) {	/* check for negative numbers */
		g_set_error(error, STR_CONV_ERROR,
			STR_CONV_ERROR_FAIL,
			"number '%s' is invalid", str);
		return 0;
	}

	errno = 0;
	val = g_ascii_strtoull(str, &endptr, base);
	if (val == G_MAXUINT64 && errno == ERANGE) {
		g_set_error(error, STR_CONV_ERROR,
			STR_CONV_ERROR_OVERFLOW,
			"number '%s' is too large", str);
		return 0;
	}

	if (val == 0) {
		if (errno == EINVAL) {
			g_set_error(error, STR_CONV_ERROR,
				STR_CONV_ERROR_INVALID_BASE,
				"base '%d' is invalid", base);
			return 0;
		}

		if (endptr == str) {
			g_set_error(error, STR_CONV_ERROR,
				STR_CONV_ERROR_FAIL,
				"number '%s' invalid", str);
			return 0;
		}
	}

	if (*endptr != '\0') {
		g_set_error(error, STR_CONV_ERROR,
			STR_CONV_ERROR_EXTRA_SYMBOLS,
			"extra symbols in number '%s'", str);
		return 0;
	}

	return val;
}

guint str_conv_to_mask(const gchar *str, GError **error)
{
	guint32 mask;
	const gchar *p;
	gint digit;

	mask = 0U;
	for (p = str; *p != '\0'; p++) {
		digit = g_ascii_digit_value(*p);
		if (digit < 0 || digit > 1) {
			g_set_error(error, STR_CONV_ERROR,
				STR_CONV_ERROR_FAIL,
				"invalid bit mask '%s'", str);
			return 0;
		}

		mask <<= 1;
		mask |= digit;
	}

	return mask;
}

