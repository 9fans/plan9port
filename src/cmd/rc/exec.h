/*
 * Definitions used in the interpreter
 */
extern void Xappend(void), Xasync(void), Xbackq(void), Xbang(void), Xclose(void);
extern void Xconc(void), Xcount(void), Xdelfn(void), Xdol(void), Xqw(void), Xdup(void);
extern void Xexit(void), Xfalse(void), Xfn(void), Xfor(void), Xglob(void);
extern void Xjump(void), Xmark(void), Xmatch(void), Xpipe(void), Xread(void), Xhere(void), Xhereq(void);
extern void Xrdwr(void), Xsrcline(void);
extern void Xunredir(void), Xstar(void), Xreturn(void), Xsubshell(void);
extern void Xtrue(void), Xword(void), Xwrite(void), Xpipefd(void), Xcase(void);
extern void Xlocal(void), Xunlocal(void), Xassign(void), Xsimple(void), Xpopm(void), Xpush(void);
extern void Xrdcmds(void), Xwastrue(void), Xif(void), Xifnot(void), Xpipewait(void);
extern void Xpopredir(void), Xsub(void), Xeflag(void), Xsettrue(void);
extern void Xerror1(char*);
extern void Xerror2(char*,char*);
extern void Xerror3(char*,char*,char*);

/*
 * word lists are in correct order,
 * i.e. word0->word1->word2->word3->0
 */
struct word{
	char *word;
	word *next;
};
struct list{
	word *words;
	list *next;
};
word *newword(char *, word *), *copywords(word *, word *);

struct redir{
	int type;			/* what to do */
	int from, to;			/* what to do it to */
	redir *next;			/* what else to do (reverse order) */
};

/*
 * redir types
 */
#define	ROPEN	1			/* dup2(from, to); close(from); */
#define	RDUP	2			/* dup2(from, to); */
#define	RCLOSE	3			/* close(from); */
void	shuffleredir(void);

struct thread{
	code *code;			/* code for this thread */
	int pc;				/* code[pc] is the next instruction */
	int line;			/* source code line for Xsrcline */
	list *argv;			/* argument stack */
	redir *redir;			/* redirection stack */
	redir *startredir;		/* redir inheritance point */
	var *local;			/* list of local variables */
	lexer *lex;			/* lexer for Xrdcmds */
	int iflag;			/* interactive? */
	int pid;			/* process for Xpipewait to wait for */
	char *status;			/* status for Xpipewait */
	thread *ret;			/* who continues when this finishes */
};
extern thread *runq;
void turfstack(var*);

extern int mypid;
extern int ntrap;			/* number of outstanding traps */
extern int trap[NSIG];			/* number of outstanding traps per type */

code *codecopy(code*);
extern code *codebuf;			/* compiler output */
extern int ifnot;

struct builtin{
	char *name;
	void (*fnc)(void);
};
extern void (*builtinfunc(char *name))(void);

void execcd(void), execwhatis(void), execeval(void), execexec(void);
int execforkexec(void);
void execexit(void), execshift(void);
void execwait(void), execumask(void), execdot(void), execflag(void);
void execfunc(var*), execcmds(io*, char*, var*, redir*);
void startfunc(var*, word*, var*, redir*);

char *srcfile(thread*);
char *getstatus(void);
