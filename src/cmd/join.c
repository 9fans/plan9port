/*	join F1 F2 on stuff */
#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <ctype.h>
#define F1 0
#define F2 1
#define F0 3
#define	NFLD	100	/* max field per line */
#define comp() runecmp(ppi[F1][j1],ppi[F2][j2])
FILE *f[2];
Rune buf[2][BUFSIZ];	/*input lines */
Rune *ppi[2][NFLD+1];	/* pointers to fields in lines */
Rune *s1,*s2;
#define j1 joinj1
#define j2 joinj2

int	j1	= 1;	/* join of this field of file 1 */
int	j2	= 1;	/* join of this field of file 2 */
int	olist[2*NFLD];	/* output these fields */
int	olistf[2*NFLD];	/* from these files */
int	no;		/* number of entries in olist */
Rune	sep1	= ' ';	/* default field separator */
Rune	sep2	= '\t';
char *sepstr=" ";
int	discard;	/* count of truncated lines */
Rune	null[BUFSIZ]/*	= L""*/;
int	a1;
int 	a2;

char *getoptarg(int*, char***);
void output(int, int);
int input(int);
void oparse(char*);
void error(char*, char*);
void seek1(void), seek2(void);
Rune *strtorune(Rune *, char *);


void
main(int argc, char **argv)
{
	int i;

	while (argc > 1 && argv[1][0] == '-') {
		if (argv[1][1] == '\0')
			break;
		switch (argv[1][1]) {
		case '-':
			argc--;
			argv++;
			goto proceed;
		case 'a':
			switch(*getoptarg(&argc, &argv)) {
			case '1':
				a1++;
				break;
			case '2':
				a2++;
				break;
			default:
				error("incomplete option -a","");
			}
			break;
		case 'e':
			strtorune(null, getoptarg(&argc, &argv));
			break;
		case 't':
			sepstr=getoptarg(&argc, &argv);
			chartorune(&sep1, sepstr);
			sep2 = sep1;
			break;
		case 'o':
			if(argv[1][2]!=0 ||
			   argc>2 && strchr(argv[2],',')!=0)
				oparse(getoptarg(&argc, &argv));
			else for (no = 0; no<2*NFLD && argc>2; no++){
				if (argv[2][0] == '1' && argv[2][1] == '.') {
					olistf[no] = F1;
					olist[no] = atoi(&argv[2][2]);
				} else if (argv[2][0] == '2' && argv[2][1] == '.') {
					olist[no] = atoi(&argv[2][2]);
					olistf[no] = F2;
				} else if (argv[2][0] == '0')
					olistf[no] = F0;
				else
					break;
				argc--;
				argv++;
			}
			break;
		case 'j':
			if(argc <= 2)
				break;
			if (argv[1][2] == '1')
				j1 = atoi(argv[2]);
			else if (argv[1][2] == '2')
				j2 = atoi(argv[2]);
			else
				j1 = j2 = atoi(argv[2]);
			argc--;
			argv++;
			break;
		case '1':
			j1 = atoi(getoptarg(&argc, &argv));
			break;
		case '2':
			j2 = atoi(getoptarg(&argc, &argv));
			break;
		}
		argc--;
		argv++;
	}
proceed:
	for (i = 0; i < no; i++)
		if (olist[i]-- > NFLD)	/* 0 origin */
			error("field number too big in -o","");
	if (argc != 3)
		error("usage: join [-1 x -2 y] [-o list] file1 file2","");
	j1--;
	j2--;	/* everyone else believes in 0 origin */
	s1 = ppi[F1][j1];
	s2 = ppi[F2][j2];
	if (strcmp(argv[1], "-") == 0)
		f[F1] = stdin;
	else if ((f[F1] = fopen(argv[1], "r")) == 0)
		error("can't open %s", argv[1]);
	if(strcmp(argv[2], "-") == 0) {
		f[F2] = stdin;
	} else if ((f[F2] = fopen(argv[2], "r")) == 0)
		error("can't open %s", argv[2]);

	if(ftell(f[F2]) >= 0)
		seek2();
	else if(ftell(f[F1]) >= 0)
		seek1();
	else
		error("neither file is randomly accessible","");
	if (discard)
		error("some input line was truncated", "");
	exits("");
}
int runecmp(Rune *a, Rune *b){
	while(*a==*b){
		if(*a=='\0') return 0;
		a++;
		b++;
	}
	if(*a<*b) return -1;
	return 1;
}
char *runetostr(char *buf, Rune *r){
	char *s;
	for(s=buf;*r;r++) s+=runetochar(s, r);
	*s='\0';
	return buf;
}
Rune *strtorune(Rune *buf, char *s){
	Rune *r;
	for(r=buf;*s;r++) s+=chartorune(r, s);
	*r='\0';
	return buf;
}
/* lazy.  there ought to be a clean way to combine seek1 & seek2 */
#define get1() n1=input(F1)
#define get2() n2=input(F2)
void
seek2(void)
{
	int n1, n2;
	int top2=0;
	int bot2 = ftell(f[F2]);
	get1();
	get2();
	while(n1>0 && n2>0 || (a1||a2) && n1+n2>0) {
		if(n1>0 && n2>0 && comp()>0 || n1==0) {
			if(a2) output(0, n2);
			bot2 = ftell(f[F2]);
			get2();
		} else if(n1>0 && n2>0 && comp()<0 || n2==0) {
			if(a1) output(n1, 0);
			get1();
		} else /*(n1>0 && n2>0 && comp()==0)*/ {
			while(n2>0 && comp()==0) {
				output(n1, n2);
				top2 = ftell(f[F2]);
				get2();
			}
			fseek(f[F2], bot2, 0);
			get2();
			get1();
			for(;;) {
				if(n1>0 && n2>0 && comp()==0) {
					output(n1, n2);
					get2();
				} else if(n1>0 && n2>0 && comp()<0 || n2==0) {
					fseek(f[F2], bot2, 0);
					get2();
					get1();
				} else /*(n1>0 && n2>0 && comp()>0 || n1==0)*/{
					fseek(f[F2], top2, 0);
					bot2 = top2;
					get2();
					break;
				}
			}
		}
	}
}
void
seek1(void)
{
	int n1, n2;
	int top1=0;
	int bot1 = ftell(f[F1]);
	get1();
	get2();
	while(n1>0 && n2>0 || (a1||a2) && n1+n2>0) {
		if(n1>0 && n2>0 && comp()>0 || n1==0) {
			if(a2) output(0, n2);
			get2();
		} else if(n1>0 && n2>0 && comp()<0 || n2==0) {
			if(a1) output(n1, 0);
			bot1 = ftell(f[F1]);
			get1();
		} else /*(n1>0 && n2>0 && comp()==0)*/ {
			while(n2>0 && comp()==0) {
				output(n1, n2);
				top1 = ftell(f[F1]);
				get1();
			}
			fseek(f[F1], bot1, 0);
			get2();
			get1();
			for(;;) {
				if(n1>0 && n2>0 && comp()==0) {
					output(n1, n2);
					get1();
				} else if(n1>0 && n2>0 && comp()>0 || n1==0) {
					fseek(f[F1], bot1, 0);
					get2();
					get1();
				} else /*(n1>0 && n2>0 && comp()<0 || n2==0)*/{
					fseek(f[F1], top1, 0);
					bot1 = top1;
					get1();
					break;
				}
			}
		}
	}
}

