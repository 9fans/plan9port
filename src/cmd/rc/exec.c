#include "rc.h"
#include "getflags.h"
#include "exec.h"
#include "io.h"
#include "fns.h"

char *argv0="rc";
io *err;
int mypid;
thread *runq;
char **environ;

/*
 * Start executing the given code at the given pc with the given redirection
 */
void
start(code *c, int pc, var *local, redir *redir)
{
	thread *p = new(thread);
	p->code = codecopy(c);
	p->line = 0;
	p->pc = pc;
	p->argv = 0;
	p->redir = p->startredir = redir;
	p->lex = 0;
	p->local = local;
	p->iflag = 0;
	p->pid = 0;
	p->status = 0;
	p->ret = runq;
	runq = p;
}

void
startfunc(var *func, word *starval, var *local, redir *redir)
{
	start(func->fn, func->pc, local, redir);
	runq->local = newvar("*", runq->local);
	runq->local->val = starval;
	runq->local->changed = 1;
}

static void
popthread(void)
{
	thread *p = runq;
	while(p->argv) poplist();
	while(p->local && (p->ret==0 || p->local!=p->ret->local))
		Xunlocal();
	runq = p->ret;
	if(p->lex) freelexer(p->lex);
	codefree(p->code);
	free(p->status);
	free(p);
}

word*
Newword(char *s, word *next)
{
	word *p=new(word);
	p->word = s;
	p->next = next;
	return p;
}
word*
newword(char *s, word *next)
{
	return Newword(estrdup(s), next);
}
word*
Pushword(char *s)
{
	word *p;
	if(s==0)
		panic("null pushword", 0);
	if(runq->argv==0)
		panic("pushword but no argv!", 0);
	p = Newword(s, runq->argv->words);
	runq->argv->words = p;
	return p;
}
word*
pushword(char *s)
{
	return Pushword(estrdup(s));
}
char*
Freeword(word *p)
{
	char *s = p->word;
	free(p);
	return s;
}
void
freewords(word *w)
{
	word *p;
	while((p = w)!=0){
		w = w->next;
		free(Freeword(p));
	}
}
char*
Popword(void)
{
	word *p;
	if(runq->argv==0)
		panic("popword but no argv!", 0);
	p = runq->argv->words;
	if(p==0)
		panic("popword but no word!", 0);
	runq->argv->words = p->next;
	return Freeword(p);
}
void
popword(void)
{
	free(Popword());
}

void
pushlist(void)
{
	list *p = new(list);
	p->words = 0;
	p->next = runq->argv;
	runq->argv = p;
}
word*
Poplist(void)
{
	word *w;
	list *p = runq->argv;
	if(p==0)
		panic("poplist but no argv", 0);
	w = p->words;
	runq->argv = p->next;
	free(p);
	return w;
}
void
poplist(void)
{
	freewords(Poplist());
}

int
count(word *w)
{
	int n;
	for(n = 0;w;n++) w = w->next;
	return n;
}

void
pushredir(int type, int from, int to)
{
	redir *rp = new(redir);
	rp->type = type;
	rp->from = from;
	rp->to = to;
	rp->next = runq->redir;
	runq->redir = rp;
}

static void
dontclose(int fd)
{
	redir *rp;

	if(fd<0)
		return;
	for(rp = runq->redir; rp != runq->startredir; rp = rp->next){
		if(rp->type == RCLOSE && rp->from == fd){
			rp->type = 0;
			break;
		}
	}
}

/*
 * we are about to start a new thread that should exit on
 * return, so the current stack is not needed anymore.
 * free all the threads and lexers, but preserve the
 * redirections and anything referenced by local.
 */
void
turfstack(var *local)
{
	while(local){
		thread *p;

		for(p = runq; p && p->local == local; p = p->ret)
			p->local = local->next;
		local = local->next;
	}
	while(runq) {
		if(runq->lex) dontclose(runq->lex->input->fd);
		popthread();
	}
}

void
shuffleredir(void)
{
	redir **rr, *rp;

	rp = runq->redir;
	if(rp==0)
		return;
	runq->redir = rp->next;
	rp->next = runq->startredir;
	for(rr = &runq->redir; *rr != rp->next; rr = &((*rr)->next))
		;
	*rr = rp;
}

/*
 * get command line flags, initialize keywords & traps.
 * get values from environment.
 * set $pid, $cflag, $*
 * fabricate bootstrap code and start it (*=(argv);. -bq /usr/lib/rcmain $*)
 * start interpreting code
 */
