/*
 * THIS FILE IS NOT IDENTICAL TO THE ORIGINAL
 * FROM THE BZIP2 DISTRIBUTION.
 *
 * It has been modified, mainly to break the library
 * into smaller pieces.
 *
 * Russ Cox
 * rsc@plan9.bell-labs.com
 * July 2000
 */

/*---------------------------------------------*/
/*--
  Place a 1 beside your platform, and 0 elsewhere.
  Attempts to autosniff this even if you don't.
--*/


/*--
  Generic 32-bit Unix.
  Also works on 64-bit Unix boxes.
--*/
#define BZ_UNIX      1

/*--
  Win32, as seen by Jacob Navia's excellent
  port of (Chris Fraser & David Hanson)'s excellent
  lcc compiler.
--*/
#define BZ_LCCWIN32  0

#if defined(_WIN32) && !defined(__CYGWIN__)
#undef  BZ_LCCWIN32
#define BZ_LCCWIN32 1
#undef  BZ_UNIX
#define BZ_UNIX 0
#endif

/*--
  Plan 9 from Bell Labs
--*/
#define BZ_PLAN9     0

#if defined(PLAN9)
#undef  BZ_UNIX
#define BZ_UNIX 0
#undef  BZ_PLAN9
#define BZ_PLAN9 1
#endif

#if BZ_UNIX
# include "unix.h"
#elif BZ_LCCWIN32
# include "lccwin32.h"
#elif BZ_PLAN9
# include "plan9.h"
#endif

#ifdef __GNUC__
#   define NORETURN __attribute__ ((noreturn))
#else
#   define NORETURN /**/
#endif

/*--
  Some more stuff for all platforms :-)
  This might have to get moved into the platform-specific
  header files if we encounter a machine with different sizes.
--*/

typedef char            Char;
typedef unsigned char   Bool;
typedef unsigned char   UChar;
typedef int             Int32;
typedef unsigned int    UInt32;
typedef short           Int16;
typedef unsigned short  UInt16;

#define True  ((Bool)1)
#define False ((Bool)0)

/*--
  IntNative is your platform's `native' int size.
  Only here to avoid probs with 64-bit platforms.
--*/
typedef int IntNative;
