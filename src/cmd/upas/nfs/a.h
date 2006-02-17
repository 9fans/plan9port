#include <u.h>
#include <libc.h>
#include <bio.h>
#include <auth.h>
#include <thread.h>
#include <fcall.h>
#include <plumb.h>
#include <9p.h>

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

#define esmprint smprint
#define emalloc(n) mallocz(n, 1)
#define erealloc realloc
#define estrdup strdup

enum
{
	NoEncoding,
	QuotedPrintable,
	Base64,
};

char*	decode(int, char*, int*);
char*	tcs(char*, char*);
char*	unrfc2047(char*);

extern Imap *imap;
