/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_LOCK_H
#define MBB_LOCK_H

void mbb_lock_reader_lock(void);
void mbb_lock_reader_unlock(void);

void mbb_lock_writer_lock(void);
void mbb_lock_writer_unlock(void);

#endif
