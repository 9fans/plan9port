#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <frame.h>
#include <plumb.h>
#include <termios.h>

#define fatal	sysfatal

typedef struct Text	Text;
typedef struct Readbuf	Readbuf;

enum
{
	/* these are chosen to use malloc()'s properties well */
	HiWater	= 640000,	/* max size of history */
	LoWater	= 330000,	/* min size of history after max'ed */
};

/* various geometric paramters */
enum
{
	Scrollwid 	= 12,		/* width of scroll bar */
	Scrollgap 	= 4,		/* gap right of scroll bar */
	Maxtab		= 4,
};

enum
{
	Cut,
	Paste,
	Snarf,
	Send,
	Scroll,
	Plumb,
};


#define	SCROLLKEY	Kdown
#define	ESC		0x1B
#define CUT		0x18	/* ctrl-x */		
#define COPY		0x03	/* crtl-c */
#define PASTE		0x16	/* crtl-v */
#define BACKSCROLLKEY	Kup

#define	READBUFSIZE 8192

struct Text
{
	Frame		*f;		/* frame ofr terminal */
	Mouse		m;
	uint		nr;		/* num of runes in term */
	Rune		*r;		/* runes for term */
	uint		nraw;		/* num of runes in raw buffer */
	Rune		*raw;		/* raw buffer */
	uint		org;		/* first rune on the screen */
	uint		q0;		/* start of selection region */
	uint		q1;		/* end of selection region */
	uint		qh;		/* unix point */
	int		npart;		/* partial runes read from console */
	char		part[UTFmax];	
	int		nsnarf;		/* snarf buffer */
	Rune		*snarf;
};

struct Readbuf
{
	short	n;				/* # bytes in buf */
	uchar	data[READBUFSIZE];		/* data bytes */
};

void	mouse(void);
void	domenu2(int);
void	loop(void);
void	geom(void);
void	fill(void);
void	tcheck(void);
void	updatesel(void);
void	doreshape(void);
void	rcstart(int fd[2]);
void	runewrite(Rune*, int);
void	consread(void);
void	conswrite(char*, int);
int	bswidth(Rune c);
void	cut(void);
void	paste(Rune*, int, int);
void	snarfupdate(void);
void	snarf(void);
void	show(uint);
void	key(Rune);
void	setorigin(uint org, int exact);
uint	line2q(uint);
uint	backnl(uint, uint);
int	cansee(uint);
uint	backnl(uint, uint);
void	addraw(Rune*, int);
void	mselect(void);
void	doubleclick(uint *q0, uint *q1);
int	clickmatch(int cl, int cr, int dir, uint *q);
Rune	*strrune(Rune *s, Rune c);
int	consready(void);
Rectangle scrpos(Rectangle r, ulong p0, ulong p1, ulong tot);
void	scrdraw(void);
void	scroll(int);
void	hostproc(void *arg);
void	hoststart(void);
void	pdx(int, char*, int);
void	plumbstart(void);
void	plumb(uint, uint);
void	plumbclick(uint*, uint*);
int	getpts(int fd[], char *slave);

#define	runemalloc(n)		malloc((n)*sizeof(Rune))
#define	runerealloc(a, n)	realloc(a, (n)*sizeof(Rune))
#define	runemove(a, b, n)	memmove(a, b, (n)*sizeof(Rune))
