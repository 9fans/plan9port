#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <mach.h>
#include <regexp.h>
#define Extern extern
#include "acid.h"
#include "y.tab.h"

void	cvtatof(Node*, Node*);
void	cvtatoi(Node*, Node*);
void	cvtitoa(Node*, Node*);
void	bprint(Node*, Node*);
void	funcbound(Node*, Node*);
void	printto(Node*, Node*);
void	getfile(Node*, Node*);
void	fmt(Node*, Node*);
void	pcfile(Node*, Node*);
void	pcline(Node*, Node*);
void	setproc(Node*, Node*);
void	strace(Node*, Node*);
void	follow(Node*, Node*);
void	reason(Node*, Node*);
void	newproc(Node*, Node*);
void	startstop(Node*, Node*);
void	match(Node*, Node*);
void	status(Node*, Node*);
void	xkill(Node*,Node*);
void	waitstop(Node*, Node*);
void waitsyscall(Node*, Node*);
void	stop(Node*, Node*);
void	start(Node*, Node*);
void	filepc(Node*, Node*);
void	doerror(Node*, Node*);
void	rc(Node*, Node*);
void	doaccess(Node*, Node*);
void	map(Node*, Node*);
void	readfile(Node*, Node*);
void	interpret(Node*, Node*);
void	include(Node*, Node*);
void	includepipe(Node*, Node*);
void	regexp(Node*, Node*);
void textfile(Node*, Node*);
void deltextfile(Node*, Node*);
void stringn(Node*, Node*);
void xregister(Node*, Node*);
void refconst(Node*, Node*);
void dolook(Node*, Node*);
void step1(Node*, Node*);

typedef struct Btab Btab;
struct Btab
{
	char	*name;
	void	(*fn)(Node*, Node*);
} tab[] =
{
	"access",	doaccess,
	"atof",		cvtatof,
	"atoi",		cvtatoi,
	"deltextfile",	deltextfile,
	"error",	doerror,
	"file",		getfile,
	"filepc",	filepc,
	"fnbound",	funcbound,
	"fmt",		fmt,
	"follow",	follow,
	"include",	include,
	"includepipe",	includepipe,
	"interpret",	interpret,
	"itoa",		cvtitoa,
	"kill",		xkill,
	"map",		map,
	"match",	match,
	"newproc",	newproc,
	"pcfile",	pcfile,
	"pcline",	pcline,
	"print",	bprint,
	"printto",	printto,
	"rc",		rc,
	"readfile",	readfile,
	"reason",	reason,
	"refconst",	refconst,
	"regexp",	regexp,
	"register",	xregister,
	"setproc",	setproc,
	"start",	start,
	"startstop",	startstop,
	"status",	status,
	"step1",	step1,
	"stop",		stop,
	"strace",	strace,
	"stringn",	stringn,
	"textfile",	textfile,
	"var",	dolook,
	"waitstop",	waitstop,
	"waitsyscall",	waitsyscall,
	0
};

void
mkprint(Lsym *s)
{
	prnt = malloc(sizeof(Node));
	memset(prnt, 0, sizeof(Node));
	prnt->op = OCALL;
	prnt->left = malloc(sizeof(Node));
	memset(prnt->left, 0, sizeof(Node));
	prnt->left->sym = s;
}

void
installbuiltin(void)
{
	Btab *b;
	Lsym *s;

	b = tab;
	while(b->name) {
		s = look(b->name);
		if(s == 0)
			s = enter(b->name, Tid);

		s->builtin = b->fn;
		if(b->fn == bprint)
			mkprint(s);
		b++;
	}
}

void
match(Node *r, Node *args)
{
	int i;
	List *f;
	Node *av[Maxarg];
	Node resi, resl;

	na = 0;
	flatten(av, args);
	if(na != 2)
		error("match(obj, list): arg count");

	expr(av[1], &resl);
	if(resl.type != TLIST)
		error("match(obj, list): need list");
	expr(av[0], &resi);

	r->op = OCONST;
	r->type = TINT;
	r->store.fmt = 'D';
	r->store.u.ival = -1;

	i = 0;
	for(f = resl.store.u.l; f; f = f->next) {
		if(resi.type == f->type) {
			switch(resi.type) {
			case TINT:
				if(resi.store.u.ival == f->store.u.ival) {
					r->store.u.ival = i;
					return;
				}
				break;
			case TFLOAT:
				if(resi.store.u.fval == f->store.u.fval) {
					r->store.u.ival = i;
					return;
				}
				break;
			case TSTRING:
				if(scmp(resi.store.u.string, f->store.u.string)) {
					r->store.u.ival = i;
					return;
				}
				break;
			case TLIST:
				error("match(obj, list): not defined for list");
			}
		}
		i++;
	}
}