void
main(int argc, char *argv[], char *envp[])
{
	code bootstrap[20];
	char num[12];
	char *rcmain=Rcmain;

	if(snprint(Rcmain, sizeof(Rcmain), "%s/rcmain", unsharp("#9")) <= 0)
		return;

	environ = envp;

	int i;
	argv0 = argv[0];
	argc = getflags(argc, argv, "srdiIlxebpvVc:1m:1[command]", 1);
	if(argc==-1)
		usage("[file [arg ...]]");
	if(argv[0][0]=='-')
		flag['l'] = flagset;
	if(flag['I'])
		flag['i'] = 0;
	else if(flag['i']==0 && argc==1 && Isatty(0)) flag['i'] = flagset;
	if(flag['m']) rcmain = flag['m'][0];
	err = openiofd(2);
	kinit();
	Trapinit();
	Vinit();
	inttoascii(num, mypid = getpid());
	setvar("pid", newword(num, (word *)0));
	setvar("cflag", flag['c']?newword(flag['c'][0], (word *)0)
				:(word *)0);
	setvar("rcname", newword(argv[0], (word *)0));
	bootstrap[0].i = 1;
	bootstrap[1].s="*bootstrap*";
	bootstrap[2].f = Xmark;
	bootstrap[3].f = Xword;
	bootstrap[4].s="*";
	bootstrap[5].f = Xassign;
	bootstrap[6].f = Xmark;
	bootstrap[7].f = Xmark;
	bootstrap[8].f = Xword;
	bootstrap[9].s="*";
	bootstrap[10].f = Xdol;
	bootstrap[11].f = Xword;
	bootstrap[12].s = rcmain;
	bootstrap[13].f = Xword;
	bootstrap[14].s="-bq";
	bootstrap[15].f = Xword;
	bootstrap[16].s=".";
	bootstrap[17].f = Xsimple;
	bootstrap[18].f = Xexit;
	bootstrap[19].f = 0;
	start(bootstrap, 2, (var*)0, (redir*)0);

	/* prime bootstrap argv */
	pushlist();
	for(i = argc-1;i!=0;--i) pushword(argv[i]);

	for(;;){
		if(flag['r'])
			pfnc(err, runq);
		(*runq->code[runq->pc++].f)();
		if(ntrap)
			dotrap();
	}
}
/*
 * Opcode routines
 * Arguments on stack (...)
 * Arguments in line [...]
 * Code in line with jump around {...}
 *
 * Xappend(file)[fd]			open file to append
 * Xassign(name, val)			assign val to name
 * Xasync{... Xexit}			make thread for {}, no wait
 * Xbackq(split){... Xreturn}		make thread for {}, push stdout
 * Xbang				complement condition
 * Xcase(pat, value){...}		exec code on match, leave (value) on
 * 					stack
 * Xclose[i]				close file descriptor
 * Xconc(left, right)			concatenate, push results
 * Xcount(name)				push var count
 * Xdelfn(name)				delete function definition
 * Xdol(name)				get variable value
 * Xdup[i j]				dup file descriptor
 * Xexit				rc exits with status
 * Xfalse{...}				execute {} if false
 * Xfn(name){... Xreturn}		define function
 * Xfor(var, list){... Xreturn}		for loop
 * Xglob(list)				glob a list of words inplace
 * Xjump[addr]				goto
 * Xlocal(name, val)			create local variable, assign value
 * Xmark				mark stack
 * Xmatch(pat, str)			match pattern, set status
 * Xpipe[i j]{... Xreturn}{... Xreturn}	construct a pipe between 2 new threads,
 * 					wait for both
 * Xpipefd[type]{... Xreturn}		connect {} to pipe (input or output,
 * 					depending on type), push /dev/fd/??
 * Xpopm(value)				pop value from stack
 * Xpush(words)				push words down a list
 * Xqw(words)				quote words inplace
 * Xrdwr(file)[fd]			open file for reading and writing
 * Xread(file)[fd]			open file to read
 * Xreturn				kill thread
 * Xsimple(args)			run command and wait
 * Xsrcline[line]			set current source line number
 * Xsubshell{... Xexit}			execute {} in a subshell and wait
 * Xtrue{...}				execute {} if true
 * Xunlocal				delete local variable
 * Xword[string]			push string
 * Xwrite(file)[fd]			open file to write
 */

