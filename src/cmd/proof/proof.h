#include <cursor.h>
#undef isspace
#define isspace proofisspace
#define	NPAGES	500
#define NFONT	33
#define NSIZE	40
#define MINSIZE 4
#define	DEFMAG	(10.0/11.0)	/* was (10.0/11.0), then 1 */
#define MAXVIEW 40

#define	ONES	~0

#define devname proof_devname
#define getc	proof_getc
#define ungetc	proof_ungetc

extern	char	devname[];
extern	double	mag;
extern	int	nview;
extern	int	hpos, vpos, curfont, cursize;
extern	int	DIV, res;
extern	int	Mode;

extern	Point	offset;		/* for small pages within big page */
extern	Point	xyoffset;	/* for explicit x,y move */
extern	Cursor	deadmouse;

extern	char	*libfont;

void	mapscreen(void);
void	clearscreen(void);
char	*getcmdstr(void);

void	readmapfile(char *);
void	dochar(Rune*);
void	bufput(void);
void	loadfontname(int, char *);
void	allfree(void);
void	readpage(void);
int	isspace(int);

extern	int	getc(void);
extern	int	getrune(void);
extern	void	ungetc(void);
extern	ulong	offsetc(void);
extern	ulong	seekc(ulong);
extern	char*	rdlinec(void);


#define	dprint	if (dbg) fprint

extern	int	dbg;
extern	int	resized;
