#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#define Extern
#include "acid.h"
#include "y.tab.h"

extern int __ifmt(Fmt*);

static Biobuf	bioout;
static char*	lm[16];
static int	nlm;
static char*	mtype;

static	int attachfiles(int, char**);
int	xfmt(Fmt*);
int	isnumeric(char*);
void	die(void);
void	setcore(Fhdr*);

void
usage(void)
{
	fprint(2, "usage: acid [-c core] [-l module] [-m machine] [-qrw] [-k] [pid] [file]\n");
	exits("usage");
}

Map*
dumbmap(int fd)
{
	Map *dumb;
	Seg s;

	dumb = allocmap();
	memset(&s, 0, sizeof s);
	s.fd = fd;
	s.base = 0;
	s.offset = 0;
	s.size = 0xFFFFFFFF;
	s.name = "data";
	s.file = "<dumb>";
	if(addseg(dumb, s) < 0){
		freemap(dumb);
		return nil;
	}
	if(mach == nil)
		mach = machcpu;
	return dumb;
}

void
main(int argc, char *argv[])
{
	Lsym *volatile l;
	Node *n;
	char buf[128], *s;
	int pid, i;

	argv0 = argv[0];
	pid = 0;
	quiet = 1;

	mtype = 0;
	ARGBEGIN{
	case 'A':
		abort();
		break;
	case 'm':
		mtype = ARGF();
		break;
	case 'w':
		wtflag = 1;
		break;
	case 'l':
		s = ARGF();
		if(s == 0)
			usage();
		lm[nlm++] = s;
		break;
	case 'k':
		kernel++;
		break;
	case 'q':
		quiet = 0;
		break;
	case 'r':
		pid = 1;
		remote++;
		kernel++;
		break;
	default:
		usage();
	}ARGEND

	USED(pid);

	fmtinstall('Z', Zfmt);
	fmtinstall('L', locfmt);
	Binit(&bioout, 1, OWRITE);
	bout = &bioout;

	initexpr();
	initprint();
	kinit();
	initialising = 1;
	pushfile(0);
	loadvars();
	installbuiltin();
	acidregs = mallocz(sizeof *acidregs, 1);
	acidregs->rw = acidregsrw;

	if(mtype && machbyname(mtype) == 0)
		print("unknown machine %s", mtype);

	if (attachfiles(argc, argv) < 0)
		varreg();		/* use default register set on error */
	if(mach == nil)
		mach = machcpu;

	symhdr = nil;	/* not supposed to use this anymore */

	l = mkvar("acid");
	l->v->set = 1;
	l->v->type = TLIST;
	l->v->store.u.l = nil;

	loadmodule(unsharp("#9/acid/port"));
	for(i = 0; i < nlm; i++) {
		if(access(lm[i], AREAD) >= 0)
			loadmodule(lm[i]);
		else {
			sprint(buf, "#9/acid/%s", lm[i]);
			loadmodule(unsharp(buf));
		}
	}

	userinit();
	varsym();

	l = look("acidmap");
	if(l && l->proc) {
		if(setjmp(err) == 0){
			n = an(ONAME, ZN, ZN);
			n->sym = l;
			n = an(OCALL, n, ZN);
			execute(n);
		}
	}

	interactive = 1;
	initialising = 0;
	line = 1;

	notify(catcher);

	for(;;) {
		if(setjmp(err)) {
			Binit(&bioout, 1, OWRITE);
			unwind();
		}
		stacked = 0;

		Bprint(bout, "acid; ");

		if(yyparse() != 1)
			die();
		restartio();

		unwind();
	}
/*
	Bputc(bout, '\n');
	exits(0);
*/
}

void
setstring(char *var, char *s)
{
	Lsym *l;
	Value *v;

	l = mkvar(var);
	v = l->v;
	v->store.fmt = 's';
	v->set = 1;
	v->store.u.string = strnode(s ? s : "");
	v->type = TSTRING;
}

