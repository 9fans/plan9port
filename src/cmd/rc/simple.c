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
	int i=ifnot;
	thread *p=runq;
	code *c;
loop:
	c=&p->code[p->pc];
	while(1){
		if(c->f==Xpopredir || c->f==Xunlocal)
			c++;
		else if(c->f==Xsrcline)
			c += 2;
		else if(c->f==Xwastrue){
			c++;
			i=0;
		}
		else if(c->f==Xifnot){
			if(i)
				c += 2;
			else
				c = &p->code[c[1].i];
		}
		else if(c->f==Xreturn){
			p = p->ret;
			if(p==0)
				return 1;
			goto loop;
		}else
			break;
	}
	return c->f==Xexit;
}

void (*builtinfunc(char *name))(void)
{
	extern builtin Builtin[];
	builtin *bp;

	for(bp = Builtin;bp->name;bp++)
		if(strcmp(name, bp->name)==0)
			return bp->fnc;
	return 0;
}

void
Xsimple(void)
{
	void (*f)(void);
	word *a;
	var *v;
	int pid;

	a = runq->argv->words;
	if(a==0){
		Xerror1("empty argument list");
		return;
	}
	if(flag['x'])
		pfmt(err, "%v\n", a); /* wrong, should do redirs */
	v = gvlook(a->word);
	if(v->fn)
		execfunc(v);
	else{
		if(strcmp(a->word, "builtin")==0){
			a = a->next;
			if(a==0){
				Xerror1("builtin: empty argument list");
				return;
			}
			popword();	/* "builtin" */
		}
		f = builtinfunc(a->word);
		if(f){
			(*f)();
			return;
		}
		if(exitnext()){
			/* fork and wait is redundant */
			pushword("exec");
			execexec();
			/* does not return */
		}
		else{
			if((pid = execforkexec()) < 0){
				Xerror2("try again", Errstr());
				return;
			}
			poplist();

			/* interrupts don't get us out */
			while(Waitfor(pid) < 0)
				;
		}
	}
}

static void
doredir(redir *rp)
{
	if(rp){
		doredir(rp->next);
		switch(rp->type){
		case ROPEN:
			if(rp->from!=rp->to){
				Dup(rp->from, rp->to);
				Close(rp->from);
			}
			break;
		case RDUP:
			Dup(rp->from, rp->to);
			break;
		case RCLOSE:
			Close(rp->from);
			break;
		}
	}
}

word*
_searchpath(char *w, char *v)
{
	static struct word nullpath = { "", 0 };
	word *path;

	if(w[0] && w[0] != '/' && w[0] != '#' &&
	  (w[0] != '.' || (w[1] && w[1] != '/' && (w[1] != '.' || w[2] && w[2] != '/')))){
		path = vlook(v)->val;
		if(path)
			return path;
	}
	return &nullpath;
}

char*
makepath(char *dir, char *file)
{
	char *path;
	int m, n = strlen(dir);
	if(n==0) return estrdup(file);
	while (n > 0 && dir[n-1]=='/') n--;
	while (file[0]=='/') file++;
	m = strlen(file);
	path = emalloc(n + m + 2);
	if(n>0) memmove(path, dir, n);
	path[n++]='/';
	memmove(path+n, file, m+1);
	return path;
}

static char**
mkargv(word *a)
{
	char **argv = (char **)emalloc((count(a)+2)*sizeof(char *));
	char **argp = argv+1;
	for(;a;a = a->next) *argp++=a->word;
	*argp = 0;
	return argv;
}

void
execexec(void)
{
	char **argv;
	word *path;

	popword();	/* "exec" */
	if(runq->argv->words==0){
		Xerror1("exec: empty argument list");
		return;
	}
	argv = mkargv(runq->argv->words);
	Updenv();
	doredir(runq->redir);
	for(path = _searchpath(argv[1], "path"); path; path = path->next){
		argv[0] = makepath(path->word, argv[1]);
		Exec(argv);
	}
	setstatus(Errstr());
	pfln(err, srcfile(runq), runq->line);
	pfmt(err, ": %s: %s\n", argv[1], getstatus());
	Xexit();
}

void
execfunc(var *func)
{
	popword();	/* name */
	startfunc(func, Poplist(), runq->local, runq->redir);
}

