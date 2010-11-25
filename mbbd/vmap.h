/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef VARIOUS_MAP_H
#define VARIOUS_MAP_H

#include "inetslicer.h"
#include "timeslicer.h"
#include "map.h"

static inline void inet_map_init(struct map *map)
{
	map_init(map, &time_ops);
	map_init_slicer(map, inet_slicer_new);
}

static inline void time_map_init(struct map *map)
{
	map_init(map, &inet_ops);
	map_init_slicer(map, time_slicer_new);
}

#endif