void
newproc(Node *r, Node *args)
{
	int i;
	Node res;
	char *p, *e;
	char *argv[Maxarg], buf[Strsize];

	i = 1;
	argv[0] = symfil;

	if(args) {
		expr(args, &res);
		if(res.type != TSTRING)
			error("newproc(): arg not string");
		if(res.store.u.string->len >= sizeof(buf))
			error("newproc(): too many arguments");
		memmove(buf, res.store.u.string->string, res.store.u.string->len);
		buf[res.store.u.string->len] = '\0';
		p = buf;
		e = buf+res.store.u.string->len;
		for(;;) {
			while(p < e && (*p == '\t' || *p == ' '))
				*p++ = '\0';
			if(p >= e)
				break;
			argv[i++] = p;
			if(i >= Maxarg)
				error("newproc: too many arguments");
			while(p < e && *p != '\t' && *p != ' ')
				p++;
		}
	}
	argv[i] = 0;
	r->op = OCONST;
	r->type = TINT;
	r->store.fmt = 'D';
	r->store.u.ival = nproc(argv);
}

void
step1(Node *r, Node *args)
{
	Node res;

	USED(r);
	if(args == 0)
		error("step1(pid): no pid");
	expr(args, &res);
	if(res.type != TINT)
		error("step1(pid): arg type");

	msg(res.store.u.ival, "step");
	notes(res.store.u.ival);
	dostop(res.store.u.ival);
}

void
startstop(Node *r, Node *args)
{
	Node res;

	USED(r);
	if(args == 0)
		error("startstop(pid): no pid");
	expr(args, &res);
	if(res.type != TINT)
		error("startstop(pid): arg type");

	msg(res.store.u.ival, "startstop");
	notes(res.store.u.ival);
	dostop(res.store.u.ival);
}

void
waitstop(Node *r, Node *args)
{
	Node res;

	USED(r);
	if(args == 0)
		error("waitstop(pid): no pid");
	expr(args, &res);
	if(res.type != TINT)
		error("waitstop(pid): arg type");

	Bflush(bout);
	msg(res.store.u.ival, "waitstop");
	notes(res.store.u.ival);
	dostop(res.store.u.ival);
}

void
waitsyscall(Node *r, Node *args)
{
	Node res;

	USED(r);
	if(args == 0)
		error("waitsyscall(pid): no pid");
	expr(args, &res);
	if(res.type != TINT)
		error("waitsycall(pid): arg type");

	Bflush(bout);
	msg(res.store.u.ival, "sysstop");
	notes(res.store.u.ival);
	dostop(res.store.u.ival);
}

void
start(Node *r, Node *args)
{
	Node res;

	USED(r);
	if(args == 0)
		error("start(pid): no pid");
	expr(args, &res);
	if(res.type != TINT)
		error("start(pid): arg type");

	msg(res.store.u.ival, "start");
}

void
stop(Node *r, Node *args)
{
	Node res;

	USED(r);
	if(args == 0)
		error("stop(pid): no pid");
	expr(args, &res);
	if(res.type != TINT)
		error("stop(pid): arg type");

	Bflush(bout);
	msg(res.store.u.ival, "stop");
	notes(res.store.u.ival);
	dostop(res.store.u.ival);
}

void
xkill(Node *r, Node *args)
{
	Node res;

	USED(r);
	if(args == 0)
		error("kill(pid): no pid");
	expr(args, &res);
	if(res.type != TINT)
		error("kill(pid): arg type");

	msg(res.store.u.ival, "kill");
	deinstall(res.store.u.ival);
}

void
xregister(Node *r, Node *args)
{
	int tid;
	Regdesc *rp;
	Node res, resid;
	Node *av[Maxarg];

	na = 0;
	flatten(av, args);
	if(na != 1/* && na != 2 */)
		error("register(name): arg count");

	expr(av[0], &res);
	if(res.type != TSTRING)
		error("register(name): arg type: name should be string");
	tid = 0;
	if(na == 2){
		expr(av[1], &resid);
		if(resid.type != TINT)
			error("register(name[, threadid]): arg type: threadid should be int");
		tid = resid.store.u.ival;
	}
	if((rp = regdesc(res.store.u.string->string)) == nil)
		error("no such register");
	r->op = OCONST;
	r->type = TREG;
	r->store.fmt = rp->format;
	r->store.u.reg.name = rp->name;
	r->store.u.reg.thread = tid;
}

void
refconst(Node *r, Node *args)
{
	Node *n;

	if(args == 0)
		error("refconst(expr): arg count");

	n = an(OCONST, ZN, ZN);
	expr(args, n);

	r->op = OCONST;
	r->type = TCON;
	r->store.u.con = n;
}

void
dolook(Node *r, Node *args)
{
	Node res;
	Lsym *l;

	if(args == 0)
		error("var(string): arg count");
	expr(args, &res);
	if(res.type != TSTRING)
		error("var(string): arg type");

	r->op = OCONST;
	if((l = look(res.store.u.string->string)) == nil || l->v->set == 0){
		r->type = TLIST;
		r->store.u.l = nil;
	}else{
		r->type = l->v->type;
		r->store = l->v->store;
	}
}