int
input(int n)		/* get input line and split into fields */
{
	register int i, c;
	Rune *bp;
	Rune **pp;
	char line[BUFSIZ];

	bp = buf[n];
	pp = ppi[n];
	if (fgets(line, BUFSIZ, f[n]) == 0)
		return(0);
	strtorune(bp, line);
	i = 0;
	do {
		i++;
		if (sep1 == ' ')	/* strip multiples */
			while ((c = *bp) == sep1 || c == sep2)
				bp++;	/* skip blanks */
		*pp++ = bp;	/* record beginning */
		while ((c = *bp) != sep1 && c != '\n' && c != sep2 && c != '\0')
			bp++;
		*bp++ = '\0';	/* mark end by overwriting blank */
	} while (c != '\n' && c != '\0' && i < NFLD-1);
	if (c != '\n')
		discard++;

	*pp = 0;
	return(i);
}

void
output(int on1, int on2)	/* print items from olist */
{
	int i;
	Rune *temp;
	char buf[BUFSIZ];

	if (no <= 0) {	/* default case */
		printf("%s", runetostr(buf, on1? ppi[F1][j1]: ppi[F2][j2]));
		for (i = 0; i < on1; i++)
			if (i != j1)
				printf("%s%s", sepstr, runetostr(buf, ppi[F1][i]));
		for (i = 0; i < on2; i++)
			if (i != j2)
				printf("%s%s", sepstr, runetostr(buf, ppi[F2][i]));
		printf("\n");
	} else {
		for (i = 0; i < no; i++) {
			if (olistf[i]==F0 && on1>j1)
				temp = ppi[F1][j1];
			else if (olistf[i]==F0 && on2>j2)
				temp = ppi[F2][j2];
			else {
				temp = ppi[olistf[i]][olist[i]];
				if(olistf[i]==F1 && on1<=olist[i] ||
				   olistf[i]==F2 && on2<=olist[i] ||
				   *temp==0)
					temp = null;
			}
			printf("%s", runetostr(buf, temp));
			if (i == no - 1)
				printf("\n");
			else
				printf("%s", sepstr);
		}
	}
}

void
error(char *s1, char *s2)
{
	fprintf(stderr, "join: ");
	fprintf(stderr, s1, s2);
	fprintf(stderr, "\n");
	exits(s1);
}

char *
getoptarg(int *argcp, char ***argvp)
{
	int argc = *argcp;
	char **argv = *argvp;
	if(argv[1][2] != 0)
		return &argv[1][2];
	if(argc<=2 || argv[2][0]=='-')
		error("incomplete option %s", argv[1]);
	*argcp = argc-1;
	*argvp = ++argv;
	return argv[1];
}

void
oparse(char *s)
{
	for (no = 0; no<2*NFLD && *s; no++, s++) {
		switch(*s) {
		case 0:
			return;
		case '0':
			olistf[no] = F0;
			break;
		case '1':
		case '2':
			if(s[1] == '.' && isdigit((uchar)s[2])) {
				olistf[no] = *s=='1'? F1: F2;
				olist[no] = atoi(s += 2);
				break;
			} /* fall thru */
		default:
			error("invalid -o list", "");
		}
		if(s[1] == ',')
			s++;
	}
}
