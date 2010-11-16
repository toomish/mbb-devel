#ifndef MACROS_H
#define MACROS_H

#define SWAP(a, b) { \
	__typeof__(a) __swap_tmp; \
	__swap_tmp = a; \
	a = b; \
	b = __swap_tmp; \
}

#define NELEM(p) (sizeof(p) / sizeof(p[0]))
#define STRSIZE(str) (sizeof(str) - 1)

#define exec_if(expr, cond) if (cond) expr

#define __init __attribute__ ((constructor))
#define __sentinel(n) __attribute__ ((sentinel(n)))

#ifndef __nonnull
#define __nonnull(arg) __attribute__ ((nonnull(arg)))
#endif

/* static inline void final_before__(void) __attribute__ ((always_inline)); */
static inline void __attribute__ ((always_inline)) final_before__(void) {}

#define on_final __extension__ inline void final_before__(void)

#define final for(final_before__();; __extension__ ({ return; }))

#endif