void
status(Node *r, Node *args)
{
	Node res;
	char *p;

	USED(r);
	if(args == 0)
		error("status(pid): no pid");
	expr(args, &res);
	if(res.type != TINT)
		error("status(pid): arg type");

	p = getstatus(res.store.u.ival);
	r->store.u.string = strnode(p);
	r->op = OCONST;
	r->store.fmt = 's';
	r->type = TSTRING;
}

void
reason(Node *r, Node *args)
{
	Node res;

	if(args == 0)
		error("reason(cause): no cause");
	expr(args, &res);
	if(res.type != TINT)
		error("reason(cause): arg type");

	r->op = OCONST;
	r->type = TSTRING;
	r->store.fmt = 's';
	r->store.u.string = strnode((*mach->exc)(cormap, acidregs));
}

void
follow(Node *r, Node *args)
{
	int n, i;
	Node res;
	u64int f[10];
	List **tail, *l;

	if(args == 0)
		error("follow(addr): no addr");
	expr(args, &res);
	if(res.type != TINT)
		error("follow(addr): arg type");

	n = (*mach->foll)(cormap, acidregs, res.store.u.ival, f);
	if (n < 0)
		error("follow(addr): %r");
	tail = &r->store.u.l;
	for(i = 0; i < n; i++) {
		l = al(TINT);
		l->store.u.ival = f[i];
		l->store.fmt = 'X';
		*tail = l;
		tail = &l->next;
	}
}

void
funcbound(Node *r, Node *args)
{
	int n;
	Node res;
	u64int bounds[2];
	List *l;

	if(args == 0)
		error("fnbound(addr): no addr");
	expr(args, &res);
	if(res.type != TINT)
		error("fnbound(addr): arg type");

	n = fnbound(res.store.u.ival, bounds);
	if (n >= 0) {
		r->store.u.l = al(TINT);
		l = r->store.u.l;
		l->store.u.ival = bounds[0];
		l->store.fmt = 'X';
		l->next = al(TINT);
		l = l->next;
		l->store.u.ival = bounds[1];
		l->store.fmt = 'X';
	}
}

void
setproc(Node *r, Node *args)
{
	Node res;

	USED(r);
	if(args == 0)
		error("setproc(pid): no pid");
	expr(args, &res);
	if(res.type != TINT)
		error("setproc(pid): arg type");

	sproc(res.store.u.ival);
}

void
filepc(Node *r, Node *args)
{
	int i;
	Node res;
	char *p, c;
	u64int v;

	if(args == 0)
		error("filepc(filename:line): arg count");
	expr(args, &res);
	if(res.type != TSTRING)
		error("filepc(filename:line): arg type");

	p = strchr(res.store.u.string->string, ':');
	if(p == 0)
		error("filepc(filename:line): bad arg format");

	c = *p;
	*p++ = '\0';
	i = file2pc(res.store.u.string->string, atoi(p), &v);
	p[-1] = c;
	if(i < 0)
		error("filepc(filename:line): can't find address");

	r->op = OCONST;
	r->type = TINT;
	r->store.fmt = 'D';
	r->store.u.ival = v;
}

void
interpret(Node *r, Node *args)
{
	Node res;
	int isave;

	if(args == 0)
		error("interpret(string): arg count");
	expr(args, &res);
	if(res.type != TSTRING)
		error("interpret(string): arg type");

	pushstr(&res);

	isave = interactive;
	interactive = 0;
	r->store.u.ival = yyparse();
	interactive = isave;
	popio();
	r->op = OCONST;
	r->type = TINT;
	r->store.fmt = 'D';
}

void
include(Node *r, Node *args)
{
	char *file, *libfile;
	static char buf[1024];
	Node res;
	int isave;

	if(args == 0)
		error("include(string): arg count");
	expr(args, &res);
	if(res.type != TSTRING)
		error("include(string): arg type");

	Bflush(bout);

	libfile = nil;
	file = res.store.u.string->string;
	if(access(file, AREAD) < 0 && file[0] != '/'){
		snprint(buf, sizeof buf, "#9/acid/%s", file);
		libfile = unsharp(buf);
		if(access(libfile, AREAD) >= 0){
			strecpy(buf, buf+sizeof buf, libfile);
			file = buf;
		}
		free(libfile);
	}

	pushfile(file);
	isave = interactive;
	interactive = 0;
	r->store.u.ival = yyparse();
	interactive = isave;
	popio();
	r->op = OCONST;
	r->type = TINT;
	r->store.fmt = 'D';
}

