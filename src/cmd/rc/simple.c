/*
 * Maybe `simple' is a misnomer.
 */
#include "rc.h"
#include "getflags.h"
#include "exec.h"
#include "io.h"
#include "fns.h"
/*
 * Search through the following code to see if we're just going to exit.
 */
int
exitnext(void){
	union code *c=&runq->code[runq->pc];
	while(c->f==Xpopredir) c++;
	return c->f==Xexit;
}

void
Xsimple(void)
{
	word *a;
	thread *p = runq;
	var *v;
	struct builtin *bp;
	int pid;
	globlist();
	a = runq->argv->words;
	if(a==0){
		Xerror1("empty argument list");
		return;
	}
	if(flag['x'])
		pfmt(err, "%v\n", p->argv->words); /* wrong, should do redirs */
	v = gvlook(a->word);
	if(v->fn)
		execfunc(v);
	else{
		if(strcmp(a->word, "builtin")==0){
			if(count(a)==1){
				pfmt(err, "builtin: empty argument list\n");
				setstatus("empty arg list");
				poplist();
				return;
			}
			a = a->next;
			popword();
		}
		for(bp = Builtin;bp->name;bp++)
			if(strcmp(a->word, bp->name)==0){
				(*bp->fnc)();
				return;
			}
		if(exitnext()){
			/* fork and wait is redundant */
			pushword("exec");
			execexec();
			Xexit();
		}
		else{
			flush(err);
			Updenv();	/* necessary so changes don't go out again */
			if((pid = execforkexec()) < 0){
				Xerror("try again");
				return;
			}

			/* interrupts don't get us out */
			poplist();
			while(Waitfor(pid, 1) < 0)
				;
		}
	}
}
struct word nullpath = { "", 0};

void
doredir(redir *rp)
{
	if(rp){
		doredir(rp->next);
		switch(rp->type){
		case ROPEN:
			if(rp->from!=rp->to){
				Dup(rp->from, rp->to);
				close(rp->from);
			}
			break;
		case RDUP:
			Dup(rp->from, rp->to);
			break;
		case RCLOSE:
			close(rp->from);
			break;
		}
	}
}

word*
searchpath(char *w)
{
	word *path;
	if(strncmp(w, "/", 1)==0
/*	|| strncmp(w, "#", 1)==0 */
	|| strncmp(w, "./", 2)==0
	|| strncmp(w, "../", 3)==0
	|| (path = vlook("path")->val)==0)
		path=&nullpath;
	return path;
}

void
execexec(void)
{
	popword();	/* "exec" */
	if(runq->argv->words==0){
		Xerror1("empty argument list");
		return;
	}
	doredir(runq->redir);
	Execute(runq->argv->words, searchpath(runq->argv->words->word));
	poplist();
}

void
execfunc(var *func)
{
	word *starval;
	popword();
	starval = runq->argv->words;
	runq->argv->words = 0;
	poplist();
	start(func->fn, func->pc, runq->local);
	runq->local = newvar(strdup("*"), runq->local);
	runq->local->val = starval;
	runq->local->changed = 1;
}

int
dochdir(char *word)
{
	/* report to /dev/wdir if it exists and we're interactive */
	static int wdirfd = -2;
	if(chdir(word)<0) return -1;
	if(flag['i']!=0){
		if(wdirfd==-2)	/* try only once */
			wdirfd = open("/dev/wdir", OWRITE|OCEXEC);
		if(wdirfd>=0)
			write(wdirfd, word, strlen(word));
	}
	return 1;
}

