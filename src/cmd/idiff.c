/*
 * interactive diff, inspired/stolen from
 * kernighan and pike, _unix programming environment_.
 */

#include <u.h>
#include <libc.h>
#include <bio.h>

int diffbflag;
int diffwflag;

void copy(Biobuf*, char*, Biobuf*, char*);
void idiff(Biobuf*, char*, Biobuf*, char*, Biobuf*, char*, Biobuf*, char*);
int opentemp(char*, int, long);
void rundiff(char*, char*, int);

void
usage(void)
{
	fprint(2, "usage: idiff [-bw] file1 file2\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int fd, ofd;
	char diffout[40], idiffout[40];
	Biobuf *b1, *b2, bdiff, bout, bstdout;
	Dir *d;

	ARGBEGIN{
	default:
		usage();
	case 'b':
		diffbflag++;
		break;
	case 'w':
		diffwflag++;
		break;
	}ARGEND

	if(argc != 2)
		usage();

	if((d = dirstat(argv[0])) == nil)
		sysfatal("stat %s: %r", argv[0]);
	if(d->mode&DMDIR)
		sysfatal("%s is a directory", argv[0]);
	free(d);
	if((d = dirstat(argv[1])) == nil)
		sysfatal("stat %s: %r", argv[1]);
	if(d->mode&DMDIR)
		sysfatal("%s is a directory", argv[1]);
	free(d);

	if((b1 = Bopen(argv[0], OREAD)) == nil)
		sysfatal("open %s: %r", argv[0]);
	if((b2 = Bopen(argv[1], OREAD)) == nil)
		sysfatal("open %s: %r", argv[1]);

	strcpy(diffout, "/tmp/idiff.XXXXXX");
	fd = opentemp(diffout, ORDWR|ORCLOSE, 0);
	strcpy(idiffout, "/tmp/idiff.XXXXXX");
	ofd = opentemp(idiffout, ORDWR|ORCLOSE, 0);
	rundiff(argv[0], argv[1], fd);
	seek(fd, 0, 0);
	Binit(&bdiff, fd, OREAD);
	Binit(&bout, ofd, OWRITE);
	idiff(b1, argv[0], b2, argv[1], &bdiff, diffout, &bout, idiffout);
	Bterm(&bdiff);
	Bflush(&bout);
	seek(ofd, 0, 0);
	Binit(&bout, ofd, OREAD);
	Binit(&bstdout, 1, OWRITE);
	copy(&bout, idiffout, &bstdout, "<stdout>");
	exits(nil);
}

int
opentemp(char *template, int mode, long perm)
{
	int fd, i;
	char *p;	

	p = strdup(template);
	if(p == nil)
		sysfatal("strdup out of memory");
	fd = -1;
	for(i=0; i<10; i++){
		mktemp(p);
		if(access(p, 0) < 0 && (fd=create(p, mode, perm)) >= 0)
			break;
		strcpy(p, template);
	}
	if(fd < 0)
		sysfatal("could not create temporary file");
	strcpy(template, p);
	free(p);

	return fd;
}

void
rundiff(char *arg1, char *arg2, int outfd)
{
	char *arg[10], *p;
	int narg, pid;
	Waitmsg *w;

	narg = 0;
	arg[narg++] = "/bin/diff";
	arg[narg++] = "-n";
	if(diffbflag)
		arg[narg++] = "-b";
	if(diffwflag)
		arg[narg++] = "-w";
	arg[narg++] = arg1;
	arg[narg++] = arg2;
	arg[narg] = nil;

	switch(pid = fork()){
	case -1:
		sysfatal("fork: %r");

	case 0:
		dup(outfd, 1);
		close(0);
		exec("/bin/diff", arg);
		sysfatal("exec: %r");

	default:
		w = wait();
		if(w==nil)
			sysfatal("wait: %r");
		if(w->pid != pid)
			sysfatal("wait got unexpected pid %d", w->pid);
		if((p = strchr(w->msg, ':')) && strcmp(p, ": some") != 0)
			sysfatal("%s", w->msg);
		free(w);
	}
}

void
runcmd(char *cmd)
{
	char *arg[10];
	int narg, pid, wpid;

	narg = 0;
	arg[narg++] = "/bin/rc";
	arg[narg++] = "-c";
	arg[narg++] = cmd;
	arg[narg] = nil;

	switch(pid = fork()){
	case -1:
		sysfatal("fork: %r");

	case 0:
		exec("/bin/rc", arg);
		sysfatal("exec: %r");

	default:
		wpid = waitpid();
		if(wpid < 0)
			sysfatal("wait: %r");
		if(wpid != pid)
			sysfatal("wait got unexpected pid %d", wpid);
	}
}

void
parse(char *s, int *pfrom1, int *pto1, int *pcmd, int *pfrom2, int *pto2)
{
	*pfrom1 = *pto1 = *pfrom2 = *pto2 = 0;

	s = strchr(s, ':');
	if(s == nil)
		sysfatal("bad diff output0");
	s++;
	*pfrom1 = strtol(s, &s, 10);
	if(*s == ','){
		s++;
		*pto1 = strtol(s, &s, 10);
	}else
		*pto1 = *pfrom1;
	if(*s++ != ' ')
		sysfatal("bad diff output1");
	*pcmd = *s++;
	if(*s++ != ' ')
		sysfatal("bad diff output2");
	s = strchr(s, ':');
	if(s == nil)
		sysfatal("bad diff output3");
	s++;
	*pfrom2 = strtol(s, &s, 10);
	if(*s == ','){
		s++;
		*pto2 = strtol(s, &s, 10);
	}else
		*pto2 = *pfrom2;
}

void
skiplines(Biobuf *b, char *name, int n)
{
	int i;

	for(i=0; i<n; i++){
		while(Brdline(b, '\n')==nil){
			if(Blinelen(b) <= 0)
				sysfatal("early end of file on %s", name);
			Bseek(b, Blinelen(b), 1);
		}
	}
}

void
copylines(Biobuf *bin, char *nin, Biobuf *bout, char *nout, int n)
{
	char buf[4096], *p;
	int i, m;

	for(i=0; i<n; i++){
		while((p=Brdline(bin, '\n'))==nil){
			if(Blinelen(bin) <= 0)
				sysfatal("early end of file on %s", nin);
			m = Blinelen(bin);
			if(m > sizeof buf)
				m = sizeof buf;
			m = Bread(bin, buf, m);
			if(Bwrite(bout, buf, m) != m)
				sysfatal("error writing %s: %r", nout);
		}
		if(Bwrite(bout, p, Blinelen(bin)) != Blinelen(bin))
			sysfatal("error writing %s: %r", nout);
	}
}

void
copy(Biobuf *bin, char *nin, Biobuf *bout, char *nout)
{
	char buf[4096];
	int m;

	USED(nin);
	while((m = Bread(bin, buf, sizeof buf)) > 0)
		if(Bwrite(bout, buf, m) != m)
			sysfatal("error writing %s: %r", nout);
}

void
idiff(Biobuf *b1, char *name1, Biobuf *b2, char *name2, Biobuf *bdiff, char *namediff, Biobuf *bout, char *nameout)
{
	char buf[256], *p;
	int interactive, defaultanswer, cmd, diffoffset;
	int n, from1, to1, from2, to2, nf1, nf2;
	Biobuf berr;

	nf1 = 1;
	nf2 = 1;
	interactive = 1;
	defaultanswer = 0;
	Binit(&berr, 2, OWRITE);
	while(diffoffset = Boffset(bdiff), p = Brdline(bdiff, '\n')){
		p[Blinelen(bdiff)-1] = '\0';
		parse(p, &from1, &to1, &cmd, &from2, &to2);
		p[Blinelen(bdiff)-1] = '\n';
		n = to1-from1 + to2-from2 + 1;	/* #lines from diff */
		if(cmd == 'c')
			n += 2;
		else if(cmd == 'a')
			from1++;
		else if(cmd == 'd')
			from2++;
		to1++;	/* make half-open intervals */
		to2++;
		if(interactive){
			p[Blinelen(bdiff)-1] = '\0';
			fprint(2, "%s\n", p);
			p[Blinelen(bdiff)-1] = '\n';
			copylines(bdiff, namediff, &berr, "<stderr>", n);
			Bflush(&berr);
		}else
			skiplines(bdiff, namediff, n);
		do{
			if(interactive){
				fprint(2, "? ");
				memset(buf, 0, sizeof buf);
				if(read(0, buf, sizeof buf - 1) < 0)
					sysfatal("read console: %r");
			}else
				buf[0] = defaultanswer;

			switch(buf[0]){
			case '>':
				copylines(b1, name1, bout, nameout, from1-nf1);
				skiplines(b1, name1, to1-from1);
				skiplines(b2, name2, from2-nf2);
				copylines(b2, name2, bout, nameout, to2-from2);
				break;
			case '<':
				copylines(b1, name1, bout, nameout, to1-nf1);
				skiplines(b2, name2, to2-nf2);
				break;
			case '=':
				copylines(b1, name1, bout, nameout, from1-nf1);
				skiplines(b1, name1, to1-from1);
				skiplines(b2, name2, to2-nf2);
				if(Bseek(bdiff, diffoffset, 0) != diffoffset)
					sysfatal("seek in diff output: %r");
				copylines(bdiff, namediff, bout, nameout, n+1);
				break;
			case '!':
				runcmd(buf+1);
				break;
			case 'q':
				if(buf[1]=='<' || buf[1]=='>' || buf[1]=='='){
					interactive = 0;
					defaultanswer = buf[1];
				}else
					fprint(2, "must be q<, q>, or q=\n");
				break;
			default:
				fprint(2, "expect: <, >, =, q<, q>, q=, !cmd\n");
				break;
			}
		}while(buf[0] != '<' && buf[0] != '>' && buf[0] != '=');
		nf1 = to1;
		nf2 = to2;
	}
	copy(b1, name1, bout, nameout);
}
