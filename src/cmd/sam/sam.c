#include "sam.h"

Rune	genbuf[BLOCKSIZE];
int	io;
int	panicking;
int	rescuing;
String	genstr;
String	rhs;
String	curwd;
String	cmdstr;
Rune	empty[] = { 0 };
char	*genc;
File	*curfile;
File	*flist;
File	*cmd;
jmp_buf	mainloop;
List	tempfile = { 'p' };
int	quitok = TRUE;
int	downloaded;
int	dflag;
int	Rflag;
char	*machine;
char	*home;
int	bpipeok;
int	termlocked;
char	*samterm = SAMTERM;
char	*rsamname = RSAM;
File	*lastfile;
Disk	*disk;
long	seq;

char *winsize;

Rune	baddir[] = { '<', 'b', 'a', 'd', 'd', 'i', 'r', '>', '\n'};

void	usage(void);

extern int notify(void(*)(void*,char*));

void
main(int _argc, char **_argv)
{
	volatile int i, argc;
	char **volatile argv;
	String *t;
	char *termargs[10], **ap;

	argc = _argc;
	argv = _argv;
	ap = termargs;
	*ap++ = "samterm";
	ARGBEGIN{
	case 'd':
		dflag++;
		break;
	case 'r':
		machine = EARGF(usage());
		break;
	case 'R':
		Rflag++;
		break;
	case 't':
		samterm = EARGF(usage());
		break;
	case 's':
		rsamname = EARGF(usage());
		break;
	default:
		dprint("sam: unknown flag %c\n", ARGC());
		usage();
	/* options for samterm */
	case 'a':
		*ap++ = "-a";
		break;
	case 'W':
		*ap++ = "-W";
		*ap++ = EARGF(usage());
		break;
	}ARGEND
	*ap = nil;

	Strinit(&cmdstr);
	Strinit0(&lastpat);
	Strinit0(&lastregexp);
	Strinit0(&genstr);
	Strinit0(&rhs);
	Strinit0(&curwd);
	Strinit0(&plan9cmd);
	home = getenv(HOME);
	disk = diskinit();
	if(home == 0)
		home = "/";
	if(!dflag)
		startup(machine, Rflag, termargs, (char**)argv);
	notify(notifyf);
	getcurwd();
	if(argc>0){
		for(i=0; i<argc; i++){
			if(!setjmp(mainloop)){
				t = tmpcstr(argv[i]);
				Straddc(t, '\0');
				Strduplstr(&genstr, t);
				freetmpstr(t);
				fixname(&genstr);
				logsetname(newfile(), &genstr);
			}
		}
	}else if(!downloaded)
		newfile();
	seq++;
	if(file.nused)
		current(file.filepptr[0]);
	setjmp(mainloop);
	cmdloop();
	trytoquit();	/* if we already q'ed, quitok will be TRUE */
	exits(0);
}

void
usage(void)
{
	dprint("usage: sam [-d] [-t samterm] [-s sam name] [-r machine] [file ...]\n");
	exits("usage");
}

void
rescue(void)
{
	int i, nblank = 0;
	File *f;
	char *c;
	char buf[256];
	char *root;

	if(rescuing++)
		return;
	io = -1;
	for(i=0; i<file.nused; i++){
		f = file.filepptr[i];
		if(f==cmd || f->b.nc==0 || !fileisdirty(f))
			continue;
		if(io == -1){
			sprint(buf, "%s/sam.save", home);
			io = create(buf, 1, 0777);
			if(io<0)
				return;
		}
		if(f->name.s[0]){
			c = Strtoc(&f->name);
			strncpy(buf, c, sizeof buf-1);
			buf[sizeof buf-1] = 0;
			free(c);
		}else
			sprint(buf, "nameless.%d", nblank++);
		root = getenv("PLAN9");
		if(root == nil)
			root = "/usr/local/plan9";
		fprint(io, "#!/bin/sh\n%s/bin/samsave '%s' $* <<'---%s'\n", root, buf, buf);
		addr.r.p1 = 0, addr.r.p2 = f->b.nc;
		writeio(f);
		fprint(io, "\n---%s\n", (char *)buf);
	}
}

void
panic(char *s)
{
	int wasd;

	if(!panicking++ && !setjmp(mainloop)){
		wasd = downloaded;
		downloaded = 0;
		dprint("sam: panic: %s: %r\n", s);
		if(wasd)
			fprint(2, "sam: panic: %s: %r\n", s);
		rescue();
		abort();
	}
}

