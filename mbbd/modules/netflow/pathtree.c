/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include <ftw.h>

#include "pathtree.h"
#include "variant.h"

#define DIR_MARK '@'

GString *nf_get_data_dir(void);

static inline void path_tree_init(struct path_tree *pt)
{
	pt->array = g_ptr_array_new();
	pt->trash = (Trash) TRASH_INITIALIZER;
	pt->cur = 0;
}

static inline void do_glob(glob_t *gl, gchar *str)
{
	gint flags = GLOB_MARK;

	if (gl->gl_pathc)
		flags |= GLOB_APPEND;

	glob(str, flags, NULL, gl);
}

static int cmpstringp(const void *pa, const void *pb)
{
	return strcmp(*(gchar **) pa, *(gchar **) pb);
}

static inline void do_ftw(struct path_tree *pt, gchar *dirpath)
{
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	GPtrArray *array = pt->array;
	guint count;
	guint len;

	__extension__
	int walker(const char *fpath, const struct stat *sb, int typeflag)
	{
		gchar *path;

		(void) sb;

		if (typeflag == FTW_F) {
			path = g_strdup(fpath);
			g_ptr_array_add(array, path);
			trash_push(&pt->trash, path, NULL);
		}

		return 0;
	}

	len = array->len;

	g_static_mutex_lock(&mutex);
	ftw(dirpath, walker, 20);
	g_static_mutex_unlock(&mutex);

	count = array->len - len;
	if (count > 1) {
		qsort(&g_ptr_array_index(array, len),
			count, sizeof(gpointer), cmpstringp
		);
	}
}

gboolean path_tree_walk(struct path_tree *pt, GSList *list)
{
	gsize rpath_len = 0;
	GString *rpath;
	glob_t *globbuf;
	gsize start;

	path_tree_init(pt);

	if ((rpath = nf_get_data_dir()) != NULL)
		rpath_len = rpath->len;

	globbuf = g_new0(glob_t, 1);
	trash_push(&pt->trash, globbuf, NULL);
	trash_push(&pt->trash, globbuf, (GDestroyNotify) globfree);

	for (; list != NULL; list = list->next) {
		gchar *str = variant_get_string(list->data);
		gboolean dir = FALSE;

		if (strlen(str) == 0)
			continue;

		if (*str == DIR_MARK) {
			dir = TRUE;
			str++;

			if (*str == '\0')
				continue;
		}

		if (*str != '/') {
			if (rpath == NULL)
				continue;

			g_string_truncate(rpath, rpath_len);
			g_string_append(rpath, str);
			str = rpath->str;
		}

		start = globbuf->gl_pathc;
		do_glob(globbuf, str);

		if (dir == FALSE) {
			for (; start < globbuf->gl_pathc; start++)
				g_ptr_array_add(pt->array, globbuf->gl_pathv[start]);
		} else {
			gchar *dirname;

			for (; start < globbuf->gl_pathc; start++) {
				dirname = globbuf->gl_pathv[start];

				if (g_str_has_suffix(dirname, "/"))
					do_ftw(pt, dirname);
			}
		}
	}

	if (rpath != NULL)
		g_string_free(rpath, TRUE);

	if (pt->array->len == 0) {
		path_tree_free(pt);
		return FALSE;
	}

	return TRUE;
}

gchar *path_tree_next(struct path_tree *pt)
{
	if (pt->cur >= pt->array->len)
		return NULL;

	return g_ptr_array_index(pt->array, pt->cur++);
}

void path_tree_free(struct path_tree *pt)
{
	g_ptr_array_free(pt->array, TRUE);
	trash_empty(&pt->trash);
	pt->array = NULL;
}

