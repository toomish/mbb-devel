/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_FUNC_H
#define MBB_FUNC_H

#include <glib.h>

#include "mbbmodule.h"
#include "mbbcap.h"
#include "xmltag.h"

typedef void (*mbb_func_t)(XmlTag *tag, XmlTag **ans);

struct mbb_func_struct {
	gchar *name;
	mbb_func_t func;
	mbb_cap_t cap_mask;
	MbbModule *module;
};

#define MBB_FUNC(name) func_##name##_struct

#define MBB_FUNC_REGISTER_STRUCT(n, f, m) \
static struct mbb_func_struct MBB_FUNC(f) = { \
	.name = n, \
	.func = f, \
	.cap_mask = m \
}

#ifdef MBB_INIT_STRUCT
#define MBB_INIT_FUNC_STRUCT(f) MBB_INIT_STRUCT(mbb_func_register, &MBB_FUNC(f))
#endif

#define MBB_DEFINE_FUNC(func, method, cap) \
	static void func(XmlTag *tag, XmlTag **ans); \
	MBB_FUNC_REGISTER_STRUCT(method, func, cap); \
	static void func(XmlTag *tag, XmlTag **ans)

void mbb_func_register(struct mbb_func_struct *func_struct);
gboolean mbb_func_mregister(struct mbb_func_struct *func_struct, MbbModule *mod);
void mbb_func_unregister(struct mbb_func_struct *func_struct);
void mbb_func_unregister_list(GSList *list);

GSList *mbb_func_get_methods(mbb_cap_t mask);
gboolean mbb_func_call(gchar *name, XmlTag *tag, XmlTag **ans);

#endif
