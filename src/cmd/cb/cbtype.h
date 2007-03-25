#undef	_U
#undef	_L
#undef	_N
#undef	_S
#undef	_P
#undef	_C
#undef	_X
#undef	_O

#define	_U	01
#define	_L	02
#define	_N	04
#define	_S	010
#define	_P	020
#define	_C	040
#define	_X	0100
#define	_O	0200

extern	unsigned char	_cbtype_[];	/* in /usr/src/libc/gen/ctype_.c */

#undef isop
#undef	isalpha
#undef	isupper
#undef	islower
#undef	isdigit
#undef	isxdigit
#undef	isspace
#undef ispunct
#undef isalnum
#undef isprint
#undef iscntrl
#undef isascii
#undef toupper
#undef tolower
#undef toascii

#define isop(c)	((_cbtype_+1)[c]&_O)
#define	isalpha(c)	((_cbtype_+1)[c]&(_U|_L))
#define	isupper(c)	((_cbtype_+1)[c]&_U)
#define	islower(c)	((_cbtype_+1)[c]&_L)
#define	isdigit(c)	((_cbtype_+1)[c]&_N)
#define	isxdigit(c)	((_cbtype_+1)[c]&(_N|_X))
#define	isspace(c)	((_cbtype_+1)[c]&_S)
#define ispunct(c)	((_cbtype_+1)[c]&_P)
#define isalnum(c)	((_cbtype_+1)[c]&(_U|_L|_N))
#define isprint(c)	((_cbtype_+1)[c]&(_P|_U|_L|_N))
#define iscntrl(c)	((_cbtype_+1)[c]&_C)
#define isascii(c)	((unsigned)(c)<=0177)
#define toupper(c)	((c)-'a'+'A')
#define tolower(c)	((c)-'A'+'a')
#define toascii(c)	((c)&0177)