void
includepipe(Node *r, Node *args)
{
	Node res;
	int i, isave, pid, pip[2];
	char *argv[4];
	Waitmsg *w;

	USED(r);
	if(args == 0)
		error("includepipe(string): arg count");
	expr(args, &res);
	if(res.type != TSTRING)
		error("includepipe(string): arg type");

	Bflush(bout);

	argv[0] = "rc";
	argv[1] = "-c";
	argv[2] = res.store.u.string->string;
	argv[3] = 0;

	if(pipe(pip) < 0)
		error("pipe: %r");

	pid = fork();
	switch(pid) {
	case -1:
		close(pip[0]);
		close(pip[1]);
		error("fork: %r");
	case 0:
		close(pip[0]);
		close(0);
		open("/dev/null", OREAD);
		dup(pip[1], 1);
		if(pip[1] > 1)
			close(pip[1]);
		for(i=3; i<100; i++)
			close(i);
		exec("rc", argv);
		sysfatal("exec rc: %r");
	}

	close(pip[1]);
	pushfd(pip[0]);

	isave = interactive;
	interactive = 0;
	r->store.u.ival = yyparse();
	interactive = isave;
	popio();

	r->op = OCONST;
	r->type = TINT;
	r->store.fmt = 'D';

	w = waitfor(pid);
	if(w->msg && w->msg[0])
		error("includepipe(\"%s\"): %s", argv[2], w->msg);	/* leaks w */
	free(w);
}

void
rc(Node *r, Node *args)
{
	Node res;
	int pid;
	char *p, *q, *argv[4];
	Waitmsg *w;

	USED(r);
	if(args == 0)
		error("rc(string): arg count");
	expr(args, &res);
	if(res.type != TSTRING)
		error("rc(string): arg type");

	argv[0] = "rc";
	argv[1] = "-c";
	argv[2] = res.store.u.string->string;
	argv[3] = 0;

	pid = fork();
	switch(pid) {
	case -1:
		error("fork %r");
	case 0:
		exec("rc", argv);
		exits(0);
	default:
		w = waitfor(pid);
		break;
	}
	p = w->msg;
	q = strrchr(p, ':');
	if (q)
		p = q+1;

	r->op = OCONST;
	r->type = TSTRING;
	r->store.u.string = strnode(p);
	free(w);
	r->store.fmt = 's';
}

void
doerror(Node *r, Node *args)
{
	Node res;

	USED(r);
	if(args == 0)
		error("error(string): arg count");
	expr(args, &res);
	if(res.type != TSTRING)
		error("error(string): arg type");

	error(res.store.u.string->string);
}

void
doaccess(Node *r, Node *args)
{
	Node res;

	if(args == 0)
		error("access(filename): arg count");
	expr(args, &res);
	if(res.type != TSTRING)
		error("access(filename): arg type");

	r->op = OCONST;
	r->type = TINT;
	r->store.fmt = 'D';
	r->store.u.ival = 0;
	if(access(res.store.u.string->string, 4) == 0)
		r->store.u.ival = 1;
}

void
readfile(Node *r, Node *args)
{
	Node res;
	int n, fd;
	char *buf;
	Dir *db;

	if(args == 0)
		error("readfile(filename): arg count");
	expr(args, &res);
	if(res.type != TSTRING)
		error("readfile(filename): arg type");

	fd = open(res.store.u.string->string, OREAD);
	if(fd < 0)
		return;

	db = dirfstat(fd);
	if(db == nil || db->length == 0)
		n = 8192;
	else
		n = db->length;
	free(db);

	buf = malloc(n);
	n = read(fd, buf, n);

	if(n > 0) {
		r->op = OCONST;
		r->type = TSTRING;
		r->store.u.string = strnodlen(buf, n);
		r->store.fmt = 's';
	}
	free(buf);
	close(fd);
}

void
getfile(Node *r, Node *args)
{
	int n;
	char *p;
	Node res;
	String *s;
	Biobuf *bp;
	List **l, *new;

	if(args == 0)
		error("file(filename): arg count");
	expr(args, &res);
	if(res.type != TSTRING)
		error("file(filename): arg type");

	r->op = OCONST;
	r->type = TLIST;
	r->store.u.l = 0;

	p = res.store.u.string->string;
	bp = Bopen(p, OREAD);
	if(bp == 0)
		return;

	l = &r->store.u.l;
	for(;;) {
		p = Brdline(bp, '\n');
		n = Blinelen(bp);
		if(p == 0) {
			if(n == 0)
				break;
			s = strnodlen(0, n);
			Bread(bp, s->string, n);
		}
		else
			s = strnodlen(p, n-1);

		new = al(TSTRING);
		new->store.u.string = s;
		new->store.fmt = 's';
		*l = new;
		l = &new->next;
	}
	Bterm(bp);
}

void
cvtatof(Node *r, Node *args)
{
	Node res;

	if(args == 0)
		error("atof(string): arg count");
	expr(args, &res);
	if(res.type != TSTRING)
		error("atof(string): arg type");

	r->op = OCONST;
	r->type = TFLOAT;
	r->store.u.fval = atof(res.store.u.string->string);
	r->store.fmt = 'f';
}

void
cvtatoi(Node *r, Node *args)
{
	Node res;

	if(args == 0)
		error("atoi(string): arg count");
	expr(args, &res);
	if(res.type != TSTRING)
		error("atoi(string): arg type");

	r->op = OCONST;
	r->type = TINT;
	r->store.u.ival = strtoull(res.store.u.string->string, 0, 0);
	r->store.fmt = 'D';
}

