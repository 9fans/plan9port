#include "rc.h"
#include "getflags.h"
#include "exec.h"
#include "io.h"
#include "fns.h"
/*
 * Start executing the given code at the given pc with the given redirection
 */
char *argv0="rc";

void
start(code *c, int pc, var *local)
{
	struct thread *p = new(struct thread);

	p->code = codecopy(c);
	p->pc = pc;
	p->argv = 0;
	p->redir = p->startredir = runq?runq->redir:0;
	p->local = local;
	p->cmdfile = 0;
	p->cmdfd = 0;
	p->eof = 0;
	p->iflag = 0;
	p->lineno = 1;
	p->ret = runq;
	runq = p;
}

word*
newword(char *wd, word *next)
{
	word *p = new(word);
	p->word = strdup(wd);
	p->next = next;
	return p;
}

void
pushword(char *wd)
{
	if(runq->argv==0)
		panic("pushword but no argv!", 0);
	runq->argv->words = newword(wd, runq->argv->words);
}

void
popword(void)
{
	word *p;
	if(runq->argv==0)
		panic("popword but no argv!", 0);
	p = runq->argv->words;
	if(p==0)
		panic("popword but no word!", 0);
	runq->argv->words = p->next;
	efree(p->word);
	efree((char *)p);
}

void
freelist(word *w)
{
	word *nw;
	while(w){
		nw = w->next;
		efree(w->word);
		efree((char *)w);
		w = nw;
	}
}

void
pushlist(void)
{
	list *p = new(list);
	p->next = runq->argv;
	p->words = 0;
	runq->argv = p;
}

void
poplist(void)
{
	list *p = runq->argv;
	if(p==0)
		panic("poplist but no argv", 0);
	freelist(p->words);
	runq->argv = p->next;
	efree((char *)p);
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
	redir * rp = new(redir);
	rp->type = type;
	rp->from = from;
	rp->to = to;
	rp->next = runq->redir;
	runq->redir = rp;
}

var*
newvar(char *name, var *next)
{
	var *v = new(var);
	v->name = name;
	v->val = 0;
	v->fn = 0;
	v->changed = 0;
	v->fnchanged = 0;
	v->next = next;
	v->changefn = 0;
	return v;
}
/*
 * get command line flags, initialize keywords & traps.
 * get values from environment.
 * set $pid, $cflag, $*
 * fabricate bootstrap code and start it (*=(argv);. /usr/lib/rcmain $*)
 * start interpreting code
 */
