/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef PATH_TREE_H
#define PATH_TREE_H

#include "trash.h"

struct path_tree {
	GPtrArray *array;
	Trash trash;
	guint cur;
};

gboolean path_tree_walk(struct path_tree *pt, GSList *list);
gchar *path_tree_next(struct path_tree *pt);
void path_tree_free(struct path_tree *pt);

#endif