void
hiccough(char *s)
{
	File *f;
	int i;

	if(rescuing)
		exits("rescue");
	if(s)
		dprint("%s\n", s);
	resetcmd();
	resetxec();
	resetsys();
	if(io > 0)
		close(io);

	/*
	 * back out any logged changes & restore old sequences
	 */
	for(i=0; i<file.nused; i++){
		f = file.filepptr[i];
		if(f==cmd)
			continue;
		if(f->seq==seq){
			bufdelete(&f->epsilon, 0, f->epsilon.nc);
			f->seq = f->prevseq;
			f->dot.r = f->prevdot;
			f->mark = f->prevmark;
			state(f, f->prevmod ? Dirty: Clean);
		}
	}

	update();
	if (curfile) {
		if (curfile->unread)
			curfile->unread = FALSE;
		else if (downloaded)
			outTs(Hcurrent, curfile->tag);
	}
	longjmp(mainloop, 1);
}

void
intr(void)
{
	error(Eintr);
}

void
trytoclose(File *f)
{
	char *t;
	char buf[256];

	if(f == cmd)	/* possible? */
		return;
	if(f->deleted)
		return;
	if(fileisdirty(f) && !f->closeok){
		f->closeok = TRUE;
		if(f->name.s[0]){
			t = Strtoc(&f->name);
			strncpy(buf, t, sizeof buf-1);
			free(t);
		}else
			strcpy(buf, "nameless file");
		error_s(Emodified, buf);
	}
	f->deleted = TRUE;
}

void
trytoquit(void)
{
	int c;
	File *f;

	if(!quitok){
		for(c = 0; c<file.nused; c++){
			f = file.filepptr[c];
			if(f!=cmd && fileisdirty(f)){
				quitok = TRUE;
				eof = FALSE;
				error(Echanges);
			}
		}
	}
}

void
load(File *f)
{
	Address saveaddr;

	Strduplstr(&genstr, &f->name);
	filename(f);
	if(f->name.s[0]){
		saveaddr = addr;
		edit(f, 'I');
		addr = saveaddr;
	}else{
		f->unread = 0;
		f->cleanseq = f->seq;
	}

	fileupdate(f, TRUE, TRUE);
}

void
cmdupdate(void)
{
	if(cmd && cmd->seq!=0){
		fileupdate(cmd, FALSE, downloaded);
		cmd->dot.r.p1 = cmd->dot.r.p2 = cmd->b.nc;
		telldot(cmd);
	}
}

void
delete(File *f)
{
	if(downloaded && f->rasp)
		outTs(Hclose, f->tag);
	delfile(f);
	if(f == curfile)
		current(0);
}

void
update(void)
{
	int i, anymod;
	File *f;

	settempfile();
	for(anymod = i=0; i<tempfile.nused; i++){
		f = tempfile.filepptr[i];
		if(f==cmd)	/* cmd gets done in main() */
			continue;
		if(f->deleted) {
			delete(f);
			continue;
		}
		if(f->seq==seq && fileupdate(f, FALSE, downloaded))
			anymod++;
		if(f->rasp)
			telldot(f);
	}
	if(anymod)
		seq++;
}

File *
current(File *f)
{
	return curfile = f;
}

void
edit(File *f, int cmd)
{
	int empty = TRUE;
	Posn p;
	int nulls;

	if(cmd == 'r')
		logdelete(f, addr.r.p1, addr.r.p2);
	if(cmd=='e' || cmd=='I'){
		logdelete(f, (Posn)0, f->b.nc);
		addr.r.p2 = f->b.nc;
	}else if(f->b.nc!=0 || (f->name.s[0] && Strcmp(&genstr, &f->name)!=0))
		empty = FALSE;
	if((io = open(genc, OREAD))<0) {
		if (curfile && curfile->unread)
			curfile->unread = FALSE;
		error_r(Eopen, genc);
	}
	p = readio(f, &nulls, empty, TRUE);
	closeio((cmd=='e' || cmd=='I')? -1 : p);
	if(cmd == 'r')
		f->ndot.r.p1 = addr.r.p2, f->ndot.r.p2 = addr.r.p2+p;
	else
		f->ndot.r.p1 = f->ndot.r.p2 = 0;
	f->closeok = empty;
	if (quitok)
		quitok = empty;
	else
		quitok = FALSE;
	state(f, empty && !nulls? Clean : Dirty);
	if(empty && !nulls)
		f->cleanseq = f->seq;
	if(cmd == 'e')
		filename(f);
}

