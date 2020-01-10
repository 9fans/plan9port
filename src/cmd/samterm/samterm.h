#define	SAMTERM

#define	RUNESIZE	sizeof(Rune)
#define	MAXFILES	256
#define	READBUFSIZE 8192
#define	NL	5

enum{
	Up,
	Down
};

typedef struct Text	Text;
typedef struct Section	Section;
typedef struct Rasp	Rasp;
typedef struct Readbuf Readbuf;

struct Section
{
	long	nrunes;
	Rune	*text;		/* if null, we haven't got it */
	Section	*next;
};

struct Rasp
{
	long	nrunes;
	Section	*sect;
};

#define	Untagged	((ushort)65535)

struct Text
{
	Rasp	rasp;
	short	nwin;
	short	front;		/* input window */
	ushort	tag;
	char	lock;
	Flayer	l[NL];		/* screen storage */
};

struct Readbuf
{
	short	n;					/* # bytes in buf */
	uchar	data[READBUFSIZE];		/* data bytes */
};

enum Resource
{
	RHost,
	RKeyboard,
	RMouse,
	RPlumb,
	RResize,
	NRes
};

extern int	protodebug;

extern Text	**text;
extern uchar	**name;
extern ushort	*tag;
extern int	nname;
extern int	mname;
extern Cursor	bullseye;
extern Cursor	deadmouse;
extern Cursor	lockarrow;
extern Cursor	*cursor;
extern Flayer	*which;
extern Flayer	*work;
extern Text	cmd;
extern Rune	*scratch;
extern long	nscralloc;
extern char	hostlock;
extern char	hasunlocked;
extern long	snarflen;
extern Mousectl* mousectl;
extern Keyboardctl* keyboardctl;
extern Mouse*	mousep;
extern long	modified;
extern int	maxtab;
extern Readbuf	hostbuf[2];	/* double buffer; it's synchronous communication */
extern Readbuf	plumbbuf[2];	/* double buffer; it's synchronous communication */
extern Channel *plumbc;
extern Channel *hostc;
extern int	hversion;
extern int	plumbfd;
extern int	hostfd[2];
extern int	exiting;
extern int	autoindent;

#define gettext sam_gettext	/* stupid gcc built-in functions */
Rune	*gettext(Flayer*, long, ulong*);
void	*alloc(ulong n);

void	iconinit(void);
void	getscreen(int, char**);
void	initio(void);
void	setlock(void);
void	outcmd(void);
void	rinit(Rasp*);
void	startnewfile(int, Text*);
void	getmouse(void);
void	mouseunblock(void);
void	kbdblock(void);
void	extstart(void);
void	hoststart(void);
int	plumbstart(void);
int	button(int but);
int	load(char*, int);
int	waitforio(void);
int	rcvchar(void);
int	getch(void);
int	kbdchar(void);
int	qpeekc(void);
void	cut(Text*, int, int, int);
void	paste(Text*, int);
void	snarf(Text*, int);
int	center(Flayer*, long);
int	xmenuhit(int, Menu*);
void	buttons(int);
int	getr(Rectangle*);
void	current(Flayer*);
void	duplicate(Flayer*, Rectangle, Font*, int);
void	startfile(Text*);
void	panic(char*);
void	panic1(Display*, char*);
void	closeup(Flayer*);
void	Strgrow(Rune**, long*, int);
int	RESIZED(void);
void	resize(void);
void	rcv(void);
void	type(Flayer*, int);
void	menu2hit(void);
void	menu3hit(void);
void	scroll(Flayer*, int);
void	hcheck(int);
void	rclear(Rasp*);
int	whichmenu(int);
void	hcut(int, long, long);
void	horigin(int, long);
void	hgrow(int, long, long, int);
int	hdata(int, long, uchar*, int);
int	hdatarune(int, long, Rune*, int);
Rune	*rload(Rasp*, ulong, ulong, ulong*);
void	menuins(int, uchar*, Text*, int, int);
void	menudel(int);
Text	*sweeptext(int, int);
void	setpat(char*);
void	scrdraw(Flayer*, long tot);
int	rcontig(Rasp*, ulong, ulong, int);
int	rmissing(Rasp*, ulong, ulong);
void	rresize(Rasp *, long, long, long);
void	rdata(Rasp*, long, long, Rune*);
void	rclean(Rasp*);
void	scrorigin(Flayer*, int, long);
long	scrtotal(Flayer*);
void	flnewlyvisible(Flayer*);
char	*rcvstring(void);
void	Strcpy(Rune*, Rune*);
void	Strncpy(Rune*, Rune*, long);
void	flushtyping(int);
void	dumperrmsg(int, int, int, int);
int	screensize(int*,int*);
void	getmouse(void);

#include "mesg.h"

void	outTs(Tmesg, int);
void	outT0(Tmesg);
void	outTl(Tmesg, long);
void	outTslS(Tmesg, int, long, Rune*);
void	outTsll(Tmesg, int, long, long);
void	outTsl(Tmesg, int, long);
void	outTsv(Tmesg, int, vlong);
void	outTv(Tmesg, vlong);
void	outstart(Tmesg);
void	outcopy(int, uchar*);
void	outshort(int);
void	outlong(long);
void	outvlong(vlong);
void	outsend(void);