void
cvtitoa(Node *r, Node *args)
{
	Node res;
	Node *av[Maxarg];
	s64int ival;
	char buf[128], *fmt;

	if(args == 0)
err:
		error("itoa(number [, printformat]): arg count");
	na = 0;
	flatten(av, args);
	if(na == 0 || na > 2)
		goto err;
	expr(av[0], &res);
	if(res.type != TINT)
		error("itoa(integer): arg type");
	ival = (int)res.store.u.ival;
	fmt = "%d";
	if(na == 2){
		expr(av[1], &res);
		if(res.type != TSTRING)
			error("itoa(integer, string): arg type");
		fmt = res.store.u.string->string;
	}

	sprint(buf, fmt, ival);
	r->op = OCONST;
	r->type = TSTRING;
	r->store.u.string = strnode(buf);
	r->store.fmt = 's';
}

List*
mapent(Map *m)
{
	int i;
	List *l, *n, **t, *h;

	h = 0;
	t = &h;
	for(i = 0; i < m->nseg; i++) {
		l = al(TSTRING);
		n = al(TLIST);
		n->store.u.l = l;
		*t = n;
		t = &n->next;
		l->store.u.string = strnode(m->seg[i].name);
		l->store.fmt = 's';
		l->next = al(TSTRING);
		l = l->next;
		l->store.u.string = strnode(m->seg[i].file ? m->seg[i].file : "");
		l->store.fmt = 's';
		l->next = al(TINT);
		l = l->next;
		l->store.u.ival = m->seg[i].base;
		l->store.fmt = 'X';
		l->next = al(TINT);
		l = l->next;
		l->store.u.ival = m->seg[i].base + m->seg[i].size;
		l->store.fmt = 'X';
		l->next = al(TINT);
		l = l->next;
		l->store.u.ival = m->seg[i].offset;
		l->store.fmt = 'X';
	}
	return h;
}

void
map(Node *r, Node *args)
{
	int i;
	Map *m;
	List *l;
	char *nam, *fil;
	Node *av[Maxarg], res;

	na = 0;
	flatten(av, args);

	if(na != 0) {
		expr(av[0], &res);
		if(res.type != TLIST)
			error("map(list): map needs a list");
		if(listlen(res.store.u.l) != 5)
			error("map(list): list must have 5 entries");

		l = res.store.u.l;
		if(l->type != TSTRING)
			error("map name must be a string");
		nam = l->store.u.string->string;
		l = l->next;
		if(l->type != TSTRING)
			error("map file must be a string");
		fil = l->store.u.string->string;
		m = symmap;
		i = findseg(m, nam, fil);
		if(i < 0) {
			m = cormap;
			i = findseg(m, nam, fil);
		}
		if(i < 0)
			error("%s %s is not a map entry", nam, fil);
		l = l->next;
		if(l->type != TINT)
			error("map entry not int");
		m->seg[i].base = l->store.u.ival;
/*
		if (strcmp(ent, "text") == 0)
			textseg(l->store.u.ival, &fhdr);
*/
		l = l->next;
		if(l->type != TINT)
			error("map entry not int");
		m->seg[i].size = l->store.u.ival - m->seg[i].base;
		l = l->next;
		if(l->type != TINT)
			error("map entry not int");
		m->seg[i].offset = l->store.u.ival;
	}

	r->type = TLIST;
	r->store.u.l = 0;
	if(symmap)
		r->store.u.l = mapent(symmap);
	if(cormap) {
		if(r->store.u.l == 0)
			r->store.u.l = mapent(cormap);
		else {
			for(l = r->store.u.l; l->next; l = l->next)
				;
			l->next = mapent(cormap);
		}
	}
}

void
flatten(Node **av, Node *n)
{
	if(n == 0)
		return;

	switch(n->op) {
	case OLIST:
		flatten(av, n->left);
		flatten(av, n->right);
		break;
	default:
		av[na++] = n;
		if(na >= Maxarg)
			error("too many function arguments");
		break;
	}
}

static struct
{
	char *name;
	u64int val;
} sregs[Maxarg/2];
static int nsregs;

static int
straceregrw(Regs *regs, char *name, u64int *val, int isr)
{
	int i;

	if(!isr){
		werrstr("saved registers cannot be written");
		return -1;
	}
	for(i=0; i<nsregs; i++)
		if(strcmp(sregs[i].name, name) == 0){
			*val = sregs[i].val;
			return 0;
		}
	return rget(acidregs, name, val);
}

