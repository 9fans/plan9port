#include "sam.h"

Rune	samname[] = L"~~sam~~";

Rune *left[]= {
	L"{[(<«",
	L"\n",
	L"'\"`",
	0
};
Rune *right[]= {
	L"}])>»",
	L"\n",
	L"'\"`",
	0
};

char	RSAM[] = "sam";
char	SAMTERM[] = "/bin/aux/samterm";
char	HOME[] = "HOME";
char	TMPDIR[] = "/tmp";
char	SH[] = "rc";
char	SHPATH[] = "/bin/rc";
char	RX[] = "rx";
char	RXPATH[] = "/bin/rx";
char	SAMSAVECMD[] = "/bin/rc\n/sys/lib/samsave";

void
dprint(char *z, ...)
{
	char buf[BLOCKSIZE];
	va_list arg;

	va_start(arg, z);
	vseprint(buf, &buf[BLOCKSIZE], z, arg);
	va_end(arg);
	termwrite(buf);
}

void
print_ss(char *s, String *a, String *b)
{
	dprint("?warning: %s: `%.*S' and `%.*S'\n", s, a->n, a->s, b->n, b->s);
}

void
print_s(char *s, String *a)
{
	dprint("?warning: %s `%.*S'\n", s, a->n, a->s);
}

char*
getuser(void)
{
	static char user[64];
	int fd;

	if(user[0] == 0){
		fd = open("/dev/user", 0);
		if(fd<0 || read(fd, user, sizeof user-1)<=0)
			strcpy(user, "none");
		close(fd);
	}
	return user;
}

int
statfile(char *name, ulong *dev, uvlong *id, long *time, long *length, long *appendonly)
{
	Dir *dirb;

	dirb = dirstat(name);
	if(dirb == nil)
		return -1;
	if(dev)
		*dev = dirb->type|(dirb->dev<<16);
	if(id)
		*id = dirb->qid.path;
	if(time)
		*time = dirb->mtime;
	if(length)
		*length = dirb->length;
	if(appendonly)
		*appendonly = dirb->mode & DMAPPEND;
	free(dirb);
	return 1;
}

int
statfd(int fd, ulong *dev, uvlong *id, long *time, long *length, long *appendonly)
{
	Dir *dirb;

	dirb = dirfstat(fd);
	if(dirb == nil)
		return -1;
	if(dev)
		*dev = dirb->type|(dirb->dev<<16);
	if(id)
		*id = dirb->qid.path;
	if(time)
		*time = dirb->mtime;
	if(length)
		*length = dirb->length;
	if(appendonly)
		*appendonly = dirb->mode & DMAPPEND;
	free(dirb);
	return 1;
}

void
notifyf(void *a, char *s)
{
	USED(a);
	if(bpipeok && strcmp(s, "sys: write on closed pipe") == 0)
		noted(NCONT);
	if(strcmp(s, "interrupt") == 0)
		noted(NCONT);
	panicking = 1;
	rescue();
	noted(NDFLT);
}

int
newtmp(int num)
{
	int i, fd;
	static char	tempnam[30];

	i = getpid();
	do
		snprint(tempnam, sizeof tempnam, "%s/%d%.4s%dsam", TMPDIR, num, getuser(), i++);
	while(access(tempnam, 0) == 0);
	fd = create(tempnam, ORDWR|OCEXEC|ORCLOSE, 0000);
	if(fd < 0){
		remove(tempnam);
		fd = create(tempnam, ORDWR|OCEXEC|ORCLOSE, 0000);
	}
	return fd;
}

int
waitfor(int pid)
{
	int msg;
	Waitmsg *w;

	while((w = wait()) != nil){
		if(w->pid != pid){
			free(w);
			continue;
		}
		msg = (w->msg[0] != '\0');
		free(w);
		return msg;
	}
	return -1;
}

void
samerr(char *buf)
{
	sprint(buf, "%s/sam.err", TMPDIR);
}

void*
emalloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == 0)
		panic("malloc fails");
	memset(p, 0, n);
	return p;
}

void*
erealloc(void *p, ulong n)
{
	p = realloc(p, n);
	if(p == 0)
		panic("realloc fails");
	return p;
}