void
execcd(void)
{
	word *a = runq->argv->words;
	word *cdpath;
	char dir[512];
	setstatus("can't cd");
	cdpath = vlook("cdpath")->val;
	switch(count(a)){
	default:
		pfmt(err, "Usage: cd [directory]\n");
		break;
	case 2:
		if(a->next->word[0]=='/' || cdpath==0)
			cdpath=&nullpath;
		for(;cdpath;cdpath = cdpath->next){
			strcpy(dir, cdpath->word);
			if(dir[0])
				strcat(dir, "/");
			strcat(dir, a->next->word);
			if(dochdir(dir)>=0){
				if(strlen(cdpath->word)
				&& strcmp(cdpath->word, ".")!=0)
					pfmt(err, "%s\n", dir);
				setstatus("");
				break;
			}
		}
		if(cdpath==0)
			pfmt(err, "Can't cd %s: %r\n", a->next->word);
		break;
	case 1:
		a = vlook("home")->val;
		if(count(a)>=1){
			if(dochdir(a->word)>=0)
				setstatus("");
			else
				pfmt(err, "Can't cd %s: %r\n", a->word);
		}
		else
			pfmt(err, "Can't cd -- $home empty\n");
		break;
	}
	poplist();
}

void
execexit(void)
{
	switch(count(runq->argv->words)){
	default:
		pfmt(err, "Usage: exit [status]\nExiting anyway\n");
	case 2:
		setstatus(runq->argv->words->next->word);
	case 1:	Xexit();
	}
}

void
execshift(void)
{
	int n;
	word *a;
	var *star;
	switch(count(runq->argv->words)){
	default:
		pfmt(err, "Usage: shift [n]\n");
		setstatus("shift usage");
		poplist();
		return;
	case 2:
		n = atoi(runq->argv->words->next->word);
		break;
	case 1:
		n = 1;
		break;
	}
	star = vlook("*");
	for(;n && star->val;--n){
		a = star->val->next;
		efree(star->val->word);
		efree((char *)star->val);
		star->val = a;
		star->changed = 1;
	}
	setstatus("");
	poplist();
}

int
octal(char *s)
{
	int n = 0;
	while(*s==' ' || *s=='\t' || *s=='\n') s++;
	while('0'<=*s && *s<='7') n = n*8+*s++-'0';
	return n;
}

int
mapfd(int fd)
{
	redir *rp;
	for(rp = runq->redir;rp;rp = rp->next){
		switch(rp->type){
		case RCLOSE:
			if(rp->from==fd)
				fd=-1;
			break;
		case RDUP:
		case ROPEN:
			if(rp->to==fd)
				fd = rp->from;
			break;
		}
	}
	return fd;
}
union code rdcmds[4];

void
execcmds(io *f)
{
	static int first = 1;
	if(first){
		rdcmds[0].i = 1;
		rdcmds[1].f = Xrdcmds;
		rdcmds[2].f = Xreturn;
		first = 0;
	}
	start(rdcmds, 1, runq->local);
	runq->cmdfd = f;
	runq->iflast = 0;
}

void
execeval(void)
{
	char *cmdline, *s, *t;
	int len = 0;
	word *ap;
	if(count(runq->argv->words)<=1){
		Xerror1("Usage: eval cmd ...");
		return;
	}
	eflagok = 1;
	for(ap = runq->argv->words->next;ap;ap = ap->next)
		len+=1+strlen(ap->word);
	cmdline = emalloc(len);
	s = cmdline;
	for(ap = runq->argv->words->next;ap;ap = ap->next){
		for(t = ap->word;*t;) *s++=*t++;
		*s++=' ';
	}
	s[-1]='\n';
	poplist();
	execcmds(opencore(cmdline, len));
	efree(cmdline);
}
union code dotcmds[14];