void
Xappend(void)
{
	char *file;
	int fd;

	switch(count(runq->argv->words)){
	default:
		Xerror1(">> requires singleton");
		return;
	case 0:
		Xerror1(">> requires file");
		return;
	case 1:
		break;
	}
	file = runq->argv->words->word;
	if((fd = Open(file, 1))<0 && (fd = Creat(file))<0){
		Xerror3(">> can't open", file, Errstr());
		return;
	}
	Seek(fd, 0L, 2);
	pushredir(ROPEN, fd, runq->code[runq->pc++].i);
	poplist();
}

void
Xsettrue(void)
{
	setstatus("");
}

void
Xbang(void)
{
	setstatus(truestatus()?"false":"");
}

void
Xclose(void)
{
	pushredir(RCLOSE, runq->code[runq->pc++].i, 0);
}

void
Xdup(void)
{
	pushredir(RDUP, runq->code[runq->pc].i, runq->code[runq->pc+1].i);
	runq->pc+=2;
}

void
Xeflag(void)
{
	if(!truestatus()) Xexit();
}

void
Xexit(void)
{
	static int beenhere = 0;

	if(getpid()==mypid && !beenhere){
		var *trapreq = vlook("sigexit");
		word *starval = vlook("*")->val;
		if(trapreq->fn){
			beenhere = 1;
			--runq->pc;
			startfunc(trapreq, copywords(starval, (word*)0), (var*)0, (redir*)0);
			return;
		}
	}
	Exit();
}

void
Xfalse(void)
{
	if(truestatus()) runq->pc = runq->code[runq->pc].i;
	else runq->pc++;
}
int ifnot;		/* dynamic if not flag */

void
Xifnot(void)
{
	if(ifnot)
		runq->pc++;
	else
		runq->pc = runq->code[runq->pc].i;
}

void
Xjump(void)
{
	runq->pc = runq->code[runq->pc].i;
}

void
Xmark(void)
{
	pushlist();
}

void
Xpopm(void)
{
	poplist();
}

void
Xpush(void)
{
	word *t, *h = Poplist();
	for(t = h; t->next; t = t->next)
		;
	t->next = runq->argv->words;
	runq->argv->words = h;
}

static int
herefile(char *tmp)
{
	char *s = tmp+strlen(tmp)-1;
	static int ser;
	int fd, i;

	i = ser++;
	while(*s == 'Y'){
		*s-- = (i%26) + 'A';
		i = i/26;
	}
	i = getpid();
	while(*s == 'X'){
		*s-- = (i%10) + '0';
		i = i/10;
	}
	s++;
	for(i='a'; i<'z'; i++){
		if(access(tmp, 0)!=0 && (fd = Creat(tmp))>=0)
			return fd;
		*s = i;
	}
	return -1;
}

void
Xhere(void)
{
	char file[]="/tmp/hereXXXXXXXXXXYY";
	int fd;
	io *io;

	if((fd = herefile(file))<0){
		Xerror3("<< can't get temp file", file, Errstr());
		return;
	}
	io = openiofd(fd);
	psubst(io, (unsigned char*)runq->code[runq->pc++].s);
	flushio(io);
	closeio(io);

	/* open for reading and unlink */
	if((fd = Open(file, 3))<0){
		Xerror3("<< can't open", file, Errstr());
		return;
	}
	pushredir(ROPEN, fd, runq->code[runq->pc++].i);
}

void
Xhereq(void)
{
	char file[]="/tmp/hereXXXXXXXXXXYY", *body;
	int fd;

	if((fd = herefile(file))<0){
		Xerror3("<< can't get temp file", file, Errstr());
		return;
	}
	body = runq->code[runq->pc++].s;
	Write(fd, body, strlen(body));
	Close(fd);

	/* open for reading and unlink */
	if((fd = Open(file, 3))<0){
		Xerror3("<< can't open", file, Errstr());
		return;
	}
	pushredir(ROPEN, fd, runq->code[runq->pc++].i);
}

void
Xread(void)
{
	char *file;
	int fd;

	switch(count(runq->argv->words)){
	default:
		Xerror1("< requires singleton");
		return;
	case 0:
		Xerror1("< requires file");
		return;
	case 1:
		break;
	}
	file = runq->argv->words->word;
	if((fd = Open(file, 0))<0){
		Xerror3("< can't open", file, Errstr());
		return;
	}
	pushredir(ROPEN, fd, runq->code[runq->pc++].i);
	poplist();
}

