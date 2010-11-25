/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_OBJECT_H
#define MBB_OBJECT_H

#include <glib.h>
#include <time.h>

#define DEFINE_METHOD(type, method) \
static inline type mbb_object_##method(struct mbb_object *to) \
{ \
	return to->methods->method(to->ptr); \
}

#define DEFINE_CALLBACK(name, type, method) \
static inline type mbb_object_##name(struct mbb_object *to, gpointer self) \
{ \
	to->methods->method(self); \
}

struct mbb_inet_pool;

typedef gint (*get_int_func_t)(gpointer ptr);
typedef time_t (*get_time_func_t)(gpointer ptr);
typedef gpointer (*get_ptr_func_t)(gpointer ptr);
typedef void (*void_func_t)(gpointer ptr);

struct mbb_object_interface {
	get_int_func_t get_id;
	get_time_func_t get_start;
	get_time_func_t get_end;
	get_ptr_func_t get_pool;
	void_func_t on_delete;
};

struct mbb_object {
	gpointer ptr;
	struct mbb_object_interface *methods;
};

DEFINE_METHOD(gint, get_id)
DEFINE_METHOD(time_t, get_start)
DEFINE_METHOD(time_t, get_end)
DEFINE_METHOD(struct mbb_inet_pool *, get_pool)
DEFINE_CALLBACK(emit_del, void, on_delete)

#endif