int
main(int argc, char *argv[])
{
	code bootstrap[32];
	char num[12], *rcmain;
	int i;

	/* needed for rcmain later */
	putenv("PLAN9", unsharp("#9"));

	argc = getflags(argc, argv, "DSYsrdiIlxepvVc:1m:1[command]", 1);
	if(argc==-1)
		usage("[file [arg ...]]");
	if(argv[0][0]=='-')
		flag['l'] = flagset;
	if(flag['I'])
		flag['i'] = 0;
	else if(flag['i']==0 && argc==1 && Isatty(0)) flag['i'] = flagset;
	rcmain = flag['m'] ? flag['m'][0] : Rcmain();
	err = openfd(2);
	kinit();
	Trapinit();
	Vinit();
	inttoascii(num, mypid = getpid());
	pathinit();
	setvar("pid", newword(num, (word *)0));
	setvar("cflag", flag['c']?newword(flag['c'][0], (word *)0)
				:(word *)0);
	setvar("rcname", newword(argv[0], (word *)0));
	i = 0;
	bootstrap[i++].i = 1;
	bootstrap[i++].f = Xmark;
	bootstrap[i++].f = Xword;
	bootstrap[i++].s="*";
	bootstrap[i++].f = Xassign;
	bootstrap[i++].f = Xmark;
	bootstrap[i++].f = Xmark;
	bootstrap[i++].f = Xword;
	bootstrap[i++].s="*";
	bootstrap[i++].f = Xdol;
	bootstrap[i++].f = Xword;
	bootstrap[i++].s = rcmain;
	bootstrap[i++].f = Xword;
	bootstrap[i++].s=".";
	bootstrap[i++].f = Xsimple;
	bootstrap[i++].f = Xexit;
	bootstrap[i].i = 0;
	start(bootstrap, 1, (var *)0);
	/* prime bootstrap argv */
	pushlist();
	argv0 = strdup(argv[0]);
	for(i = argc-1;i!=0;--i) pushword(argv[i]);
	for(;;){
		if(flag['r'])
			pfnc(err, runq);
		runq->pc++;
		(*runq->code[runq->pc-1].f)();
		if(ntrap)
			dotrap();
	}
	return 0;  /* not reached; silence OS X Lion gcc */
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
 * Xbackq{... Xreturn}			make thread for {}, push stdout
 * Xbang				complement condition
 * Xcase(pat, value){...}		exec code on match, leave (value) on
 * 					stack
 * Xclose[i]				close file descriptor
 * Xconc(left, right)			concatenate, push results
 * Xcount(name)				push var count
 * Xdelfn(name)				delete function definition
 * Xdeltraps(names)			delete named traps
 * Xdol(name)				get variable value
 * Xqdol(name)				concatenate variable components
 * Xdup[i j]				dup file descriptor
 * Xexit				rc exits with status
 * Xfalse{...}				execute {} if false
 * Xfn(name){... Xreturn}			define function
 * Xfor(var, list){... Xreturn}		for loop
 * Xjump[addr]				goto
 * Xlocal(name, val)			create local variable, assign value
 * Xmark				mark stack
 * Xmatch(pat, str)			match pattern, set status
 * Xpipe[i j]{... Xreturn}{... Xreturn}	construct a pipe between 2 new threads,
 * 					wait for both
 * Xpipefd[type]{... Xreturn}		connect {} to pipe (input or output,
 * 					depending on type), push /dev/fd/??
 * Xpopm(value)				pop value from stack
 * Xrdwr(file)[fd]			open file for reading and writing
 * Xread(file)[fd]			open file to read
 * Xsettraps(names){... Xreturn}		define trap functions
 * Xshowtraps				print trap list
 * Xsimple(args)			run command and wait
 * Xreturn				kill thread
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
	int f;
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
	if((f = open(file, 1))<0 && (f = Creat(file))<0){
		pfmt(err, "%s: ", file);
		Xerror("can't open");
		return;
	}
	Seek(f, 0L, 2);
	pushredir(ROPEN, f, runq->code[runq->pc].i);
	runq->pc++;
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
	pushredir(RCLOSE, runq->code[runq->pc].i, 0);
	runq->pc++;
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
	if(eflagok && !truestatus()) Xexit();
}

