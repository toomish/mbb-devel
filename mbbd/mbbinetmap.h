#ifndef MBB_MAP_H
#define MBB_MAP_H

#include "mbbunit.h"

typedef struct mbb_umap MbbUMap;

gboolean mbb_map_add_unit(MbbUnit *unit, struct map_cross *cross);

void mbb_map_del_inet(MbbInetPoolEntry *entry);
void mbb_map_del_unit(MbbUnit *unit);

MbbUMap *mbb_umap_create(void);
MbbUMap *mbb_umap_from_unit(MbbUnit *unit);
MbbUnit *mbb_umap_find(MbbUMap *umap, ipv4_t ip, time_t t);
void mbb_umap_free(MbbUMap *umap);

#endif
