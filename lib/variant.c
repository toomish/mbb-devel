#include "variant.h"

enum variant_type {
	VARIANT_INT,
	VARIANT_LONG,
	VARIANT_STRING,
	VARIANT_STATIC_STRING,
	VARIANT_POINTER
};

struct variant {
	enum variant_type type;
	union {
		gint num;
		glong lnum;
		gchar *str;
		struct {
			gpointer *data;
			gchar *(*convert)(gpointer *);
			void (*free)(gpointer *);
		} ptr;
	} un;
};

void variant_free(Variant *var)
{
	if (var->type == VARIANT_STRING)
		g_free(var->un.str);
	else if (var->type == VARIANT_POINTER) {
		if (var->un.ptr.free != NULL)
			var->un.ptr.free(var->un.ptr.data);
	}

	g_free(var);
}
	
Variant *variant_new_int(gint value)
{
	Variant *var;

	var = g_new(Variant, 1);
	var->type = VARIANT_INT;
	var->un.num = value;

	return var;
}

Variant *variant_new_long(glong value)
{
	Variant *var;

	var = g_new(Variant, 1);
	var->type = VARIANT_LONG;
	var->un.lnum = value;

	return var;
}

Variant *variant_new_string(gchar *str)
{
	Variant *var;

	var = g_new(Variant, 1);
	var->type = VARIANT_STRING;
	var->un.str = g_strdup(str);

	return var;
}

Variant *variant_new_alloc_string(gchar *str)
{
	Variant *var;

	var = g_new(Variant, 1);
	var->type = VARIANT_STRING;
	var->un.str = str;

	return var;
}

Variant *variant_new_static_string(gchar *str)
{
	Variant *var;

	var = g_new(Variant, 1);
	var->type = VARIANT_STATIC_STRING;
	var->un.str = str;

	return var;
}

Variant *variant_new_pointer(gpointer *data, gchar *(*convert)(gpointer *),
			     void (*free)(gpointer *))
{
	Variant *var;

	var = g_new(Variant, 1);
	var->type = VARIANT_POINTER;
	var->un.ptr.data = data;
	var->un.ptr.convert = convert;
	var->un.ptr.free = free;

	return var;
}

gboolean variant_is_int(Variant *var)
{
	return var->type == VARIANT_INT;
}

gboolean variant_is_long(Variant *var)
{
	return var->type == VARIANT_LONG;
}

gboolean variant_is_string(Variant *var)
{
	return var->type == VARIANT_STRING || var->type == VARIANT_STATIC_STRING;
}

gboolean variant_is_pointer(Variant *var)
{
	return var->type == VARIANT_POINTER;
}

gint variant_get_int(Variant *var)
{
	if (var->type != VARIANT_INT)
		return 0;

	return var->un.num;
}

glong variant_get_long(Variant *var)
{
	if (var->type == VARIANT_LONG)
		return var->un.lnum;

	if (var->type == VARIANT_INT)
		return var->un.num;

	return 0;
}

gchar *variant_get_string(Variant *var)
{
	if (var->type != VARIANT_STRING && var->type != VARIANT_STATIC_STRING)
		return NULL;

	return var->un.str;
}

gpointer *variant_get_pointer(Variant *var)
{
	if (var->type != VARIANT_POINTER)
		return NULL;

	return var->un.ptr.data;
}

gchar *variant_to_string(Variant *var)
{
	gchar *res;

	switch (var->type) {
	case VARIANT_INT:
		res = g_strdup_printf("%d", var->un.num);
		break;
	case VARIANT_LONG:
		res = g_strdup_printf("%ld", var->un.lnum);
		break;
	case VARIANT_STRING:
	case VARIANT_STATIC_STRING:
		res = g_strdup(var->un.str);
		break;
	case VARIANT_POINTER:
		if (var->un.ptr.convert == NULL)
			res = NULL;
		else
			res = var->un.ptr.convert(var->un.ptr.data);
		break;
	default:
		res = NULL;
	}

	return res;
}

