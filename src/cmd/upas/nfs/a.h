#include <u.h>
#include <libc.h>
#include <bio.h>
#include <auth.h>
#include <thread.h>
#include <fcall.h>
#include <plumb.h>
#include <9p.h>
#include <ctype.h>

enum
{
	STACK = 8192
};

#include "box.h"
#include "sx.h"
#include "imap.h"
#include "fs.h"

void		mailthreadinit(void);
void		mailthread(void (*fn)(void*), void*);

void		warn(char*, ...);

enum
{
	NoEncoding,
	QuotedPrintable,
	QuotedPrintableU,
	Base64
};

char*	decode(int, char*, int*);
char*	tcs(char*, char*);
char*	unrfc2047(char*);

extern Imap *imap;

#undef isnumber
#define isnumber	upas_isnumber

#define esmprint smprint
#define emalloc(n) mallocz(n, 1)
#define erealloc realloc
#define estrdup strdup

#pragma varargck type "$" Sx*
#pragma varargck type "Z" char*