void
execcd(void)
{
	word *a = runq->argv->words;
	word *cdpath;
	char *dir;

	setstatus("can't cd");
	switch(count(a)){
	default:
		pfmt(err, "Usage: cd [directory]\n");
		break;
	case 2:
		a = a->next;
		for(cdpath = _searchpath(a->word, "cdpath"); cdpath; cdpath = cdpath->next){
			dir = makepath(cdpath->word, a->word);
			if(Chdir(dir)>=0){
				if(cdpath->word[0] != '\0' && strcmp(cdpath->word, ".") != 0)
					pfmt(err, "%s\n", dir);
				free(dir);
				setstatus("");
				break;
			}
			free(dir);
		}
		if(cdpath==0)
			pfmt(err, "Can't cd %s: %s\n", a->word, Errstr());
		break;
	case 1:
		a = vlook("home")->val;
		if(a){
			if(Chdir(a->word)>=0)
				setstatus("");
			else
				pfmt(err, "Can't cd %s: %s\n", a->word, Errstr());
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
	for(;n>0 && star->val;--n){
		a = star->val->next;
		free(Freeword(star->val));
		star->val = a;
		star->changed = 1;
	}
	setstatus("");
	poplist();
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

void
execcmds(io *input, char *file, var *local, redir *redir)
{
	static union code rdcmds[5];

	if(rdcmds[0].i==0){
		rdcmds[0].i = 1;
		rdcmds[1].s="*rdcmds*";
		rdcmds[2].f = Xrdcmds;
		rdcmds[3].f = Xreturn;
		rdcmds[4].f = 0;
	}

	if(exitnext()) turfstack(local);

	start(rdcmds, 2, local, redir);
	runq->lex = newlexer(input, file);
}

void
execeval(void)
{
	char *cmds;
	int len;
	io *f;

	popword();	/* "eval" */

	if(runq->argv->words==0){
		Xerror1("Usage: eval cmd ...");
		return;
	}
	Xqw();		/* make into single word */
	cmds = Popword();
	len = strlen(cmds);
	cmds[len++] = '\n';
	poplist();

	f = openiostr();
	pfln(f, srcfile(runq), runq->line);
	pstr(f, " *eval*");

	execcmds(openiocore(cmds, len), closeiostr(f), runq->local, runq->redir);
}

void
execdot(void)
{
	int fd, bflag, iflag, qflag;
	word *path, *argv;
	char *file;

	popword();	/* "." */

	bflag = iflag = qflag = 0;
	while(runq->argv->words && runq->argv->words->word[0]=='-'){
		char *f = runq->argv->words->word+1;
		if(*f == '-'){
			popword();
			break;
		}
		for(; *f; f++){
			switch(*f){
			case 'b':
				bflag = 1;
				continue;
			case 'i':
				iflag = 1;
				continue;
			case 'q':
				qflag = 1;
				continue;
			}
			goto Usage;
		}
		popword();
	}

	/* get input file */
	if(runq->argv->words==0){
Usage:
		Xerror1("Usage: . [-biq] file [arg ...]");
		return;
	}
	argv = Poplist();
		
	file = 0;
	fd = -1;
	for(path = _searchpath(argv->word, "path"); path; path = path->next){
		file = makepath(path->word, argv->word);
		fd = Open(file, 0);
		if(fd >= 0)
			break;
		if(strcmp(file, "/dev/stdin")==0){	/* for sun & ucb */
			fd = Dup1(0);
			if(fd>=0)
				break;
		}
		free(file);
	}
	if(fd<0){
		if(!qflag)
			Xerror3(". can't open", argv->word, Errstr());
		freewords(argv);
		return;
	}

	execcmds(openiofd(fd), file, (var*)0, runq->redir);
	pushredir(RCLOSE, fd, 0);
	runq->lex->qflag = qflag;
	runq->iflag = iflag;
	if(iflag || !bflag && flag['b']==0){
		runq->lex->peekc=EOF;
		runq->lex->epilog="";
	}

	runq->local = newvar("*", runq->local);
	runq->local->val = argv->next;
	argv->next=0;
	runq->local->changed = 1;

	runq->local = newvar("0", runq->local);
	runq->local->val = argv;
	runq->local->changed = 1;
}

void
execflag(void)
{
	char *letter, *val;
	switch(count(runq->argv->words)){
	case 2:
		setstatus(flag[(unsigned char)runq->argv->words->next->word[0]]?"":"flag not set");
		break;
	case 3:
		letter = runq->argv->words->next->word;
		val = runq->argv->words->next->next->word;
		if(strlen(letter)==1){
			if(strcmp(val, "+")==0){
				flag[(unsigned char)letter[0]] = flagset;
				break;
			}
			if(strcmp(val, "-")==0){
				flag[(unsigned char)letter[0]] = 0;
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
	char *file;
	io *out;
	int found, sep;
	a = runq->argv->words->next;
	if(a==0){
		Xerror1("Usage: whatis name ...");
		return;
	}
	setstatus("");
	out = openiofd(mapfd(1));
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
				pstr(out, ")\n");
			}
			found = 1;
		}
		else
			found = 0;
		v = gvlook(a->word);
		if(v->fn)
			pfmt(out, "fn %q %s\n", v->name, v->fn[v->pc-1].s);
		else{
			if(builtinfunc(a->word))
				pfmt(out, "builtin %s\n", a->word);
			else {
				for(path = _searchpath(a->word, "path"); path; path = path->next){
					file = makepath(path->word, a->word);
					if(Executable(file)){
						pfmt(out, "%s\n", file);
						free(file);
						break;
					}
					free(file);
				}
				if(!path && !found){
					pfmt(err, "%s: not found\n", a->word);
					setstatus("not found");
				}
			}
		}
		flushio(out);
	}
	poplist();
	free(closeiostr(out));	/* don't close fd */
}

void
execwait(void)
{
	switch(count(runq->argv->words)){
	default:
		Xerror1("Usage: wait [pid]");
		return;
	case 2:
		Waitfor(atoi(runq->argv->words->next->word));
		break;
	case 1:
		Waitfor(-1);
		break;
	}
	poplist();
}