int
getname(File *f, String *s, int save)
{
	int c, i;

	Strzero(&genstr);
	if(genc){
		free(genc);
		genc = 0;
	}
	if(s==0 || (c = s->s[0])==0){		/* no name provided */
		if(f)
			Strduplstr(&genstr, &f->name);
		goto Return;
	}
	if(c!=' ' && c!='\t')
		error(Eblank);
	for(i=0; (c=s->s[i])==' ' || c=='\t'; i++)
		;
	while(s->s[i] > ' ')
		Straddc(&genstr, s->s[i++]);
	if(s->s[i])
		error(Enewline);
	fixname(&genstr);
	if(f && (save || f->name.s[0]==0)){
		logsetname(f, &genstr);
		if(Strcmp(&f->name, &genstr)){
			quitok = f->closeok = FALSE;
			f->qidpath = 0;
			f->mtime = 0;
			state(f, Dirty); /* if it's 'e', fix later */
		}
	}
    Return:
	genc = Strtoc(&genstr);
	i = genstr.n;
	if(i && genstr.s[i-1]==0)
		i--;
	return i;	/* strlen(name) */
}

void
filename(File *f)
{
	if(genc)
		free(genc);
	genc = Strtoc(&genstr);
	dprint("%c%c%c %s\n", " '"[f->mod],
		"-+"[f->rasp!=0], " ."[f==curfile], genc);
}

void
undostep(File *f, int isundo)
{
	uint p1, p2;
	int mod;

	mod = f->mod;
	fileundo(f, isundo, 1, &p1, &p2, TRUE);
	f->ndot = f->dot;
	if(f->mod){
		f->closeok = 0;
		quitok = 0;
	}else
		f->closeok = 1;

	if(f->mod != mod){
		f->mod = mod;
		if(mod)
			mod = Clean;
		else
			mod = Dirty;
		state(f, mod);
	}
}

int
undo(int isundo)
{
	File *f;
	int i;
	Mod max;

	max = undoseq(curfile, isundo);
	if(max == 0)
		return 0;
	settempfile();
	for(i = 0; i<tempfile.nused; i++){
		f = tempfile.filepptr[i];
		if(f!=cmd && undoseq(f, isundo)==max)
			undostep(f, isundo);
	}
	return 1;
}

int
readcmd(String *s)
{
	int retcode;

	if(flist != 0)
		fileclose(flist);
	flist = fileopen();

	addr.r.p1 = 0, addr.r.p2 = flist->b.nc;
	retcode = plan9(flist, '<', s, FALSE);
	fileupdate(flist, FALSE, FALSE);
	flist->seq = 0;
	if (flist->b.nc > BLOCKSIZE)
		error(Etoolong);
	Strzero(&genstr);
	Strinsure(&genstr, flist->b.nc);
	bufread(&flist->b, (Posn)0, genbuf, flist->b.nc);
	memmove(genstr.s, genbuf, flist->b.nc*RUNESIZE);
	genstr.n = flist->b.nc;
	Straddc(&genstr, '\0');
	return retcode;
}

void
getcurwd(void)
{
	String *t;
	char buf[256];

	buf[0] = 0;
	getwd(buf, sizeof(buf));
	t = tmpcstr(buf);
	Strduplstr(&curwd, t);
	freetmpstr(t);
	if(curwd.n == 0)
		warn(Wpwd);
	else if(curwd.s[curwd.n-1] != '/')
		Straddc(&curwd, '/');
}

void
cd(String *str)
{
	int i, fd;
	char *s;
	File *f;
	String owd;

	getcurwd();
	if(getname((File *)0, str, FALSE))
		s = genc;
	else
		s = home;
	if(chdir(s))
		syserror("chdir");
	fd = open("/dev/wdir", OWRITE);
	if(fd > 0)
		write(fd, s, strlen(s));
	dprint("!\n");
	Strinit(&owd);
	Strduplstr(&owd, &curwd);
	getcurwd();
	settempfile();
	/*
	 * Two passes so that if we have open
	 * /a/foo.c and /b/foo.c and cd from /b to /a,
	 * we don't ever have two foo.c simultaneously.
	 */
	for(i=0; i<tempfile.nused; i++){
		f = tempfile.filepptr[i];
		if(f!=cmd && f->name.s[0]!='/' && f->name.s[0]!=0){
			Strinsert(&f->name, &owd, (Posn)0);
			fixname(&f->name);
			sortname(f);
		}
	}
	for(i=0; i<tempfile.nused; i++){
		f = tempfile.filepptr[i];
		if(f != cmd && Strispre(&curwd, &f->name)){
			fixname(&f->name);
			sortname(f);
		}
	}
	Strclose(&owd);
}

int
loadflist(String *s)
{
	int c, i;

	c = s->s[0];
	for(i = 0; s->s[i]==' ' || s->s[i]=='\t'; i++)
		;
	if((c==' ' || c=='\t') && s->s[i]!='\n'){
		if(s->s[i]=='<'){
			Strdelete(s, 0L, (long)i+1);
			readcmd(s);
		}else{
			Strzero(&genstr);
			while((c = s->s[i++]) && c!='\n')
				Straddc(&genstr, c);
			Straddc(&genstr, '\0');
		}
	}else{
		if(c != '\n')
			error(Eblank);
		Strdupl(&genstr, empty);
	}
	if(genc)
		free(genc);
	genc = Strtoc(&genstr);
	return genstr.s[0];
}

