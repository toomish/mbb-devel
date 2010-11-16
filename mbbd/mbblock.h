#ifndef MBB_LOCK_H
#define MBB_LOCK_H

void mbb_lock_reader_lock(void);
void mbb_lock_reader_unlock(void);

void mbb_lock_writer_lock(void);
void mbb_lock_writer_unlock(void);

#endif
