/*
 * The authors of this software are Rob Pike and Ken Thompson.
 *              Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */
/*
 * dofmt -- format to a buffer
 * the number of characters formatted is returned,
 * or -1 if there was an error.
 * if the buffer is ever filled, flush is called.
 * it should reset the buffer and return whether formatting should continue.
 */
#define uchar _fmtuchar
#define ushort _fmtushort
#define uint _fmtuint
#define ulong _fmtulong
#define vlong _fmtvlong
#define uvlong _fmtuvlong

#ifndef USED
#define USED(x) if(x);else
#endif

typedef unsigned char		uchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;

#ifndef NOVLONGS
typedef unsigned long long	uvlong;
typedef long long		vlong;
#endif

#undef nil
#define nil		0	/* cannot be ((void*)0) because used for function pointers */

typedef int (*Fmts)(Fmt*);

typedef struct Quoteinfo Quoteinfo;
struct Quoteinfo
{
	int	quoted;		/* if set, string must be quoted */
	int	nrunesin;	/* number of input runes that can be accepted */
	int	nbytesin;	/* number of input bytes that can be accepted */
	int	nrunesout;	/* number of runes that will be generated */
	int	nbytesout;	/* number of bytes that will be generated */
};

void	*__fmtflush(Fmt*, void*, int);
void	*__fmtdispatch(Fmt*, void*, int);
int	__floatfmt(Fmt*, double);
int	__fmtpad(Fmt*, int);
int	__rfmtpad(Fmt*, int);
int	__fmtFdFlush(Fmt*);

int	__efgfmt(Fmt*);
int	__charfmt(Fmt*);
int	__runefmt(Fmt*);
int	__runesfmt(Fmt*);
int	__countfmt(Fmt*);
int	__flagfmt(Fmt*);
int	__percentfmt(Fmt*);
int	__ifmt(Fmt*);
int	__strfmt(Fmt*);
int	__badfmt(Fmt*);
int	__fmtcpy(Fmt*, const void*, int, int);
int	__fmtrcpy(Fmt*, const void*, int n);
int	__errfmt(Fmt *f);

double	__fmtpow10(int);

void	__fmtlock(void);
void	__fmtunlock(void);

#define FMTCHAR(f, t, s, c)\
	do{\
	if(t + 1 > (char*)s){\
		t = __fmtflush(f, t, 1);\
		if(t != nil)\
			s = f->stop;\
		else\
			return -1;\
	}\
	*t++ = c;\
	}while(0)

#define FMTRCHAR(f, t, s, c)\
	do{\
	if(t + 1 > (Rune*)s){\
		t = __fmtflush(f, t, sizeof(Rune));\
		if(t != nil)\
			s = f->stop;\
		else\
			return -1;\
	}\
	*t++ = c;\
	}while(0)

#define FMTRUNE(f, t, s, r)\
	do{\
	Rune _rune;\
	int _runelen;\
	if(t + UTFmax > (char*)s && t + (_runelen = runelen(r)) > (char*)s){\
		t = __fmtflush(f, t, _runelen);\
		if(t != nil)\
			s = f->stop;\
		else\
			return -1;\
	}\
	if(r < Runeself)\
		*t++ = r;\
	else{\
		_rune = r;\
		t += runetochar(t, &_rune);\
	}\
	}while(0)
