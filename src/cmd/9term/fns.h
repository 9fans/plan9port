#undef isalnum
#define isalnum runeisalnum

void	keyboardsend(char*, int);
int	whide(Window*);
int	wunhide(int);
void	freescrtemps(void);
int	parsewctl(char**, Rectangle, Rectangle*, int*, int*, int*, int*, char**, char*, char*);
Window *new(Image*, int, int, int, char*, char*, char**);
void	riosetcursor(Cursor*, int);
int	min(int, int);
int	max(int, int);
Rune*	strrune(Rune*, Rune);
int	isalnum(Rune);
void	timerstop(Timer*);
void	timercancel(Timer*);
Timer*	timerstart(int);
void	error(char*);
void	killprocs(void);
int	shutdown(void*, char*);
void	iconinit(void);
void	*erealloc(void*, uint);
void *emalloc(uint);
char *estrdup(char*);
void	button3menu(void);
void	button2menu(Window*);
void	cvttorunes(char*, int, Rune*, int*, int*, int*);
/* was (byte*,int)	runetobyte(Rune*, int); */
char* runetobyte(Rune*, int, int*);
void	timerinit(void);
int	goodrect(Rectangle);
int	rawon(void);
void	winterrupt(Window*);
int	intrc(void);

#define	runemalloc(n)		malloc((n)*sizeof(Rune))
#define	runerealloc(a, n)	realloc(a, (n)*sizeof(Rune))
#define	runemove(a, b, n)	memmove(a, b, (n)*sizeof(Rune))

void rioputsnarf(void);
void riogetsnarf(void);
