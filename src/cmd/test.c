/*
 * POSIX standard
 *	test expression
 *	[ expression ]
 *
 * Plan 9 additions:
 *	-A file exists and is append-only
 *	-L file exists and is exclusive-use
 */

#include <u.h>
#include <libc.h>
#define EQ(a,b)	((tmp=a)==0?0:(strcmp(tmp,b)==0))

extern int isatty(int); /* <unistd.h> */

int	ap;
int	ac;
char	**av;
char	*tmp;

void	synbad(char *, char *);
int	fsizep(char *);
int	isdir(char *);
int	isreg(char *);
int	isint(char *, int *);
int	hasmode(char *, ulong);
int	tio(char *, int);
int	e(void), e1(void), e2(void), e3(void);

void
main(int argc, char *argv[])
{

	ac = argc; av = argv; ap = 1;
	if(EQ(argv[0],"[")) {
		if(!EQ(argv[--ac],"]"))
			synbad("] missing","");
	}
	argv[ac] = 0;
	if (ac<=1) exits("usage");
	exits(e()?0:"false");
}

char *
nxtarg(int mt)
{
	if(ap>=ac){
		if(mt){
			ap++;
			return(0);
		}
		synbad("argument expected","");
	}
	return(av[ap++]);
}

int
nxtintarg(int *pans)
{
	if(ap<ac && isint(av[ap], pans)){
		ap++;
		return 1;
	}
	return 0;
}

int
e(void) {
	int p1;

	p1 = e1();
	if (EQ(nxtarg(1), "-o")) return(p1 || e());
	ap--;
	return(p1);
}

int
e1(void) {
	int p1;

	p1 = e2();
	if (EQ(nxtarg(1), "-a")) return (p1 && e1());
	ap--;
	return(p1);
}

int
e2(void) {
	if (EQ(nxtarg(0), "!"))
		return(!e2());
	ap--;
	return(e3());
}

int
e3(void) {
	int p1;
	char *a;
	char *p2;
	int int1, int2;

	a = nxtarg(0);
	if(EQ(a, "(")) {
		p1 = e();
		if(!EQ(nxtarg(0), ")")) synbad(") expected","");
		return(p1);
	}

	if(EQ(a, "-A"))
		return(hasmode(nxtarg(0), DMAPPEND));

	if(EQ(a, "-L"))
		return(hasmode(nxtarg(0), DMEXCL));

	if(EQ(a, "-f"))
		return(isreg(nxtarg(0)));

	if(EQ(a, "-d"))
		return(isdir(nxtarg(0)));

	if(EQ(a, "-r"))
		return(tio(nxtarg(0), 4));

	if(EQ(a, "-w"))
		return(tio(nxtarg(0), 2));

	if(EQ(a, "-x"))
		return(tio(nxtarg(0), 1));

	if(EQ(a, "-e"))
		return(tio(nxtarg(0), 0));

	if(EQ(a, "-c"))
		return(0);

	if(EQ(a, "-b"))
		return(0);

	if(EQ(a, "-u"))
		return(0);

	if(EQ(a, "-g"))
		return(0);

	if(EQ(a, "-s"))
		return(fsizep(nxtarg(0)));

	if(EQ(a, "-t"))
		if(ap>=ac || !nxtintarg(&int1))
			return(isatty(1));
		else
			return(isatty(int1));

	if(EQ(a, "-n"))
		return(!EQ(nxtarg(0), ""));
	if(EQ(a, "-z"))
		return(EQ(nxtarg(0), ""));

	p2 = nxtarg(1);
	if (p2==0)
		return(!EQ(a,""));
	if(EQ(p2, "="))
		return(EQ(nxtarg(0), a));

	if(EQ(p2, "!="))
		return(!EQ(nxtarg(0), a));

	if(!isint(a, &int1))
		return(!EQ(a,""));

	if(nxtintarg(&int2)){
		if(EQ(p2, "-eq"))
			return(int1==int2);
		if(EQ(p2, "-ne"))
			return(int1!=int2);
		if(EQ(p2, "-gt"))
			return(int1>int2);
		if(EQ(p2, "-lt"))
			return(int1<int2);
		if(EQ(p2, "-ge"))
			return(int1>=int2);
		if(EQ(p2, "-le"))
			return(int1<=int2);
	}

	synbad("unknown operator ",p2);
	return 0;		/* to shut ken up */
}

int
tio(char *a, int f)
{
	return access (a, f) >= 0;
}

/* copy to local memory; clear names for safety */
int
localstat(char *f, Dir *dir)
{
	Dir *d;

	d = dirstat(f);
	if(d == 0)
		return(-1);
	*dir = *d;
	dir->name = 0;
	dir->uid = 0;
	dir->gid = 0;
	dir->muid = 0;
	return 0;
}

/* copy to local memory; clear names for safety */
int
localfstat(int f, Dir *dir)
{
	Dir *d;

	d = dirfstat(f);
	if(d == 0)
		return(-1);
	*dir = *d;
	dir->name = 0;
	dir->uid = 0;
	dir->gid = 0;
	dir->muid = 0;
	return 0;
}

int
hasmode(char *f, ulong m)
{
	Dir dir;

	if(localstat(f,&dir)<0)
		return(0);
	return(dir.mode&m);
}

int
isdir(char *f)
{
	Dir dir;

	if(localstat(f,&dir)<0)
		return(0);
	return(dir.mode&DMDIR);
}

int
isreg(char *f)
{
	Dir dir;

	if(localstat(f,&dir)<0)
		return(0);
	return(!(dir.mode&DMDIR));
}

int
fsizep(char *f)
{
	Dir dir;

	if(localstat(f,&dir)<0)
		return(0);
	return(dir.length>0);
}

void
synbad(char *s1, char *s2)
{
	int len;

	write(2, "test: ", 6);
	if ((len = strlen(s1)) != 0)
		write(2, s1, len);
	if ((len = strlen(s2)) != 0)
		write(2, s2, len);
	write(2, "\n", 1);
	exits("bad syntax");
}

int
isint(char *s, int *pans)
{
	char *ep;

	*pans = strtol(s, &ep, 0);
	return (*ep == 0);
}