void
Xexit(void)
{
	struct var *trapreq;
	struct word *starval;
	static int beenhere = 0;
	if(getpid()==mypid && !beenhere){
		trapreq = vlook("sigexit");
		if(trapreq->fn){
			beenhere = 1;
			--runq->pc;
			starval = vlook("*")->val;
			start(trapreq->fn, trapreq->pc, (struct var *)0);
			runq->local = newvar(strdup("*"), runq->local);
			runq->local->val = copywords(starval, (struct word *)0);
			runq->local->changed = 1;
			runq->redir = runq->startredir = 0;
			return;
		}
	}
	Exit(getstatus());
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
Xread(void)
{
	char *file;
	int f;
	switch(count(runq->argv->words)){
	default:
		Xerror1("< requires singleton\n");
		return;
	case 0:
		Xerror1("< requires file\n");
		return;
	case 1:
		break;
	}
	file = runq->argv->words->word;
	if((f = open(file, 0))<0){
		pfmt(err, "%s: ", file);
		Xerror("can't open");
		return;
	}
	pushredir(ROPEN, f, runq->code[runq->pc].i);
	runq->pc++;
	poplist();
}

void
Xrdwr(void)
{
	char *file;
	int f;

	switch(count(runq->argv->words)){
	default:
		Xerror1("<> requires singleton\n");
		return;
	case 0:
		Xerror1("<> requires file\n");
		return;
	case 1:
		break;
	}
	file = runq->argv->words->word;
	if((f = open(file, ORDWR))<0){
		pfmt(err, "%s: ", file);
		Xerror("can't open");
		return;
	}
	pushredir(ROPEN, f, runq->code[runq->pc].i);
	runq->pc++;
	poplist();
}

void
turfredir(void)
{
	while(runq->redir!=runq->startredir)
		Xpopredir();
}

void
Xpopredir(void)
{
	struct redir *rp = runq->redir;
	if(rp==0)
		panic("turfredir null!", 0);
	runq->redir = rp->next;
	if(rp->type==ROPEN)
		close(rp->from);
	efree((char *)rp);
}

void
Xreturn(void)
{
	struct thread *p = runq;
	turfredir();
	while(p->argv) poplist();
	codefree(p->code);
	runq = p->ret;
	efree((char *)p);
	if(runq==0)
		Exit(getstatus());
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
	int f;
	switch(count(runq->argv->words)){
	default:
		Xerror1("> requires singleton\n");
		return;
	case 0:
		Xerror1("> requires file\n");
		return;
	case 1:
		break;
	}
	file = runq->argv->words->word;
	if((f = Creat(file))<0){
		pfmt(err, "%s: ", file);
		Xerror("can't open");
		return;
	}
	pushredir(ROPEN, f, runq->code[runq->pc].i);
	runq->pc++;
	poplist();
}

char*
list2str(word *words)
{
	char *value, *s, *t;
	int len = 0;
	word *ap;
	for(ap = words;ap;ap = ap->next)
		len+=1+strlen(ap->word);
	value = emalloc(len+1);
	s = value;
	for(ap = words;ap;ap = ap->next){
		for(t = ap->word;*t;) *s++=*t++;
		*s++=' ';
	}
	if(s==value)
		*s='\0';
	else s[-1]='\0';
	return value;
}

void
Xmatch(void)
{
	word *p;
	char *subject;
	subject = list2str(runq->argv->words);
	setstatus("no match");
	for(p = runq->argv->next->words;p;p = p->next)
		if(match(subject, p->word, '\0')){
			setstatus("");
			break;
		}
	efree(subject);
	poplist();
	poplist();
}

void
Xcase(void)
{
	word *p;
	char *s;
	int ok = 0;
	s = list2str(runq->argv->next->words);
	for(p = runq->argv->words;p;p = p->next){
		if(match(s, p->word, '\0')){
			ok = 1;
			break;
		}
	}
	efree(s);
	if(ok)
		runq->pc++;
	else
		runq->pc = runq->code[runq->pc].i;
	poplist();
}

word*
conclist(word *lp, word *rp, word *tail)
{
	char *buf;
	word *v;
	if(lp->next || rp->next)
		tail = conclist(lp->next==0?lp:lp->next, rp->next==0?rp:rp->next,
			tail);
	buf = emalloc(strlen(lp->word)+strlen(rp->word)+1);
	strcpy(buf, lp->word);
	strcat(buf, rp->word);
	v = newword(buf, tail);
	efree(buf);
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
		Xerror1("variable name not singleton!");
		return;
	}
	deglob(runq->argv->words->word);
	v = vlook(runq->argv->words->word);
	poplist();
	globlist();
	freewords(v->val);
	v->val = runq->argv->words;
	v->changed = 1;
	if(v->changefn)
		v->changefn(v);
	runq->argv->words = 0;
	poplist();
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
		Xerror1("variable name not singleton!");
		return;
	}
	s = runq->argv->words->word;
	deglob(s);
	n = 0;
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
Xqdol(void)
{
	word *a, *p;
	char *s;
	int n;
	if(count(runq->argv->words)!=1){
		Xerror1("variable name not singleton!");
		return;
	}
	s = runq->argv->words->word;
	deglob(s);
	a = vlook(s)->val;
	poplist();
	n = count(a);
	if(n==0){
		pushword("");
		return;
	}
	for(p = a;p;p = p->next) n+=strlen(p->word);
	s = emalloc(n);
	if(a){
		strcpy(s, a->word);
		for(p = a->next;p;p = p->next){
			strcat(s, " ");
			strcat(s, p->word);
		}
	}
	else
		s[0]='\0';
	pushword(s);
	efree(s);
}

word*
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

word*
subwords(word *val, int len, word *sub, word *a)
{
	int n, m;
	char *s;
	if(!sub)
		return a;
	a = subwords(val, len, sub->next, a);
	s = sub->word;
	deglob(s);
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
		Xerror1("variable name not singleton!");
		return;
	}
	s = runq->argv->next->words->word;
	deglob(s);
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
	char *s, *t;
	int n;
	char num[12];
	if(count(runq->argv->words)!=1){
		Xerror1("variable name not singleton!");
		return;
	}
	s = runq->argv->words->word;
	deglob(s);
	n = 0;
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
		Xerror1("variable name must be singleton\n");
		return;
	}
	deglob(runq->argv->words->word);
	runq->local = newvar(strdup(runq->argv->words->word), runq->local);
	runq->local->val = copywords(runq->argv->next->words, (word *)0);
	runq->local->changed = 1;
	poplist();
	poplist();
}

