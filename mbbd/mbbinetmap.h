/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_MAP_H
#define MBB_MAP_H

#include "mbbunit.h"

typedef struct mbb_umap MbbUMap;

gboolean mbb_map_add_unit(MbbUnit *unit, struct map_cross *cross);

void mbb_map_del_inet(MbbInetPoolEntry *entry);
void mbb_map_del_unit(MbbUnit *unit);

void mbb_map_clear(void);
void mbb_map_auto_glue(void);

MbbUMap *mbb_umap_create(void);
MbbUMap *mbb_umap_from_unit(MbbUnit *unit);
MbbUnit *mbb_umap_find(MbbUMap *umap, ipv4_t ip, time_t t);
void mbb_umap_free(MbbUMap *umap);

#endif
