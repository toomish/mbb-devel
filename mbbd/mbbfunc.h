/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_FUNC_H
#define MBB_FUNC_H

#include <glib.h>

#include "mbbmodule.h"
#include "mbbcap.h"
#include "xmltag.h"

#ifdef MBB_INIT_STRUCT
#define MBB_INIT_FUNCTIONS_TABLE mbb_func_table__

#define MBB_INIT_FUNCTIONS_DO \
static struct mbb_func_struct MBB_INIT_FUNCTIONS_TABLE[] = {

#define MBB_INIT_FUNCTIONS_END \
	MBB_FUNC_STRUCT(NULL, NULL, 0) \
};

#define MBB_FUNC_STRUCT(name, func, cap) { name, func, cap, NULL }

#define MBB_INIT_FUNCTIONS \
MBB_INIT_STRUCT(mbb_func_register_all, MBB_INIT_FUNCTIONS_TABLE)
#endif

typedef void (*mbb_func_t)(XmlTag *tag, XmlTag **ans);

struct mbb_func_struct {
	gchar *name;
	mbb_func_t func;
	mbb_cap_t cap_mask;
	MbbModule *module;
};

void mbb_func_register_all(struct mbb_func_struct *func_struct);
guint mbb_func_mregister(struct mbb_func_struct *func_struct, MbbModule *mod);
void mbb_func_unregister(struct mbb_func_struct *func_struct);
void mbb_func_unregister_all(struct mbb_func_struct *func_struct);

GSList *mbb_func_get_methods(mbb_cap_t mask);
gboolean mbb_func_call(gchar *name, XmlTag *tag, XmlTag **ans);

#endif
