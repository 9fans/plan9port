/*
 * Definitions used in the interpreter
 */
extern void Xappend(void), Xasync(void), Xbackq(void), Xbang(void), Xclose(void);
extern void Xconc(void), Xcount(void), Xdelfn(void), Xdol(void), Xqdol(void), Xdup(void);
extern void Xexit(void), Xfalse(void), Xfn(void), Xfor(void), Xglob(void);
extern void Xjump(void), Xmark(void), Xmatch(void), Xpipe(void), Xread(void);
extern void Xrdwr(void);
extern void Xrdfn(void), Xunredir(void), Xstar(void), Xreturn(void), Xsubshell(void);
extern void Xtrue(void), Xword(void), Xwrite(void), Xpipefd(void), Xcase(void);
extern void Xlocal(void), Xunlocal(void), Xassign(void), Xsimple(void), Xpopm(void);
extern void Xrdcmds(void), Xwastrue(void), Xif(void), Xifnot(void), Xpipewait(void);
extern void Xdelhere(void), Xpopredir(void), Xsub(void), Xeflag(void), Xsettrue(void);
extern void Xerror(char*);
extern void Xerror1(char*);
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
	char type;			/* what to do */
	short from, to;			/* what to do it to */
	struct redir *next;		/* what else to do (reverse order) */
};
#define	NSTATUS	ERRMAX			/* length of status (from plan 9) */
/*
 * redir types
 */
#define	ROPEN	1			/* dup2(from, to); close(from); */
#define	RDUP	2			/* dup2(from, to); */
#define	RCLOSE	3			/* close(from); */
struct thread{
	union code *code;		/* code for this thread */
	int pc;				/* code[pc] is the next instruction */
	struct list *argv;		/* argument stack */
	struct redir *redir;		/* redirection stack */
	struct redir *startredir;	/* redir inheritance point */
	struct var *local;		/* list of local variables */
	char *cmdfile;			/* file name in Xrdcmd */
	struct io *cmdfd;		/* file descriptor for Xrdcmd */
	int iflast;			/* static `if not' checking */
	int eof;			/* is cmdfd at eof? */
	int iflag;			/* interactive? */
	int lineno;			/* linenumber */
	int pid;			/* process for Xpipewait to wait for */
	char status[NSTATUS];		/* status for Xpipewait */
	tree *treenodes;		/* tree nodes created by this process */
	thread *ret;		/* who continues when this finishes */
};
thread *runq;
code *codecopy(code*);
code *codebuf;				/* compiler output */
int ntrap;				/* number of outstanding traps */
int trap[NSIG];				/* number of outstanding traps per type */
struct builtin{
	char *name;
	void (*fnc)(void);
};
extern struct builtin Builtin[];
int eflagok;			/* kludge flag so that -e doesn't exit in startup */
extern int havefork;

void execcd(void), execwhatis(void), execeval(void), execexec(void);
int execforkexec(void);
void execexit(void), execshift(void);
void execwait(void), execumask(void), execdot(void), execflag(void);
void execfunc(var*), execcmds(io *);
