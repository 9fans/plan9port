/*
 * compiler directive on Plan 9
 */
#ifndef USED
#define USED(x) if(x);else
#endif

/*
 * easiest way to make sure these are defined
 */
#define uchar	_fmtuchar
#define ushort	_fmtushort
#define uint	_fmtuint
#define ulong	_fmtulong
#define vlong	_fmtvlong
#define uvlong	_fmtuvlong
typedef unsigned char		uchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef unsigned long long	uvlong;
typedef long long		vlong;

/*
 * nil cannot be ((void*)0) on ANSI C,
 * because it is used for function pointers
 */
#undef	nil
#define	nil	0

#undef	nelem
#define	nelem	((void*)0)
