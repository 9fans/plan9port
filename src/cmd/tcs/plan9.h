typedef unsigned short Rune;		/* 16 bits */
typedef unsigned char uchar;
#define		Runeerror	0x80	/* decoding error in UTF */
#define		Runeself	0x80	/* rune and UTF sequences are the same (<) */
#define		UTFmax		6	/* maximum bytes per rune */

/*
	plan 9 argument parsing
*/
#define	ARGBEGIN	for((argv0? 0: (argv0= *argv)),argv++,argc--;\
			    argv[0] && argv[0][0]=='-' && argv[0][1];\
			    argc--, argv++) {\
				char *_args, *_argt, _argc;\
				_args = &argv[0][1];\
				if(_args[0]=='-' && _args[1]==0){\
					argc--; argv++; break;\
				}\
				_argc=0;while(*_args) switch(_argc= *_args++)
#define	ARGEND		}
#define	ARGF()		(_argt=_args, _args="",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): 0))
#define	ARGC()		_argc
extern char *argv0;
