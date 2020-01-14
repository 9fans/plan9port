/*
#pragma	varargck	argpos	warning	2
#pragma	varargck	argpos	warningew	2
*/

void	warning(Mntdir*, char*, ...);
void	warningew(Window*, Mntdir*, char*, ...);

#define	fbufalloc()	emalloc(BUFSIZE)
#define	fbuffree(x)	free(x)

void	plumblook(Plumbmsg *m);
void	plumbshow(Plumbmsg*m);
void	acmeputsnarf(void);
void	acmegetsnarf(void);
int	tempfile(void);
void	scrlresize(void);
Font*	getfont(int, int, char*);
char*	getarg(Text*, int, int, Rune**, int*);
char*	getbytearg(Text*, int, int, char**);
void	new(Text*, Text*, Text*, int, int, Rune*, int);
void	undo(Text*, Text*, Text*, int, int, Rune*, int);
void	scrsleep(uint);
void	savemouse(Window*);
int	restoremouse(Window*);
void	clearmouse(void);
void	allwindows(void(*)(Window*, void*), void*);
uint loadfile(int, uint, int*, int(*)(void*, uint, Rune*, int), void*, DigestState*);
void	movetodel(Window*);

Window*	errorwin(Mntdir*, int);
Window*	errorwinforwin(Window*);
Runestr cleanrname(Runestr);
void	run(Window*, char*, Rune*, int, int, char*, char*, int);
void fsysclose(void);
void	setcurtext(Text*, int);
int	isfilec(Rune);
void	rxinit(void);
int rxnull(void);
Runestr	dirname(Text*, Rune*, int);
void	error(char*);
void	cvttorunes(char*, int, Rune*, int*, int*, int*);
void*	tmalloc(uint);
void	tfree(void);
void	killprocs(void);
void	killtasks(void);
int	runeeq(Rune*, uint, Rune*, uint);
int	ALEF_tid(void);
void	iconinit(void);
Timer*	timerstart(int);
void	timerstop(Timer*);
void	timercancel(Timer*);
void	timerinit(void);
void	cut(Text*, Text*, Text*, int, int, Rune*, int);
void	paste(Text*, Text*, Text*, int, int, Rune*, int);
void	get(Text*, Text*, Text*, int, int, Rune*, int);
void	put(Text*, Text*, Text*, int, int, Rune*, int);
void	putfile(File*, int, int, Rune*, int);
void	fontx(Text*, Text*, Text*, int, int, Rune*, int);
#undef isalnum
#define isalnum acmeisalnum
int	isalnum(Rune);
void	execute(Text*, uint, uint, int, Text*);
int	search(Text*, Rune*, uint);
void	look3(Text*, uint, uint, int);
void	editcmd(Text*, Rune*, uint);
uint	min(uint, uint);
uint	max(uint, uint);
Window*	lookfile(Rune*, int);
Window*	lookid(int, int);
char*	runetobyte(Rune*, int);
Rune*	bytetorune(char*, int*);
void	fsysinit(void);
Mntdir*	fsysmount(Rune*, int, Rune**, int);
void		fsysdelid(Mntdir*);
void		fsysincid(Mntdir*);
Xfid*		respond(Xfid*, Fcall*, char*);
int		rxcompile(Rune*);
int		rgetc(void*, uint);
int		tgetc(void*, uint);
int		isaddrc(int);
int		isregexc(int);
void *emalloc(uint);
void *erealloc(void*, uint);
char	*estrdup(char*);
Range		address(uint, Text*, Range, Range, void*, uint, uint, int (*)(void*, uint),  int*, uint*);
int		rxexecute(Text*, Rune*, uint, uint, Rangeset*);
int		rxbexecute(Text*, uint, Rangeset*);
Window*	makenewwindow(Text *t);
int	expand(Text*, uint, uint, Expand*);
Rune*	skipbl(Rune*, int, int*);
Rune*	findbl(Rune*, int, int*);
char*	edittext(Window*, int, Rune*, int);
void		flushwarnings(void);
void		startplumbing(void);
long	nlcount(Text*, long, long, long*);
long	nlcounttopos(Text*, long, long, long);
Rune*	parsetag(Window*, int, int*);

Runestr	runestr(Rune*, uint);
Range range(int, int);

#define	runemalloc(a)		(Rune*)emalloc((a)*sizeof(Rune))
#define	runerealloc(a, b)	(Rune*)erealloc((a), (b)*sizeof(Rune))
#define	runemove(a, b, c)	memmove((a), (b), (c)*sizeof(Rune))

int	ismtpt(char*);
