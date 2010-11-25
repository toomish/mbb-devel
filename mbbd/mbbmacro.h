/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_MACRO_H
#define MBB_MACRO_H

#define MBB_OBJ_LOCK volatile gint lock
#define MBB_OBJ_LOCK_INIT(ptr) ptr->lock = 0

#define MBB_OBJ_REF volatile gint nref
#define MBB_OBJ_REF_INIT(ptr) ptr->nref = 1

#define MBB_OBJ_DEFINE_LOCK(obj)				\
static inline void mbb_##obj##_lock(struct mbb_##obj *ptr)	\
{								\
	while (g_atomic_int_get(&ptr->lock));			\
	g_atomic_int_set(&ptr->lock, 1);			\
}								\
								\
static inline void mbb_##obj##_unlock(struct mbb_##obj *ptr)	\
{								\
	g_atomic_int_set(&ptr->lock, 0);			\
}

#define MBB_OBJ_DEFINE_REF(obj)					\
static inline struct mbb_##obj *mbb_##obj##_ref(struct mbb_##obj *ptr)	\
{								\
	g_atomic_int_inc(&ptr->nref);				\
	return ptr;						\
}								\
								\
static inline void mbb_##obj##_unref(struct mbb_##obj *ptr)	\
{								\
	if (g_atomic_int_dec_and_test(&ptr->nref))		\
		mbb_##obj##_free(ptr);				\
}

#endif
