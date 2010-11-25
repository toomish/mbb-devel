/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_PLOCK_H
#define MBB_PLOCK_H

#include <glib.h>

#define LOCK_NAME rwlock__

static GStaticRWLock LOCK_NAME = G_STATIC_RW_LOCK_INIT;

static inline void mbb_plock_reader_lock(void)
{
	g_static_rw_lock_reader_lock(&LOCK_NAME);
}

static inline void mbb_plock_reader_unlock(void)
{
	g_static_rw_lock_reader_unlock(&LOCK_NAME);
}

static inline void mbb_plock_writer_lock(void)
{
	g_static_rw_lock_writer_lock(&LOCK_NAME);
}

static inline void mbb_plock_writer_unlock(void)
{
	g_static_rw_lock_writer_unlock(&LOCK_NAME);
}

#endif