void
Xrdwr(void)
{
	char *file;
	int fd;

	switch(count(runq->argv->words)){
	default:
		Xerror1("<> requires singleton");
		return;
	case 0:
		Xerror1("<> requires file");
		return;
	case 1:
		break;
	}
	file = runq->argv->words->word;
	if((fd = Open(file, 2))<0){
		Xerror3("<> can't open", file, Errstr());
		return;
	}
	pushredir(ROPEN, fd, runq->code[runq->pc++].i);
	poplist();
}

void
Xpopredir(void)
{
	redir *rp = runq->redir;

	if(rp==0)
		panic("Xpopredir null!", 0);
	runq->redir = rp->next;
	if(rp->type==ROPEN)
		Close(rp->from);
	free(rp);
}

void
Xreturn(void)
{
	while(runq->redir!=runq->startredir)
		Xpopredir();
	popthread();
	if(runq==0)
		Exit();
}

void
Xtrue(void)
{
	if(truestatus()) runq->pc++;
	else runq->pc = runq->code[runq->pc].i;
}

void
Xif(void)
{
	ifnot = 1;
	if(truestatus()) runq->pc++;
	else runq->pc = runq->code[runq->pc].i;
}

void
Xwastrue(void)
{
	ifnot = 0;
}

void
Xword(void)
{
	pushword(runq->code[runq->pc++].s);
}

void
Xwrite(void)
{
	char *file;
	int fd;

	switch(count(runq->argv->words)){
	default:
		Xerror1("> requires singleton");
		return;
	case 0:
		Xerror1("> requires file");
		return;
	case 1:
		break;
	}
	file = runq->argv->words->word;
	if((fd = Creat(file))<0){
		Xerror3("> can't create", file, Errstr());
		return;
	}
	pushredir(ROPEN, fd, runq->code[runq->pc++].i);
	poplist();
}

void
Xmatch(void)
{
	word *p;
	char *s;

	setstatus("no match");
	s = runq->argv->words->word;
	for(p = runq->argv->next->words;p;p = p->next)
		if(match(s, p->word, '\0')){
			setstatus("");
			break;
		}
	poplist();
	poplist();
}

void
Xcase(void)
{
	word *p;
	char *s;
	int ok = 0;

	s = runq->argv->next->words->word;
	for(p = runq->argv->words;p;p = p->next){
		if(match(s, p->word, '\0')){
			ok = 1;
			break;
		}
	}
	if(ok)
		runq->pc++;
	else
		runq->pc = runq->code[runq->pc].i;
	poplist();
}

static word*
conclist(word *lp, word *rp, word *tail)
{
	word *v, *p, **end;
	int ln, rn;

	for(end = &v;;){
		ln = strlen(lp->word), rn = strlen(rp->word);
		p = Newword(emalloc(ln+rn+1), (word *)0);
		memmove(p->word, lp->word, ln);
		memmove(p->word+ln, rp->word, rn+1);
		*end = p, end = &p->next;
		if(lp->next == 0 && rp->next == 0)
			break;
		if(lp->next) lp = lp->next;
		if(rp->next) rp = rp->next;
	}
	*end = tail;
	return v;
}

void
Xconc(void)
{
	word *lp = runq->argv->words;
	word *rp = runq->argv->next->words;
	word *vp = runq->argv->next->next->words;
	int lc = count(lp), rc = count(rp);
	if(lc!=0 || rc!=0){
		if(lc==0 || rc==0){
			Xerror1("null list in concatenation");
			return;
		}
		if(lc!=1 && rc!=1 && lc!=rc){
			Xerror1("mismatched list lengths in concatenation");
			return;
		}
		vp = conclist(lp, rp, vp);
	}
	poplist();
	poplist();
	runq->argv->words = vp;
}

void
Xassign(void)
{
	var *v;

	if(count(runq->argv->words)!=1){
		Xerror1("= variable name not singleton!");
		return;
	}
	v = vlook(runq->argv->words->word);
	poplist();
	freewords(v->val);
	v->val = Poplist();
	v->changed = 1;
}

/*
 * copy arglist a, adding the copy to the front of tail
 */
word*
copywords(word *a, word *tail)
{
	word *v = 0, **end;

	for(end=&v;a;a = a->next,end=&(*end)->next)
		*end = newword(a->word, 0);
	*end = tail;
	return v;
}