File *
readflist(int readall, int delete)
{
	Posn i;
	int c;
	File *f;
	String t;

	Strinit(&t);
	for(i=0,f=0; f==0 || readall || delete; i++){	/* ++ skips blank */
		Strdelete(&genstr, (Posn)0, i);
		for(i=0; (c = genstr.s[i])==' ' || c=='\t' || c=='\n'; i++)
			;
		if(i >= genstr.n)
			break;
		Strdelete(&genstr, (Posn)0, i);
		for(i=0; (c=genstr.s[i]) && c!=' ' && c!='\t' && c!='\n'; i++)
			;

		if(i == 0)
			break;
		genstr.s[i] = 0;
		Strduplstr(&t, tmprstr(genstr.s, i+1));
		fixname(&t);
		f = lookfile(&t);
		if(delete){
			if(f == 0)
				warn_S(Wfile, &t);
			else
				trytoclose(f);
		}else if(f==0 && readall)
			logsetname(f = newfile(), &t);
	}
	Strclose(&t);
	return f;
}

File *
tofile(String *s)
{
	File *f;

	if(s->s[0] != ' ')
		error(Eblank);
	if(loadflist(s) == 0){
		f = lookfile(&genstr);	/* empty string ==> nameless file */
		if(f == 0)
			error_s(Emenu, genc);
	}else if((f=readflist(FALSE, FALSE)) == 0)
		error_s(Emenu, genc);
	return current(f);
}

File *
getfile(String *s)
{
	File *f;

	if(loadflist(s) == 0)
		logsetname(f = newfile(), &genstr);
	else if((f=readflist(TRUE, FALSE)) == 0)
		error(Eblank);
	return current(f);
}

void
closefiles(File *f, String *s)
{
	if(s->s[0] == 0){
		if(f == 0)
			error(Enofile);
		trytoclose(f);
		return;
	}
	if(s->s[0] != ' ')
		error(Eblank);
	if(loadflist(s) == 0)
		error(Enewline);
	readflist(FALSE, TRUE);
}

void
copy(File *f, Address addr2)
{
	Posn p;
	int ni;
	for(p=addr.r.p1; p<addr.r.p2; p+=ni){
		ni = addr.r.p2-p;
		if(ni > BLOCKSIZE)
			ni = BLOCKSIZE;
		bufread(&f->b, p, genbuf, ni);
		loginsert(addr2.f, addr2.r.p2, tmprstr(genbuf, ni)->s, ni);
	}
	addr2.f->ndot.r.p2 = addr2.r.p2+(f->dot.r.p2-f->dot.r.p1);
	addr2.f->ndot.r.p1 = addr2.r.p2;
}

void
move(File *f, Address addr2)
{
	if(addr.r.p2 <= addr2.r.p2){
		logdelete(f, addr.r.p1, addr.r.p2);
		copy(f, addr2);
	}else if(addr.r.p1 >= addr2.r.p2){
		copy(f, addr2);
		logdelete(f, addr.r.p1, addr.r.p2);
	}else
		error(Eoverlap);
}

Posn
nlcount(File *f, Posn p0, Posn p1)
{
	Posn nl = 0;

	while(p0 < p1)
		if(filereadc(f, p0++)=='\n')
			nl++;
	return nl;
}

void
printposn(File *f, int charsonly)
{
	Posn l1, l2;

	if(!charsonly){
		l1 = 1+nlcount(f, (Posn)0, addr.r.p1);
		l2 = l1+nlcount(f, addr.r.p1, addr.r.p2);
		/* check if addr ends with '\n' */
		if(addr.r.p2>0 && addr.r.p2>addr.r.p1 && filereadc(f, addr.r.p2-1)=='\n')
			--l2;
		dprint("%lud", l1);
		if(l2 != l1)
			dprint(",%lud", l2);
		dprint("; ");
	}
	dprint("#%lud", addr.r.p1);
	if(addr.r.p2 != addr.r.p1)
		dprint(",#%lud", addr.r.p2);
	dprint("\n");
}

void
settempfile(void)
{
	if(tempfile.nalloc < file.nused){
		if(tempfile.filepptr)
			free(tempfile.filepptr);
		tempfile.filepptr = emalloc(sizeof(File*)*file.nused);
		tempfile.nalloc = file.nused;
	}
	memmove(tempfile.filepptr, file.filepptr, sizeof(File*)*file.nused);
	tempfile.nused = file.nused;
}