void
strace(Node *r, Node *args)
{
	Node *av[Maxarg], res;
	List *l;
	Regs regs;

	na = 0;
	flatten(av, args);

	if(na != 1)
		error("strace(list): want one arg");

	expr(av[0], &res);
	if(res.type != TLIST)
		error("strace(list): strace needs a list");
	l = res.store.u.l;
	if(listlen(l)%2)
		error("strace(list): strace needs an even-length list");
	for(nsregs=0; l; nsregs++){
		if(l->type != TSTRING)
			error("strace({r,v,r,v,...}): non-string name");
		sregs[nsregs].name = l->store.u.string->string;
		if(regdesc(sregs[nsregs].name) == nil)
			error("strace: bad register '%s'", sregs[nsregs].name);
		l = l->next;

		if(l == nil)
			error("cannot happen in strace");
		if(l->type != TINT)
			error("strace: non-int value for %s", sregs[nsregs].name);
		sregs[nsregs].val = l->store.u.ival;
		l = l->next;
	}
	regs.rw = straceregrw;

	tracelist = 0;
	if(stacktrace(cormap, &regs, trlist) <= 0)
		error("no stack frame");
	r->type = TLIST;
	r->store.u.l = tracelist;
}

void
regerror(char *msg)
{
	error(msg);
}

void
regexp(Node *r, Node *args)
{
	Node res;
	Reprog *rp;
	Node *av[Maxarg];

	na = 0;
	flatten(av, args);
	if(na != 2)
		error("regexp(pattern, string): arg count");
	expr(av[0], &res);
	if(res.type != TSTRING)
		error("regexp(pattern, string): pattern must be string");
	rp = regcomp(res.store.u.string->string);
	if(rp == 0)
		return;

	expr(av[1], &res);
	if(res.type != TSTRING)
		error("regexp(pattern, string): bad string");

	r->store.fmt = 'D';
	r->type = TINT;
	r->store.u.ival = regexec(rp, res.store.u.string->string, 0, 0);
	free(rp);
}

char vfmt[] = "aBbcCdDfFgGiIoOqQrRsSuUVxXYZ";

void
fmt(Node *r, Node *args)
{
	Node res;
	Node *av[Maxarg];

	na = 0;
	flatten(av, args);
	if(na != 2)
		error("fmt(obj, fmt): arg count");
	expr(av[1], &res);
	if(res.type != TINT || strchr(vfmt, res.store.u.ival) == 0)
		error("fmt(obj, fmt): bad format '%c'", (char)res.store.u.ival);
	expr(av[0], r);
	r->store.fmt = res.store.u.ival;
}

void
patom(char type, Store *res)
{
	int i;
	char buf[512];
	extern char *typenames[];
	Node *n;

	switch(type){
	case TREG:
		if(res->u.reg.thread)
			Bprint(bout, "register(\"%s\", %#ux)", res->u.reg.name, res->u.reg.thread);
		else
			Bprint(bout, "register(\"%s\")", res->u.reg.name);
		return;
	case TCON:
		Bprint(bout, "refconst(");
		n = res->u.con;
		patom(n->type, &n->store);
		Bprint(bout, ")");
		return;
	}

	switch(res->fmt){
	case 'c':
	case 'C':
	case 'r':
	case 'B':
	case 'b':
	case 'X':
	case 'x':
	case 'W':
	case 'D':
	case 'd':
	case 'u':
	case 'U':
	case 'Z':
	case 'V':
	case 'Y':
	case 'o':
	case 'O':
	case 'q':
	case 'Q':
	case 'a':
	case 'A':
	case 'I':
	case 'i':
		if(type != TINT){
		badtype:
			Bprint(bout, "*%s\\%c*", typenames[(uchar)type], res->fmt);
			return;
		}
		break;

	case 'f':
	case 'F':
		if(type != TFLOAT)
			goto badtype;
		break;

	case 's':
	case 'g':
	case 'G':
	case 'R':
		if(type != TSTRING)
			goto badtype;
		break;
	}

	switch(res->fmt) {
	case 'c':
		Bprint(bout, "%c", (int)res->u.ival);
		break;
	case 'C':
		if(res->u.ival < ' ' || res->u.ival >= 0x7f)
			Bprint(bout, "%3d", (int)res->u.ival&0xff);
		else
			Bprint(bout, "%3c", (int)res->u.ival);
		break;
	case 'r':
		Bprint(bout, "%C", (int)res->u.ival);
		break;
	case 'B':
		memset(buf, '0', 34);
		buf[1] = 'b';
		for(i = 0; i < 32; i++) {
			if(res->u.ival & (1<<i))
				buf[33-i] = '1';
		}
		buf[35] = '\0';
		Bprint(bout, "%s", buf);
		break;
	case 'b':
		Bprint(bout, "%#.2x", (int)res->u.ival&0xff);
		break;
	case 'X':
		Bprint(bout, "%#.8lux", (ulong)res->u.ival);
		break;
	case 'x':
		Bprint(bout, "%#.4lux", (ulong)res->u.ival&0xffff);
		break;
	case 'W':
		Bprint(bout, "%#.16llux", res->u.ival);
		break;
	case 'D':
		Bprint(bout, "%d", (int)res->u.ival);
		break;
	case 'd':
		Bprint(bout, "%d", (ushort)res->u.ival);
		break;
	case 'u':
		Bprint(bout, "%d", (int)res->u.ival&0xffff);
		break;
	case 'U':
		Bprint(bout, "%lud", (ulong)res->u.ival);
		break;
	case 'Z':
		Bprint(bout, "%llud", res->u.ival);
		break;
	case 'V':
		Bprint(bout, "%lld", res->u.ival);
		break;
	case 'Y':
		Bprint(bout, "%#.16llux", res->u.ival);
		break;
	case 'o':
		Bprint(bout, "%#.11uo", (int)res->u.ival&0xffff);
		break;
	case 'O':
		Bprint(bout, "%#.6uo", (int)res->u.ival);
		break;
	case 'q':
		Bprint(bout, "%#.11o", (short)(res->u.ival&0xffff));
		break;
	case 'Q':
		Bprint(bout, "%#.6o", (int)res->u.ival);
		break;
	case 'f':
	case 'F':
		Bprint(bout, "%g", res->u.fval);
		break;
	case 's':
	case 'g':
	case 'G':
		Bwrite(bout, res->u.string->string, res->u.string->len);
		break;
	case 'R':
		Bprint(bout, "%S", (Rune*)res->u.string->string);
		break;
	case 'a':
	case 'A':
		symoff(buf, sizeof(buf), res->u.ival, CANY);
		Bprint(bout, "%s", buf);
		break;
	case 'I':
	case 'i':
		if (symmap == nil || (*mach->das)(symmap, res->u.ival, res->fmt, buf, sizeof(buf)) < 0)
			Bprint(bout, "no instruction");
		else
			Bprint(bout, "%s", buf);
		break;
	}
}

