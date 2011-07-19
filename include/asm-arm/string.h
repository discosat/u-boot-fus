#ifndef __ASM_ARM_STRING_H
#define __ASM_ARM_STRING_H

#include <config.h>			  /* CONFIG_S3C64XX */

/*
 * We don't do inline string functions, since the
 * optimised inline asm versions are not small.
 */

#ifdef CONFIG_S3C64XX

/* We have added some optimized functions for ARM v6 (ARM11, etc) */
#undef __HAVE_ARCH_STRRCHR
#undef __HAVE_ARCH_STRCHR
#define __HAVE_ARCH_MEMCPY
#define __HAVE_ARCH_MEMMOVE
#define __HAVE_ARCH_MEMSET
//#undef __HAVE_ARCH_MEMCPY
//#undef __HAVE_ARCH_MEMMOVE
//#undef __HAVE_ARCH_MEMSET
#undef __HAVE_ARCH_MEMCHR
//#define __HAVE_ARCH_MEMZERO
#undef __HAVE_ARCH_MEMZERO

#define memzero(dest, count) memset(dest, 0, count)

/* This one is new: fill count words starting at dest with data */
extern unsigned *memset32(const unsigned *dest, unsigned data, unsigned count);

#else

#undef __HAVE_ARCH_STRRCHR
#undef __HAVE_ARCH_STRCHR
#undef __HAVE_ARCH_MEMCPY
#undef __HAVE_ARCH_MEMMOVE
#undef __HAVE_ARCH_MEMCHR
#undef __HAVE_ARCH_MEMZERO

#if 0
extern void __memzero(void *ptr, __kernel_size_t n);

#define memset(p,v,n)							\
	({								\
		if ((n) != 0) {						\
			if (__builtin_constant_p((v)) && (v) == 0)	\
				__memzero((p),(n));			\
			else						\
				memset((p),(v),(n));			\
		}							\
		(p);							\
	})

#define memzero(p,n) ({ if ((n) != 0) __memzero((p),(n)); (p); })
#else
extern void memzero(void *ptr, __kernel_size_t n);
#endif

#endif /* CONFIG_S3C64XX */

extern char * strrchr(const char * s, int c);

extern char * strchr(const char * s, int c);

extern void * memcpy(void *, const void *, __kernel_size_t);

extern void * memmove(void *, const void *, __kernel_size_t);

extern void * memchr(const void *, int, __kernel_size_t);

extern void * memset(void *, int, __kernel_size_t);

#endif