void
Xdol(void)
{
	word *a, *star;
	char *s, *t;
	int n;

	if(count(runq->argv->words)!=1){
		Xerror1("$ variable name not singleton!");
		return;
	}
	n = 0;
	s = runq->argv->words->word;
	for(t = s;'0'<=*t && *t<='9';t++) n = n*10+*t-'0';
	a = runq->argv->next->words;
	if(n==0 || *t)
		a = copywords(vlook(s)->val, a);
	else{
		star = vlook("*")->val;
		if(star && 1<=n && n<=count(star)){
			while(--n) star = star->next;
			a = newword(star->word, a);
		}
	}
	poplist();
	runq->argv->words = a;
}

void
Xqw(void)
{
	char *s, *d;
	word *a, *p;
	int n;

	a = runq->argv->words;
	if(a==0){
		pushword("");
		return;
	}
	if(a->next==0)
		return;
	n=0;
	for(p=a;p;p=p->next)
		n+=1+strlen(p->word);
	s = emalloc(n+1);
	d = s;
	d += strlen(strcpy(d, a->word));
	for(p=a->next;p;p=p->next){
		*d++=' ';
		d += strlen(strcpy(d, p->word));
	}
	free(a->word);
	freewords(a->next);
	a->word = s;
	a->next = 0;
}

static word*
copynwords(word *a, word *tail, int n)
{
	word *v, **end;
	
	v = 0;
	end = &v;
	while(n-- > 0){
		*end = newword(a->word, 0);
		end = &(*end)->next;
		a = a->next;
	}
	*end = tail;
	return v;
}

static word*
subwords(word *val, int len, word *sub, word *a)
{
	int n, m;
	char *s;

	if(sub==0)
		return a;
	a = subwords(val, len, sub->next, a);
	s = sub->word;
	m = 0;
	n = 0;
	while('0'<=*s && *s<='9')
		n = n*10+ *s++ -'0';
	if(*s == '-'){
		if(*++s == 0)
			m = len - n;
		else{
			while('0'<=*s && *s<='9')
				m = m*10+ *s++ -'0';
			m -= n;
		}
	}
	if(n<1 || n>len || m<0)
		return a;
	if(n+m>len)
		m = len-n;
	while(--n > 0)
		val = val->next;
	return copynwords(val, a, m+1);
}

void
Xsub(void)
{
	word *a, *v;
	char *s;

	if(count(runq->argv->next->words)!=1){
		Xerror1("$() variable name not singleton!");
		return;
	}
	s = runq->argv->next->words->word;
	a = runq->argv->next->next->words;
	v = vlook(s)->val;
	a = subwords(v, count(v), runq->argv->words, a);
	poplist();
	poplist();
	runq->argv->words = a;
}

void
Xcount(void)
{
	word *a;
	char *s, *t, num[12];
	int n;

	if(count(runq->argv->words)!=1){
		Xerror1("$# variable name not singleton!");
		return;
	}
	n = 0;
	s = runq->argv->words->word;
	for(t = s;'0'<=*t && *t<='9';t++) n = n*10+*t-'0';
	if(n==0 || *t){
		a = vlook(s)->val;
		inttoascii(num, count(a));
	}
	else{
		a = vlook("*")->val;
		inttoascii(num, a && 1<=n && n<=count(a)?1:0);
	}
	poplist();
	pushword(num);
}

void
Xlocal(void)
{
	if(count(runq->argv->words)!=1){
		Xerror1("local variable name must be singleton");
		return;
	}
	runq->local = newvar(runq->argv->words->word, runq->local);
	poplist();
	runq->local->val = Poplist();
	runq->local->changed = 1;
}

void
Xunlocal(void)
{
	var *hid, *v = runq->local;
	if(v==0)
		panic("Xunlocal: no locals!", 0);
	runq->local = v->next;
	hid = vlook(v->name);
	hid->changed = 1;
	freevar(v);
}

void
Xfn(void)
{
	var *v;
	word *a;
	int pc = runq->pc;
	runq->pc = runq->code[pc].i;
	for(a = runq->argv->words;a;a = a->next){
		v = gvlook(a->word);
		if(v->fn)
			codefree(v->fn);
		v->fn = codecopy(runq->code);
		v->pc = pc+2;
		v->fnchanged = 1;
	}
	poplist();
}

