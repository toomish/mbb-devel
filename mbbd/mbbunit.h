/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_ACC_UNIT_H
#define MBB_ACC_UNIT_H

#include <glib.h>
#include <time.h>

#include "mbbconsumer.h"
#include "mbbinetpool.h"
#include "mbbobject.h"
#include "mbbmacro.h"

#include "separator.h"
#include "vmap.h"
#include "re.h"

typedef struct mbb_unit MbbUnit;

struct mbb_unit {
	gint id;
	gchar *name;

	MBB_OBJ_REF;

	struct mbb_consumer *con;

	gboolean local;

	time_t start;
	time_t end;

	Separator sep;
	struct mbb_object self;

	struct map map;
};

struct mbb_unit *mbb_unit_new(gint id, gchar *name, time_t start, time_t end);
void mbb_unit_free(struct mbb_unit *unit);
void mbb_unit_join(struct mbb_unit *unit);
void mbb_unit_detach(struct mbb_unit *unit);
void mbb_unit_remove(struct mbb_unit *unit);

MBB_OBJ_DEFINE_REF(unit)

void mbb_unit_local_add(struct mbb_unit *unit);
void mbb_unit_local_del(struct mbb_unit *unit);

gint mbb_unit_get_id(struct mbb_unit *unit);
time_t mbb_unit_get_start(struct mbb_unit *unit);
time_t mbb_unit_get_end(struct mbb_unit *unit);
MbbInetPool *mbb_unit_get_pool(void);

gboolean mbb_unit_test_time(struct mbb_unit *unit, time_t start, time_t end);

gboolean mbb_unit_mod_name(struct mbb_unit *unit, gchar *newname);
gboolean mbb_unit_mod_time(struct mbb_unit *unit, time_t start, time_t end);

struct mbb_unit *mbb_unit_get_by_id(gint id);
struct mbb_unit *mbb_unit_get_by_name(gchar *name);

void mbb_unit_foreach(GFunc func, gpointer user_data);
void mbb_unit_forregex(Regex re, GFunc func, gpointer user_data);
void mbb_unit_local_forregex(Regex re, GFunc func, gpointer user_data);

void mbb_unit_inherit_time(struct mbb_unit *unit);
gboolean mbb_unit_unset_consumer(struct mbb_unit *unit);
void mbb_unit_set_consumer(struct mbb_unit *unit, struct mbb_consumer *con);

void mbb_unit_add_inet(struct mbb_unit *unit, MbbInetPoolEntry *entry);
void mbb_unit_del_inet(struct mbb_unit *unit, MbbInetPoolEntry *entry);
void mbb_unit_clear_inet(struct mbb_unit *unit);
void mbb_unit_sep_reorder(MbbInetPoolEntry *entry);

gboolean mbb_unit_map_rebuild(struct mbb_unit *unit, MbbInetPoolEntry *pp[2]);
gboolean mbb_unit_mapped(struct mbb_unit *unit);

#endif
