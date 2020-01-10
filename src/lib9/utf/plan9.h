/*
 * compiler directive on Plan 9
 */
#ifndef USED
#define USED(x) if(x);else
#endif

/*
 * easiest way to make sure these are defined
 */
#define uchar	_utfuchar
#define ushort	_utfushort
#define uint	_utfuint
#define ulong	_utfulong
typedef unsigned char		uchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;

/*
 * nil cannot be ((void*)0) on ANSI C,
 * because it is used for function pointers
 */
#undef	nil
#define	nil	0

#undef	nelem
#define	nelem(x)	(sizeof (x)/sizeof (x)[0])