void
blprint(List *l)
{
	Store *res;

	Bprint(bout, "{");
	while(l) {
		switch(l->type) {
		case TINT:
			res = &l->store;
			if(res->fmt == 'c'){
				Bprint(bout, "\'%c\'", (int)res->u.ival);
				break;
			}else if(res->fmt == 'r'){
				Bprint(bout, "\'%C\'", (int)res->u.ival);
				break;
			}
			/* fall through */
		default:
			patom(l->type, &l->store);
			break;
		case TSTRING:
			Bputc(bout, '"');
			patom(l->type, &l->store);
			Bputc(bout, '"');
			break;
		case TLIST:
			blprint(l->store.u.l);
			break;
		case TCODE:
			pcode(l->store.u.cc, 0);
			break;
		}
		l = l->next;
		if(l)
			Bprint(bout, ", ");
	}
	Bprint(bout, "}");
}

int
comx(Node res)
{
	Lsym *sl;
	Node *n, xx;

	if(res.store.fmt != 'a' && res.store.fmt != 'A')
		return 0;

	if(res.store.comt == 0 || res.store.comt->base == 0)
		return 0;

	sl = res.store.comt->base;
	if(sl->proc) {
		res.left = ZN;
		res.right = ZN;
		n = an(ONAME, ZN, ZN);
		n->sym = sl;
		n = an(OCALL, n, &res);
			n->left->sym = sl;
		expr(n, &xx);
		return 1;
	}
	print("(%s)", sl->name);
	return 0;
}

void
bprint(Node *r, Node *args)
{
	int i, nas;
	Node res, *av[Maxarg];

	USED(r);
	na = 0;
	flatten(av, args);
	nas = na;
	for(i = 0; i < nas; i++) {
		expr(av[i], &res);
		switch(res.type) {
		default:
			if(comx(res))
				break;
			patom(res.type, &res.store);
			break;
		case TCODE:
			pcode(res.store.u.cc, 0);
			break;
		case TLIST:
			blprint(res.store.u.l);
			break;
		}
	}
	if(ret == 0)
		Bputc(bout, '\n');
}

void
printto(Node *r, Node *args)
{
	int fd;
	Biobuf *b;
	int i, nas;
	Node res, *av[Maxarg];

	USED(r);
	na = 0;
	flatten(av, args);
	nas = na;

	expr(av[0], &res);
	if(res.type != TSTRING)
		error("printto(string, ...): need string");

	fd = create(res.store.u.string->string, OWRITE, 0666);
	if(fd < 0)
		fd = open(res.store.u.string->string, OWRITE);
	if(fd < 0)
		error("printto: open %s: %r", res.store.u.string->string);

	b = gmalloc(sizeof(Biobuf));
	Binit(b, fd, OWRITE);

	Bflush(bout);
	io[iop++] = bout;
	bout = b;

	for(i = 1; i < nas; i++) {
		expr(av[i], &res);
		switch(res.type) {
		default:
			if(comx(res))
				break;
			patom(res.type, &res.store);
			break;
		case TLIST:
			blprint(res.store.u.l);
			break;
		}
	}
	if(ret == 0)
		Bputc(bout, '\n');

	Bterm(b);
	close(fd);
	free(b);
	bout = io[--iop];
}