void
Xunlocal(void)
{
	var *v = runq->local, *hid;
	if(v==0)
		panic("Xunlocal: no locals!", 0);
	runq->local = v->next;
	hid = vlook(v->name);
	hid->changed = 1;
	efree(v->name);
	freewords(v->val);
	efree((char *)v);
}

void
freewords(word *w)
{
	word *nw;
	while(w){
		efree(w->word);
		nw = w->next;
		efree((char *)w);
		w = nw;
	}
}

void
Xfn(void)
{
	var *v;
	word *a;
	int end;
	end = runq->code[runq->pc].i;
	for(a = runq->argv->words;a;a = a->next){
		v = gvlook(a->word);
		if(v->fn)
			codefree(v->fn);
		v->fn = codecopy(runq->code);
		v->pc = runq->pc+2;
		v->fnchanged = 1;
	}
	runq->pc = end;
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

char*
concstatus(char *s, char *t)
{
	static char v[NSTATUS+1];
	int n = strlen(s);
	strncpy(v, s, NSTATUS);
	if(n<NSTATUS){
		v[n]='|';
		strncpy(v+n+1, t, NSTATUS-n-1);
	}
	v[NSTATUS]='\0';
	return v;
}

void
Xpipewait(void)
{
	char status[NSTATUS+1];
	if(runq->pid==-1)
		setstatus(concstatus(runq->status, getstatus()));
	else{
		strncpy(status, getstatus(), NSTATUS);
		status[NSTATUS]='\0';
		Waitfor(runq->pid, 1);
		runq->pid=-1;
		setstatus(concstatus(getstatus(), status));
	}
}

void
Xrdcmds(void)
{
	struct thread *p = runq;
	word *prompt;
	flush(err);
	nerror = 0;
	if(flag['s'] && !truestatus())
		pfmt(err, "status=%v\n", vlook("status")->val);
	if(runq->iflag){
		prompt = vlook("prompt")->val;
		if(prompt)
			promptstr = prompt->word;
		else
			promptstr="% ";
	}
	Noerror();
	if((flag['Y'] ? yyparse : parse)()){
		if(!p->iflag || p->eof && !Eintr()){
			if(p->cmdfile)
				efree(p->cmdfile);
			closeio(p->cmdfd);
			Xreturn();	/* should this be omitted? */
		}
		else{
			if(Eintr()){
				pchr(err, '\n');
				p->eof = 0;
			}
			--p->pc;	/* go back for next command */
		}
	}
	else{
		ntrap = 0;	/* avoid double-interrupts during blocked writes */
		--p->pc;	/* re-execute Xrdcmds after codebuf runs */
		start(codebuf, 1, runq->local);
	}
	freenodes();
}

void
Xerror(char *s)
{
	if(strcmp(argv0, "rc")==0 || strcmp(argv0, "/bin/rc")==0)
		pfmt(err, "rc: %s: %r\n", s);
	else
		pfmt(err, "rc (%s): %s: %r\n", argv0, s);
	flush(err);
	setstatus("error");
	while(!runq->iflag) Xreturn();
}

void
Xerror1(char *s)
{
	if(strcmp(argv0, "rc")==0 || strcmp(argv0, "/bin/rc")==0)
		pfmt(err, "rc: %s\n", s);
	else
		pfmt(err, "rc (%s): %s\n", argv0, s);
	flush(err);
	setstatus("error");
	while(!runq->iflag) Xreturn();
}

void
setstatus(char *s)
{
	setvar("status", newword(s, (word *)0));
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
Xdelhere(void)
{
	Unlink(runq->code[runq->pc++].s);
}

void
Xfor(void)
{
	if(runq->argv->words==0){
		poplist();
		runq->pc = runq->code[runq->pc].i;
	}
	else{
		freelist(runq->local->val);
		runq->local->val = runq->argv->words;
		runq->local->changed = 1;
		runq->argv->words = runq->argv->words->next;
		runq->local->val->next = 0;
		runq->pc++;
	}
}

void
Xglob(void)
{
	globlist();
}