static int
attachfiles(int argc, char **argv)
{
	volatile int pid;
	Lsym *l;

	pid = 0;
	interactive = 0;
	USED(pid);

	if(setjmp(err))
		return -1;

	attachargs(argc, argv, wtflag?ORDWR:OREAD, 1);

	setstring("objtype", mach->name);
	setstring("textfile", symfil);
	setstring("systype", symhdr ? symhdr->aname : "");
	setstring("corefile", corfil);

	l = mkvar("pids");
	l->v->set = 1;
	l->v->type = TLIST;
	l->v->store.u.l = nil;

	if(corpid)
		sproc(corpid);
	if(corhdr)
		setcore(corhdr);
	varreg();
	return 0;
}

void
setcore(Fhdr *hdr)
{
	int i;
	Lsym *l;
	Value *v;
	List **tail, *tl;

	unmapproc(cormap);
	unmapfile(corhdr, cormap);
	free(correg);
	correg = nil;

	if(hdr == nil)
		error("no core");
	if(mapfile(hdr, 0, cormap, &correg) < 0)
		error("mapfile %s: %r", hdr->filename);
	corhdr = hdr;
	corfil = hdr->filename;

	l = mkvar("pid");
	v = l->v;
	v->store.fmt = 'D';
	v->set = 1;
	v->store.u.ival = hdr->pid;

	setstring("corefile", corfil);
	setstring("cmdline", hdr->cmdline);

	l = mkvar("pids");
	l->v->set = 1;
	l->v->type = TLIST;
	l->v->store.u.l = nil;
	tail = &l->v->store.u.l;
	for(i=0; i<hdr->nthread; i++){
		tl = al(TINT);
		tl->store.u.ival = hdr->thread[i].id;
		tl->store.fmt = 'X';
		*tail = tl;
		tail = &tl->next;
	}

	if(hdr->nthread)
		sproc(hdr->thread[0].id);
}

void
die(void)
{
	Lsym *s;
	List *f;
	int first;

	Bprint(bout, "\n");

	first = 1;
	s = look("proclist");
	if(s && s->v->type == TLIST) {
		for(f = s->v->store.u.l; f; f = f->next){
			detachproc((int)f->store.u.ival);
			Bprint(bout, "%s %d", first ? "/bin/kill -9" : "", (int)f->store.u.ival);
			first = 0;
		}
	}
	if(!first)
		Bprint(bout, "\n");
	exits(0);
}

void
userinit(void)
{
	Lsym *l;
	Node *n;
	char buf[128], *p;

	sprint(buf, "#9/acid/%s", mach->name);
	loadmodule(unsharp(buf));
	p = getenv("HOME");
	if(p != 0) {
		sprint(buf, "%s/lib/acid", p);
		silent = 1;
		loadmodule(buf);
	}

	interactive = 0;
	if(setjmp(err)) {
		unwind();
		return;
	}
	l = look("acidinit");
	if(l && l->proc) {
		n = an(ONAME, ZN, ZN);
		n->sym = l;
		n = an(OCALL, n, ZN);
		execute(n);
	}
}

void
loadmodule(char *s)
{
	interactive = 0;
	if(setjmp(err)) {
		unwind();
		return;
	}
	pushfile(s);
	silent = 0;
	yyparse();
	popio();
	return;
}

Node*
an(int op, Node *l, Node *r)
{
	Node *n;

	n = gmalloc(sizeof(Node));
	memset(n, 0, sizeof(Node));
	n->gc.gclink = gcl;
	gcl = (Gc*)n;
	n->op = op;
	n->left = l;
	n->right = r;
	return n;
}

List*
al(int t)
{
	List *l;

	l = gmalloc(sizeof(List));
	memset(l, 0, sizeof(List));
	l->type = t;
	l->gc.gclink = gcl;
	gcl = (Gc*)l;
	return l;
}

Node*
con(s64int v)
{
	Node *n;

	n = an(OCONST, ZN, ZN);
	n->store.u.ival = v;
	n->store.fmt = 'X';
	n->type = TINT;
	return n;
}

void
fatal(char *fmt, ...)
{
	char buf[128];
	va_list arg;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	fprint(2, "%s: %Z (fatal problem) %s\n", argv0, buf);
	exits(buf);
}

void
yyerror(char *fmt, ...)
{
	char buf[128];
	va_list arg;

	if(strcmp(fmt, "syntax error") == 0) {
		yyerror("syntax error, near symbol '%s'", symbol);
		return;
	}
	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	print("%Z: %s\n", buf);
}