void
pcfile(Node *r, Node *args)
{
	Node res;
	char *p, buf[128];

	if(args == 0)
		error("pcfile(addr): arg count");
	expr(args, &res);
	if(res.type != TINT)
		error("pcfile(addr): arg type");

	r->type = TSTRING;
	r->store.fmt = 's';
	if(fileline(res.store.u.ival, buf, sizeof(buf)) < 0) {
		r->store.u.string = strnode("?file?");
		return;
	}
	p = strrchr(buf, ':');
	if(p == 0)
		error("pcfile(addr): funny file %s", buf);
	*p = '\0';
	r->store.u.string = strnode(buf);
}

void
pcline(Node *r, Node *args)
{
	Node res;
	char *p, buf[128];

	if(args == 0)
		error("pcline(addr): arg count");
	expr(args, &res);
	if(res.type != TINT)
		error("pcline(addr): arg type");

	r->type = TINT;
	r->store.fmt = 'D';
	if(fileline(res.store.u.ival, buf, sizeof(buf)) < 0) {
		r->store.u.ival = 0;
		return;
	}

	p = strrchr(buf, ':');
	if(p == 0)
		error("pcline(addr): funny file %s", buf);
	r->store.u.ival = atoi(p+1);
}

void
textfile(Node *r, Node *args)
{
	char *file;
	long base;
	Fhdr *fp;
	Node res, *av[Maxarg];
	List *l, *l2, **tail, *list, *tl;

	na = 0;
	flatten(av, args);

	if(na != 0) {
		expr(av[0], &res);
		if(res.type != TLIST)
			error("textfile(list): textfile needs a list");
		if(listlen(res.store.u.l) != 2)
			error("textfile(list): list must have 2 entries");

		l = res.store.u.l;
		if(l->type != TSTRING)
			error("textfile name must be a string");
		file = l->store.u.string->string;

		l = l->next;
		if(l->type != TINT)
			error("textfile base must be an int");
		base = l->store.u.ival;

		if((fp = crackhdr(file, OREAD)) == nil)
			error("crackhdr %s: %r", file);
		Bflush(bout);
		fp->base = base;
		fprint(2, "%s: %s %s %s\n", file, fp->aname, fp->mname, fp->fname);
		if(mapfile(fp, base, symmap, nil) < 0)
			fprint(2, "mapping %s: %r\n", file);
		if(corhdr){
			unmapfile(corhdr, cormap);
			mapfile(fp, base, cormap, nil);
			free(correg);
			correg = nil;
			mapfile(corhdr, 0, cormap, &correg);
		}
		if(symopen(fp) < 0)
			fprint(2, "symopen %s: %r\n", file);
		else
			addvarsym(fp);
		return;
	}

	l2 = nil;
	tail = &l2;
	for(fp=fhdrlist; fp; fp=fp->next){
		if(fp->ftype == FCORE)
			continue;
		tl = al(TLIST);
		*tail = tl;
		tail = &tl->next;

		list = al(TSTRING);
		tl->store.u.l = list;
		list->store.u.string = strnode(fp->filename);
		list->store.fmt = 's';
		list->next = al(TINT);
		list = list->next;
		list->store.fmt = 'X';
		list->store.u.ival = fp->base;
	}

	r->type = TLIST;
	r->store.u.l = l2;
}

void
deltextfile(Node *r, Node *args)
{
	int did;
	char *file;
	Fhdr *fp, *fpnext;
	Node res, *av[Maxarg];

	na = 0;
	flatten(av, args);

	if(na != 1)
		error("deltextfile(string): arg count");

	expr(av[0], &res);
	if(res.type != TSTRING)
		error("deltextfile(string): arg type");
	file = res.store.u.string->string;

	did = 0;
	for(fp=fhdrlist; fp; fp=fpnext){
		fpnext = fp->next;
		if(fp->ftype == FCORE)
			continue;
		if(strcmp(file, fp->filename) == 0){
			did = 1;
			if(fp == symhdr)
				error("cannot remove symbols from main text file");
			unmapfile(fp, symmap);
			uncrackhdr(fp);
		}
	}

	delvarsym(file);
	if(!did)
		error("symbol file %s not open", file);
}

void
stringn(Node *r, Node *args)
{
	uint addr;
	int i, n, ret;
	Node res, *av[Maxarg];
	char *buf;

	na = 0;
	flatten(av, args);
	if(na != 2)
		error("stringn(addr, n): arg count");

	expr(av[0], &res);
	if(res.type != TINT)
		error("stringn(addr, n): arg type");
	addr = res.store.u.ival;

	expr(av[1], &res);
	if(res.type != TINT)
		error("stringn(addr,n): arg type");
	n = res.store.u.ival;

	buf = malloc(n+1);
	if(buf == nil)
		error("out of memory");

	r->type = TSTRING;
	for(i=0; i<n; i++){
		ret = get1(cormap, addr, (uchar*)&buf[i], 1);
		if(ret < 0){
			free(buf);
			error("indir: %r");
		}
		addr++;
	}
	buf[n] = 0;
	r->store.u.string = strnode(buf);
	free(buf);
}