void
execdot(void)
{
	int iflag = 0;
	int fd;
	list *av;
	thread *p = runq;
	char *zero;
	static int first = 1;
	char file[512];
	word *path;

	if(first){
		dotcmds[0].i = 1;
		dotcmds[1].f = Xmark;
		dotcmds[2].f = Xword;
		dotcmds[3].s="0";
		dotcmds[4].f = Xlocal;
		dotcmds[5].f = Xmark;
		dotcmds[6].f = Xword;
		dotcmds[7].s="*";
		dotcmds[8].f = Xlocal;
		dotcmds[9].f = Xrdcmds;
		dotcmds[10].f = Xunlocal;
		dotcmds[11].f = Xunlocal;
		dotcmds[12].f = Xreturn;
		first = 0;
	}
	else
		eflagok = 1;
	popword();
	if(p->argv->words && strcmp(p->argv->words->word, "-i")==0){
		iflag = 1;
		popword();
	}
	/* get input file */
	if(p->argv->words==0){
		Xerror1("Usage: . [-i] file [arg ...]");
		return;
	}
	zero = strdup(p->argv->words->word);
	popword();
	fd=-1;
	for(path = searchpath(zero);path;path = path->next){
		strcpy(file, path->word);
		if(file[0])
			strcat(file, "/");
		strcat(file, zero);
		if((fd = open(file, 0))>=0) break;
		if(strcmp(file, "/dev/stdin")==0){	/* for sun & ucb */
			fd = Dup1(0);
			if(fd>=0)
				break;
		}
	}
	if(fd<0){
		pfmt(err, "%s: ", zero);
		setstatus("can't open");
		Xerror(".: can't open");
		return;
	}
	/* set up for a new command loop */
	start(dotcmds, 1, (struct var *)0);
	pushredir(RCLOSE, fd, 0);
	runq->cmdfile = zero;
	runq->cmdfd = openfd(fd);
	runq->iflag = iflag;
	runq->iflast = 0;
	/* push $* value */
	pushlist();
	runq->argv->words = p->argv->words;
	/* free caller's copy of $* */
	av = p->argv;
	p->argv = av->next;
	efree((char *)av);
	/* push $0 value */
	pushlist();
	pushword(zero);
	ndot++;
}

void
execflag(void)
{
	char *letter, *val;
	switch(count(runq->argv->words)){
	case 2:
		setstatus(flag[(uchar)runq->argv->words->next->word[0]]?"":"flag not set");
		break;
	case 3:
		letter = runq->argv->words->next->word;
		val = runq->argv->words->next->next->word;
		if(strlen(letter)==1){
			if(strcmp(val, "+")==0){
				flag[(uchar)letter[0]] = flagset;
				break;
			}
			if(strcmp(val, "-")==0){
				flag[(uchar)letter[0]] = 0;
				break;
			}
		}
	default:
		Xerror1("Usage: flag [letter] [+-]");
		return;
	}
	poplist();
}

void
execwhatis(void){	/* mildly wrong -- should fork before writing */
	word *a, *b, *path;
	var *v;
	struct builtin *bp;
	char file[512];
	struct io out[1];
	int found, sep;
	a = runq->argv->words->next;
	if(a==0){
		Xerror1("Usage: whatis name ...");
		return;
	}
	setstatus("");
	out->fd = mapfd(1);
	out->bufp = out->buf;
	out->ebuf = &out->buf[NBUF];
	out->strp = 0;
	for(;a;a = a->next){
		v = vlook(a->word);
		if(v->val){
			pfmt(out, "%s=", a->word);
			if(v->val->next==0)
				pfmt(out, "%q\n", v->val->word);
			else{
				sep='(';
				for(b = v->val;b && b->word;b = b->next){
					pfmt(out, "%c%q", sep, b->word);
					sep=' ';
				}
				pfmt(out, ")\n");
			}
			found = 1;
		}
		else
			found = 0;
		v = gvlook(a->word);
		if(v->fn)
			pfmt(out, "fn %s %s\n", v->name, v->fn[v->pc-1].s);
		else{
			for(bp = Builtin;bp->name;bp++)
				if(strcmp(a->word, bp->name)==0){
					pfmt(out, "builtin %s\n", a->word);
					break;
				}
			if(!bp->name){
				for(path = searchpath(a->word);path;path = path->next){
					strcpy(file, path->word);
					if(file[0])
						strcat(file, "/");
					strcat(file, a->word);
					if(Executable(file)){
						pfmt(out, "%s\n", file);
						break;
					}
				}
				if(!path && !found){
					pfmt(err, "%s: not found\n", a->word);
					setstatus("not found");
				}
			}
		}
	}
	poplist();
	flush(err);
}

void
execwait(void)
{
	switch(count(runq->argv->words)){
	default:
		Xerror1("Usage: wait [pid]");
		return;
	case 2:
		Waitfor(atoi(runq->argv->words->next->word), 0);
		break;
	case 1:
		Waitfor(-1, 0);
		break;
	}
	poplist();
}