void
marktree(Node *n)
{

	if(n == 0)
		return;

	marktree(n->left);
	marktree(n->right);

	n->gc.gcmark = 1;
	if(n->op != OCONST)
		return;

	switch(n->type) {
	case TSTRING:
		n->store.u.string->gc.gcmark = 1;
		break;
	case TLIST:
		marklist(n->store.u.l);
		break;
	case TCODE:
		marktree(n->store.u.cc);
		break;
	}
}

void
marklist(List *l)
{
	while(l) {
		l->gc.gcmark = 1;
		switch(l->type) {
		case TSTRING:
			l->store.u.string->gc.gcmark = 1;
			break;
		case TLIST:
			marklist(l->store.u.l);
			break;
		case TCODE:
			marktree(l->store.u.cc);
			break;
		}
		l = l->next;
	}
}

void
gc(void)
{
	int i;
	Lsym *f;
	Value *v;
	Gc *m, **p, *next;

	if(dogc < Mempergc)
		return;
	dogc = 0;

	/* Mark */
	for(m = gcl; m; m = m->gclink)
		m->gcmark = 0;

	/* Scan */
	for(i = 0; i < Hashsize; i++) {
		for(f = hash[i]; f; f = f->hash) {
			marktree(f->proc);
			if(f->lexval != Tid)
				continue;
			for(v = f->v; v; v = v->pop) {
				switch(v->type) {
				case TSTRING:
					v->store.u.string->gc.gcmark = 1;
					break;
				case TLIST:
					marklist(v->store.u.l);
					break;
				case TCODE:
					marktree(v->store.u.cc);
					break;
				case TCON:
					marktree(v->store.u.con);
					break;
				}
			}
		}
	}

	/* Free */
	p = &gcl;
	for(m = gcl; m; m = next) {
		next = m->gclink;
		if(m->gcmark == 0) {
			*p = next;
			free(m);	/* Sleazy reliance on my malloc */
		}
		else
			p = &m->gclink;
	}
}

void*
gmalloc(long l)
{
	void *p;

	dogc += l;
	p = malloc(l);
	if(p == 0)
		fatal("out of memory");
	return p;
}

void
checkqid(int f1, int pid)
{
	int fd;
	Dir *d1, *d2;
	char buf[128];

	if(kernel)
		return;

	d1 = dirfstat(f1);
	if(d1 == nil){
		print("checkqid: (qid not checked) dirfstat: %r\n");
		return;
	}

	sprint(buf, "/proc/%d/text", pid);
	fd = open(buf, OREAD);
	if(fd < 0 || (d2 = dirfstat(fd)) == nil){
		print("checkqid: (qid not checked) dirstat %s: %r\n", buf);
		free(d1);
		if(fd >= 0)
			close(fd);
		return;
	}

	close(fd);

	if(d1->qid.path != d2->qid.path || d1->qid.vers != d2->qid.vers || d1->qid.type != d2->qid.type){
		print("path %#llux %#llux vers %lud %lud type %d %d\n",
			d1->qid.path, d2->qid.path, d1->qid.vers, d2->qid.vers, d1->qid.type, d2->qid.type);
		print("warning: image does not match text for pid %d\n", pid);
	}
	free(d1);
	free(d2);
}

void
catcher(void *junk, char *s)
{
	USED(junk);

	if(strstr(s, "interrupt")) {
		gotint = 1;
		noted(NCONT);
	}
	if(strstr(s, "child"))
		noted(NCONT);
fprint(2, "note: %s\n", s);
	noted(NDFLT);
}

char*
system(void)
{
	char *cpu, *p, *q;
	static char kernel[128];

	cpu = getenv("cputype");
	if(cpu == 0) {
		cpu = "mips";
		print("$cputype not set; assuming %s\n", cpu);
	}
	p = getenv("terminal");
	if(p == 0 || (p=strchr(p, ' ')) == 0 || p[1] == ' ' || p[1] == 0) {
		p = "9power";
		print("missing or bad $terminal; assuming %s\n", p);
	}
	else{
		p++;
		q = strchr(p, ' ');
		if(q)
			*q = 0;
		sprint(kernel, "/%s/9%s", cpu, p);
	}
	return kernel;
}

int
isnumeric(char *s)
{
	while(*s) {
		if(*s < '0' || *s > '9')
			return 0;
		s++;
	}
	return 1;
}
