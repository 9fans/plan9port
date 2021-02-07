// See ../src/lib9/LICENSE

#ifndef _U_H_
#define _U_H_ 1
#if defined(__cplusplus)
extern "C" {
#endif

#define HAS_SYS_TERMIOS 1

#define __BSD_VISIBLE 1 /* FreeBSD 5.x */
#if defined(__sun__)
#	define __EXTENSIONS__ 1 /* SunOS */
#	if defined(__SunOS5_6__) || defined(__SunOS5_7__) || defined(__SunOS5_8__) || defined(__SunOS5_9__) || defined(__SunOS5_10__)
		/* NOT USING #define __MAKECONTEXT_V2_SOURCE 1 / * SunOS */
#	else
		/* What's left? */
#		define __MAKECONTEXT_V2_SOURCE 1
#	endif
#endif
#define _BSD_SOURCE 1
#define _NETBSD_SOURCE 1	/* NetBSD */
#define _SVID_SOURCE 1
#define _DEFAULT_SOURCE 1
#if !defined(__APPLE__) && !defined(__OpenBSD__) && !defined(__AIX__)
#	define _XOPEN_SOURCE 1000
#	define _XOPEN_SOURCE_EXTENDED 1
#endif
#if defined(__FreeBSD__)
#	include <sys/cdefs.h>
	/* for strtoll */
#	undef __ISO_C_VISIBLE
#	define __ISO_C_VISIBLE 1999
#	undef __LONG_LONG_SUPPORTED
#	define __LONG_LONG_SUPPORTED
#endif
#if defined(__AIX__)
#	define _ALL_SOURCE
#	undef HAS_SYS_TERMIOS
#endif
#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS 64

#include <inttypes.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <assert.h>
#include <setjmp.h>
#include <stddef.h>
#include <math.h>
#include <ctype.h>	/* for tolower */

/*
 * OS-specific crap
 */
#define _NEEDUCHAR 1
#define _NEEDUSHORT 1
#define _NEEDUINT 1
#define _NEEDULONG 1

typedef long p9jmp_buf[sizeof(sigjmp_buf)/sizeof(long)];

#if defined(__linux__)
#	include <sys/types.h>
#	include <pthread.h>
#	if defined(__USE_MISC)
#		undef _NEEDUSHORT
#		undef _NEEDUINT
#		undef _NEEDULONG
#	endif
#elif defined(__sun__)
#	include <sys/types.h>
#	include <pthread.h>
#	undef _NEEDUSHORT
#	undef _NEEDUINT
#	undef _NEEDULONG
#	define nil 0	/* no cast to void* */
#elif defined(__FreeBSD__)
#	include <sys/types.h>
#	include <osreldate.h>
#	include <pthread.h>
#	if !defined(_POSIX_SOURCE)
#		undef _NEEDUSHORT
#		undef _NEEDUINT
#	endif
#elif defined(__APPLE__)
#	include <sys/types.h>
#	include <pthread.h>
#	if __GNUC__ < 4
#		undef _NEEDUSHORT
#		undef _NEEDUINT
#	endif
#	undef _ANSI_SOURCE
#	undef _POSIX_C_SOURCE
#	undef _XOPEN_SOURCE
#	if !defined(NSIG)
#		define NSIG 32
#	endif
#	define _NEEDLL 1
#elif defined(__NetBSD__)
#	include <sched.h>
#	include <sys/types.h>
#	include <pthread.h>
#	undef _NEEDUSHORT
#	undef _NEEDUINT
#	undef _NEEDULONG
#elif defined(__OpenBSD__)
#	include <sys/types.h>
#	include <pthread.h>
#	undef _NEEDUSHORT
#	undef _NEEDUINT
#	undef _NEEDULONG
#else
	/* No idea what system this is -- try some defaults */
#	include <pthread.h>
#endif

#ifndef O_DIRECT
#define O_DIRECT 0
#endif

typedef signed char schar;

#ifdef _NEEDUCHAR
	typedef unsigned char uchar;
#endif
#ifdef _NEEDUSHORT
	typedef unsigned short ushort;
#endif
#ifdef _NEEDUINT
	typedef unsigned int uint;
#endif
#ifdef _NEEDULONG
	typedef unsigned long ulong;
#endif
typedef unsigned long long uvlong;
typedef long long vlong;

typedef uvlong u64int;
typedef vlong s64int;
typedef uint8_t u8int;
typedef int8_t s8int;
typedef uint16_t u16int;
typedef int16_t s16int;
typedef uintptr_t uintptr;
typedef intptr_t intptr;
typedef uint u32int;
typedef int s32int;

typedef u32int uint32;
typedef s32int int32;
typedef u16int uint16;
typedef s16int int16;
typedef u64int uint64;
typedef s64int int64;
typedef u8int uint8;
typedef s8int int8;

#undef _NEEDUCHAR
#undef _NEEDUSHORT
#undef _NEEDUINT
#undef _NEEDULONG

/*
 * Funny-named symbols to tip off 9l to autolink.
 */
#define AUTOLIB(x)	static int __p9l_autolib_ ## x = 1;
#define AUTOFRAMEWORK(x) static int __p9l_autoframework_ ## x = 1;

/*
 * Gcc is too smart for its own good.
 */
#if defined(__GNUC__)
#	undef strcmp	/* causes way too many warnings */
#	if __GNUC__ >= 4 || (__GNUC__==3 && !defined(__APPLE_CC__))
#		undef AUTOLIB
#		define AUTOLIB(x) int __p9l_autolib_ ## x __attribute__ ((weak));
#		undef AUTOFRAMEWORK
#		define AUTOFRAMEWORK(x) int __p9l_autoframework_ ## x __attribute__ ((weak));
#	else
#		undef AUTOLIB
#		define AUTOLIB(x) static int __p9l_autolib_ ## x __attribute__ ((unused));
#		undef AUTOFRAMEWORK
#		define AUTOFRAMEWORK(x) static int __p9l_autoframework_ ## x __attribute__ ((unused));
#	endif
#endif

#if defined(__cplusplus)
}
#endif
#endif
