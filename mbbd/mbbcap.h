#ifndef MBB_CAP_H
#define MBB_CAP_H

#include <glib.h>

typedef guint32 mbb_cap_t;

#define mbb_cap_for_each(n, mask) for (n = 0; mask; n++, mask >>= 1) if (mask & 1)

#define MBB_CAP_ROOT_ID		0
#define MBB_CAP_WHEEL_ID	1
#define MBB_CAP_ADMIN_ID	4
#define MBB_CAP_LOG_ID		10
#define MBB_CAP_USER_ID		20
#define MBB_CAP_CONS_ID		26
#define MBB_CAP_DUMMY_ID	31

#define MBB_CAP_ROOT	 1
#define MBB_CAP_WHEEL	(1 << MBB_CAP_WHEEL_ID)
#define MBB_CAP_ADMIN	(1 << MBB_CAP_ADMIN_ID)
#define MBB_CAP_LOG	(1 << MBB_CAP_LOG_ID)
#define MBB_CAP_USER	(1 << MBB_CAP_USER_ID)
#define MBB_CAP_CONS	(1 << MBB_CAP_CONS_ID)
#define MBB_CAP_DUMMY	(1 << MBB_CAP_DUMMY_ID)

#define MBB_CAP_ALL_DUMMY ((guint32) -1)
#define MBB_CAP_ALL (MBB_CAP_ALL_DUMMY ^ MBB_CAP_DUMMY)

#define MBB_CAP_IS_ROOT(mask)	(mask & MBB_CAP_ROOT)
#define MBB_CAP_IS_WHEEL(mask)	((mask & MBB_CAP_WHEEL) || MBB_CAP_IS_ROOT(mask))
#define MBB_CAP_IS_LOG(mask)	(mask & MBB_CAP_LOG)
#define MBB_CAP_IS_USER(mask)	(mask & MBB_CAP_USER)
#define MBB_CAP_IS_DUMMY(mask)	(mask & MBB_CAP_DUMMY)

#define mbb_cap_check(m, mask) \
	((m == 0) ? MBB_CAP_IS_DUMMY(mask) : (MBB_CAP_IS_ROOT(m) || (m & mask)))

#endif
