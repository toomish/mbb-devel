/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef SLICER_H
#define SLICER_H

#include <glib.h>

struct slice;

typedef gpointer (*s_add_func_t)(gpointer data, gpointer elem);
typedef gpointer (*s_del_func_t)(gpointer data, gpointer elem);
typedef gpointer (*s_dup_func_t)(gpointer data);
typedef void (*s_free_func_t)(gpointer data);
typedef gpointer (*s_data_dup_func_t)(gpointer data, gpointer user_data);
typedef void (*s_data_free_func_t)(gpointer data, gpointer user_data);
typedef gboolean (*s_data_is_null_func_t)(gpointer data);
typedef void (*s_user_func_t)(gpointer data, struct slice *slice, gpointer arg);

struct slice {
	gpointer begin;
	gpointer end;
};

struct slice_ops {
	gint (*cmp)(gpointer, gpointer);
	gpointer (*dup)(gpointer);
	gpointer (*inc)(gpointer);
	gpointer (*dec)(gpointer);
	void (*free)(gpointer);
};

struct slice_data_ops {
	gpointer (*add)(gpointer data, gpointer elem);
	gpointer (*del)(gpointer data, gpointer elem);
	gpointer (*dup)(gpointer data, gpointer user_data);
	void (*free)(gpointer data, gpointer user_data);
	gboolean (*is_null)(gpointer data);
};

typedef struct slicer Slicer;
typedef struct slicer_iter SlicerIter;

struct slicer_iter {
	gpointer p;
};

Slicer *slicer_new(struct slice_ops *, struct slice_data_ops *);
Slicer *slicer_copy(Slicer *slicer);

void slicer_set_user_data(Slicer *slicer, gpointer user_data);
gpointer slicer_get_user_data(Slicer *slicer);

void slicer_iter_init(SlicerIter *iter, Slicer *slicer);
gboolean slicer_iter_next(SlicerIter *iter, gpointer *data, struct slice *slice);

struct slice_ops *slicer_get_ops(Slicer *slicer);

void slicer_init(Slicer *, gpointer data, gpointer begin, gpointer end);
gint slicer_count(Slicer *);
void slicer_destroy(Slicer *);

gpointer slicer_dup_dummy(gpointer data);

void slicer_insert(Slicer *, gpointer data, gpointer begin, gpointer end);
void slicer_delete(Slicer *, gpointer data, gpointer begin, gpointer end);

void slicer_foreach(Slicer *, s_user_func_t func, gpointer arg);
void slicer_foreach_all(Slicer *slicer, s_user_func_t func, gpointer arg);

void *slicer_find(Slicer *, gpointer point);
GQueue *slicer_search(Slicer *, gpointer begin, gpointer end);

gint slice_cmp(struct slice *a, struct slice *b, struct slice_ops *sops);
gint slice_cmp_point(struct slice *slice, gpointer point, struct slice_ops *sops);
void slice_cross(struct slice_ops *sops, struct slice *a, struct slice *b,
		 struct slice *slices[3]);

void slicer_glue_null(Slicer *slicer);

#endif
