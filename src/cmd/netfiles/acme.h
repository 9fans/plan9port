typedef struct Event Event;
typedef struct Win Win;

#define	EVENTSIZE	256
struct Event
{
	int	c1;
	int	c2;
	int	oq0;
	int	oq1;
	int	q0;
	int	q1;
	int	flag;
	int	nb;
	int	nr;
	char	text[EVENTSIZE*UTFmax+1];
	char	arg[EVENTSIZE*UTFmax+1];
	char	loc[EVENTSIZE*UTFmax+1];
};

struct Win
{
	int id;
	CFid *ctl;
	CFid *tag;
	CFid *body;
	CFid *addr;
	CFid *event;
	CFid *data;
	CFid *xdata;
	Channel *c;	/* chan(Event) */
	Win *next;
	Win *prev;
	
	/* events */
	int nbuf;
	char name[1024];
	char buf[1024];
	char *bufp;
	jmp_buf jmp;
	Event e2;
	Event e3;
	Event e4;
};

Win *newwin(void);

int eventfmt(Fmt*);
int pipewinto(Win *w, char *name, int, char *fmt, ...);
int pipetowin(Win *w, char *name, int, char *fmt, ...);
char *sysrun(int errto, char*, ...);
int winaddr(Win *w, char *fmt, ...);
int winctl(Win *w, char *fmt, ...);
int windel(Win *w, int sure);
int winfd(Win *w, char *name, int);
char *winmread(Win *w, char *file);
int winname(Win *w, char *fmt, ...);
int winprint(Win *w, char *name, char *fmt, ...);
int winread(Win *w, char *file, void *a, int n);
int winseek(Win *w, char *file, int n, int off);
int winreadaddr(Win *w, uint*);
int winreadevent(Win *w, Event *e);
int winwrite(Win *w, char *file, void *a, int n);
int winwriteevent(Win *w, Event *e);
int winopenfd(Win *w, char *name, int mode);
void windeleteall(void);
void winfree(Win *w);
void winclosefiles(Win *w);
Channel *wineventchan(Win *w);
char *winindex(void);
void mountacme(void);
char *wingetname(Win *w);

void *erealloc(void*, uint);
void *emalloc(uint);
char *estrdup(char*);
char *evsmprint(char*, va_list);

int twait(int);
void twaitinit(void);

extern Win *windows;