void
Xdelfn(void)
{
	var *v;
	word *a;
	for(a = runq->argv->words;a;a = a->next){
		v = gvlook(a->word);
		if(v->fn)
			codefree(v->fn);
		v->fn = 0;
		v->fnchanged = 1;
	}
	poplist();
}

static char*
concstatus(char *s, char *t)
{
	int n, m;

	if(t==0) return s;
	if(s==0) return t;
	n = strlen(s);
	m = strlen(t);
	s = erealloc(s, n+m+2);
	if(n > 0) s[n++]='|';
	memmove(s+n, t, m+1);
	free(t);
	return s;
}

void
Xpipewait(void)
{
	char *old = Getstatus();
	if(runq->pid==-1){
		Setstatus(concstatus(runq->status, old));
		runq->status=0;
	}else{
		while(Waitfor(runq->pid) < 0)
			;
		runq->pid=-1;
		Setstatus(concstatus(Getstatus(), old));
	}
}

static char *promptstr;

void
Xrdcmds(void)
{
	thread *p = runq;

	if(flag['s'] && !truestatus())
		pfmt(err, "status=%v\n", vlook("status")->val);
	flushio(err);

	lex = p->lex;
	if(p->iflag){
		word *prompt = vlook("prompt")->val;
		if(prompt)
			promptstr = prompt->word;
		else
			promptstr="% ";
	}
	Noerror();
	nerror = 0;
	if(yyparse()){
		if(p->iflag && (!lex->eof || Eintr())){
			if(Eintr()){
				pchr(err, '\n');
				lex->eof = 0;
			}
			--p->pc;	/* go back for next command */
		}
	}
	else{
		if(lex->eof){
			dontclose(lex->input->fd);
			freelexer(lex);
			p->lex = 0;
		} else
			--p->pc;	/* re-execute Xrdcmds after codebuf runs */
		start(codebuf, 2, p->local, p->redir);
	}
	lex = 0;
	freenodes();
}

void
pprompt(void)
{
	word *prompt;

	if(!runq->iflag)
		return;

	Prompt(promptstr);
	doprompt = 0;

	prompt = vlook("prompt")->val;
	if(prompt && prompt->next)
		promptstr = prompt->next->word;
	else
		promptstr = "\t";
}

char*
srcfile(thread *p)
{
	return p->code[1].s;
}

void
Xerror1(char *s)
{
	setstatus("error");
	pfln(err, srcfile(runq), runq->line);
	pfmt(err, ": %s\n", s);
	flushio(err);
	while(!runq->iflag) Xreturn();
}
void
Xerror2(char *s, char *e)
{
	setstatus(e);
	pfln(err, srcfile(runq), runq->line);
	pfmt(err, ": %s: %s\n", s, e);
	flushio(err);
	while(!runq->iflag) Xreturn();
}
void
Xerror3(char *s, char *m, char *e)
{
	setstatus(e);
	pfln(err, srcfile(runq), runq->line);
	pfmt(err, ": %s: %s: %s\n", s, m, e);
	flushio(err);
	while(!runq->iflag) Xreturn();
}

void
Setstatus(char *s)
{
	setvar("status", Newword(s?s:estrdup(""), (word *)0));
}
void
setstatus(char *s)
{
	Setstatus(estrdup(s));
}
char*
Getstatus(void)
{
	var *status = vlook("status");
	word *val = status->val;
	if(val==0) return 0;
	status->val=0;
	status->changed=1;
	freewords(val->next);
	return Freeword(val);
}
char*
getstatus(void)
{
	var *status = vlook("status");
	return status->val?status->val->word:"";
}

int
truestatus(void)
{
	char *s;
	for(s = getstatus();*s;s++)
		if(*s!='|' && *s!='0')
			return 0;
	return 1;
}

void
Xfor(void)
{
	word *a = runq->argv->words;
	if(a==0){
		poplist();
		runq->pc = runq->code[runq->pc].i;
	}
	else{
		runq->argv->words = a->next;
		a->next = 0;
		freewords(runq->local->val);
		runq->local->val = a;
		runq->local->changed = 1;
		runq->pc++;
	}
}

void
Xglob(void)
{
	word *a, *x;

	for(a = runq->argv->words; a; a = x){
		x = a->next;
		globword(a);
	}
}

void
Xsrcline(void)
{
	runq->line = runq->code[runq->pc++].i;
}
