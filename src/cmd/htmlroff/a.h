#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>

enum
{
	Unbsp = 0x00A0,
	Uprivate = 0xF000,
	Uempty,	/* \& */
	Uamp,	/* raw & */
	Ult,		/* raw < */
	Ugt,		/* raw > */
	Utick,	/* raw ' */
	Ubtick,	/* raw ` */
	Uminus,	/* raw - */
	Uspace,	/* raw space */
	Upl,		/* symbol + */
	Ueq,		/* symbol = */
	Umi,		/* symbol - */
	Uformatted,	/* start diverted output */
	Uunformatted,	/* end diverted output */

	UPI = 720,	/* units per inch */
	UPX = 10,	/* units per pixel */

	/* special input modes */
	CopyMode = 1<<1,
	ExpandMode = 1<<2,
	ArgMode = 1<<3,
	HtmlMode = 1<<4,

	MaxLine = 1024
};

Rune*	L(char*);

void		addesc(Rune, int (*)(void), int);
void		addraw(Rune*, void(*)(Rune*));
void		addreq(Rune*, void(*)(int, Rune**), int);
void		af(Rune*, Rune*);
void		as(Rune*, Rune*);
void		br(void);
void		closehtml(void);
Rune*	copyarg(void);
void		delraw(Rune*);
void		delreq(Rune*);
void		ds(Rune*, Rune*);
int		dv(int);
int		e_nop(void);
int		e_warn(void);
void*	emalloc(uint);
void*	erealloc(void*, uint);
Rune*	erunesmprint(char*, ...);
Rune*	erunestrdup(Rune*);
char*	esmprint(char*, ...);
char*	estrdup(char*);
int		eval(Rune*);
int		evalscale(Rune*, int);
Rune*	getname(void);
int		getnext(void);
Rune*	getds(Rune*);
Rune*	_getnr(Rune*);
int		getnr(Rune*);
int		getnrr(Rune*);
int		getrune(void);
Rune*	getqarg(void);
Rune*	getline(void);
void		hideihtml(void);
void		html(Rune*, Rune*);
void		htmlinit(void);
void		ihtml(Rune*, Rune*);
void		inputnotify(void(*)(void));
void		itrap(void);
void		itrapset(void);
int		linefmt(Fmt*);
void		nr(Rune*, int);
void		_nr(Rune*, Rune*);
void		out(Rune*);
void		(*outcb)(Rune);
void		outhtml(Rune*);
void		outrune(Rune);
void		outtrap(void);
int		popinput(void);
void		printds(int);
int		pushinputfile(Rune*);
void		pushinputstring(Rune*);
int		pushstdin(void);
int		queueinputfile(Rune*);
int		queuestdin(void);
void		r_nop(int, Rune**);
void		r_warn(int, Rune**);
Rune	*readline(int);
void		reitag(void);
void		renraw(Rune*, Rune*);
void		renreq(Rune*, Rune*);
void		run(void);
void		runinput(void);
int		runmacro(int, int, Rune**);
void		runmacro1(Rune*);
Rune*	rune2html(Rune);
void		setlinenumber(Rune*, int);
void		showihtml(void);
void		sp(int);
void		t1init(void);
void		t2init(void);
void		t3init(void);
void		t4init(void);
void		t5init(void);
void		t6init(void);
void		t7init(void);
void		t8init(void);
void		t9init(void);
void		t10init(void);
void		t11init(void);
void		t12init(void);
void		t13init(void);
void		t14init(void);
void		t15init(void);
void		t16init(void);
void		t17init(void);
void		t18init(void);
void		t19init(void);
void		t20init(void);
Rune	troff2rune(Rune*);
void		unfont(void);
void		ungetnext(Rune);
void		ungetrune(Rune);
void		unitag(void);
void		warn(char*, ...);

extern	int		backslash;
extern	int		bol;
extern	Biobuf	bout;
extern	int		broke;
extern	int		dot;
extern	int		inputmode;
extern	int		inrequest;
extern	int		tick;
extern	int		utf8;
extern	int		verbose;
extern	int		linepos;

#define	runemalloc(n)	(Rune*)emalloc((n)*sizeof(Rune))
#define	runerealloc(r, n)	(Rune*)erealloc(r, (n)*sizeof(Rune))
#define	runemove(a, b, n)	memmove(a, b, (n)*sizeof(Rune))

#pragma varargck type "L" void
