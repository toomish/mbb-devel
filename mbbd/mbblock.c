/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbblock.h"

#include <glib.h>

static GStaticRWLock rwlock = G_STATIC_RW_LOCK_INIT;

void mbb_lock_reader_lock(void)
{
	g_static_rw_lock_reader_lock(&rwlock);
}

void mbb_lock_reader_unlock(void)
{
	g_static_rw_lock_reader_unlock(&rwlock);
}

void mbb_lock_writer_lock(void)
{
	g_static_rw_lock_writer_lock(&rwlock);
}

void mbb_lock_writer_unlock(void)
{
	g_static_rw_lock_writer_unlock(&rwlock);
}

