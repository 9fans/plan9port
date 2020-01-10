#include <u.h>
#include <libc.h>
#include <bio.h>
#include "plot.h"
#include <draw.h>
#include <event.h>
#include <ctype.h>

void	define(char*);
void	call(char*);
void	include(char*);
int	process(Biobuf*);
int	server(void);

enum{
	ARC,
	BOX,
	CALL,
	CFILL,
	CIRC,
	CLOSEPL,
	COLOR,
	CSPLINE,
	DEFINE,
	DISK,
	DSPLINE,
	ERASE,
	FILL,
	FRAME,
	FSPLINE,
	GRADE,
	IDLE,
	INCLUDE,
	LINE,
	LSPLINE,
	MOVE,
	OPENPL,
	PARABOLA,
	PEN,
	PAUSE,
	POINT,
	POLY,
	RANGE,
	RESTORE,
	RMOVE,
	RVEC,
	SAVE,
	SBOX,
	SPLINE,
	TEXT,
	VEC,
	LAST
};

struct pcall {
	char	*cc;
	int	numc;
} plots[] = {
	/*ARC*/ 		"a", 	1,
	/*BOX*/ 		"bo", 	2,
	/*CALL*/		"ca",	2,
	/*CFILL*/ 	"cf", 	2,
	/*CIRC*/ 		"ci", 	2,
	/*CLOSEPL*/ 	"cl", 	2,
	/*COLOR*/ 	"co", 	2,
	/*CSPLINE*/	"cs",	2,
	/*DEFINE*/	"de",	2,
	/*DISK*/		"di",	2,
	/*DSPLINE*/	"ds",	2,
	/*ERASE*/ 	"e", 	1,
	/*FILL*/ 		"fi", 	2,
	/*FRAME*/ 	"fr", 	2,
	/*FSPLINE*/	"fs",	2,
	/*GRADE*/ 	"g", 	1,
	/*IDLE*/ 		"id", 	2,
	/*INCLUDE*/	"in",	2,
	/*LINE*/ 		"li", 	2,
	/*LSPLINE*/	"ls",	2,
	/*MOVE*/ 		"m", 	1,
	/*OPENPL*/ 	"o", 	1,
	/*PARABOLA*/ 	"par", 	3,
	/*PEN*/ 		"pe", 	2,
	/*PAUSE*/ 	"pau", 	3,
	/*POINT*/ 	"poi", 	3,
	/*POLY*/ 		"pol", 	3,
	/*RANGE*/ 	"ra", 	2,
	/*RESTORE*/ 	"re", 	2,
	/*RMOVE*/ 	"rm", 	2,
	/*RVEC*/ 		"rv", 	2,
	/*SAVE*/ 		"sa", 	2,
	/*SBOX*/ 		"sb", 	2,
	/*SPLINE*/ 	"sp", 	2,
	/*TEXT*/ 		"t", 	1,
	/*VEC*/ 		"v", 	1,
	/*LAST*/	 	0, 	0
};

struct pcall *pplots;		/* last command read */

#define MAXL 16
struct fcall {
	char *name;
	char *stash;
} flibr[MAXL];			/* define strings */

struct fcall *fptr = flibr;

#define	NFSTACK	50
struct fstack{
	int peekc;
	int lineno;
	char *corebuf;
	Biobuf *fd;
	double scale;
}fstack[NFSTACK];		/* stack of open input files & defines */
struct fstack *fsp=fstack;

#define	NARGSTR	8192
char argstr[NARGSTR+1];		/* string arguments */

#define	NX	8192
double x[NX];			/* numeric arguments */

#define	NPTS	256
int cnt[NPTS];			/* control-polygon vertex counts */
double *pts[NPTS];		/* control-polygon vertex pointers */

void eresized(int new){
	if(new && getwindow(display, Refnone) < 0){
		fprint(2, "Can't reattach to window: %r\n");
		exits("resize");
	}
}
char *items[]={
	"exit",
	0
};
Menu menu={items};
void
main(int arc, char *arv[]){
	char *ap;
	Biobuf *bp;
	int fd;
	int i;
	int dflag;
	char *oflag;
	Mouse m;
	bp = 0;
	fd = dup(0, -1);		/* because openpl will close 0! */
	dflag=0;
	oflag="";
	winsize = "512x512";
	for(i=1;i!=arc;i++) if(arv[i][0]=='-') switch(arv[i][1]){
	case 'd': dflag=1; break;
	case 'o': oflag=arv[i]+2; break;
	case 's': fd=server(); break;
	}
	openpl(oflag);
	if(dflag) doublebuffer();
	for (; arc > 1; arc--, arv++) {
		if (arv[1][0] == '-') {
			ap = arv[1];
			ap++;
			switch (*ap) {
			default:
				fprint(2, "%s not allowed as argument\n", ap);
				exits("usage");
			case 'T': break;
			case 'D': break;
			case 'd': break;
			case 'o': break;
			case 's': break;
			case 'e': erase(); break;
			case 'C': closepl(); break;
			case 'w': ppause(); break;
			case 'c': color(ap+1); break;
			case 'f': cfill(ap+1); break;
			case 'p': pen(ap+1); break;
			case 'g': grade(atof(ap+1)); break;
			case 'W': winsize = ap+1; break;
			}
		}
		else if ((bp = Bopen(arv[1], OREAD)) == 0) {
			perror(arv[1]);
			fprint(2, "Cannot find file %s\n", arv[1]);
		}
		else if(process(bp)) Bterm(fsp->fd);
		else break;
	}
	if (bp == 0){
		bp = malloc(sizeof *bp);
		Binit(bp, fd, OREAD);
		process(bp);
	}
	closepl();
	for(;;){
		m=emouse();
		if(m.buttons&4 && emenuhit(3, &m, &menu)==0) exits(0);
	}
}
int nextc(void){
	int c;
	Rune r;
	for(;;){
		if(fsp->peekc!=Beof){
			c=fsp->peekc;
			fsp->peekc=Beof;
			return c;
		}
		if(fsp->fd)
			c=Bgetrune(fsp->fd);
		else if(*fsp->corebuf){
			fsp->corebuf+=chartorune(&r, fsp->corebuf);
			c=r;
		}else
			c=Beof;
		if(c!=Beof || fsp==fstack) break;
		if(fsp->fd) Bterm(fsp->fd);
		--fsp;
	}
	if(c=='\n') fsp->lineno++;
	return c;
}
/*
 * Read a string into argstr -- ignores leading spaces
 * and an optional leading quote-mark
 */
void
strarg(void){
	int c;
	Rune r;
	int quote=0;
	char *s=argstr;
	do
		c=nextc();
	while(c==' ' || c=='\t');
	if(c=='\'' || c=='"'){
		quote=c;
		c=nextc();
	}
	r = 0;
	while(c!='\n' && c!=Beof){
		r=c;
		s+=runetochar(s, &r);
		c=nextc();
	}
	if(quote && s!=argstr && r==quote) --s;
	*s='\0';
}
/*
 * Read a floating point number into argstr
 */
int
numstring(void){
	int ndp=0;
	int ndig=0;
	char *s=argstr;
	int c=nextc();
	if(c=='+' || c=='-'){
		*s++=c;
		c=nextc();
	}
	while(isdigit(c) || c=='.'){
		if(s!=&argstr[NARGSTR]) *s++=c;
		if(c=='.') ndp++;
		else ndig++;
		c=nextc();
	}
	if(ndp>1 || ndig==0){
		fsp->peekc=c;
		return 0;
	}
	if(c=='e' || c=='E'){
		if(s!=&argstr[NARGSTR]) *s++=c;
		c=nextc();
		if(c=='+' || c=='-'){
			if(s!=&argstr[NARGSTR]) *s++=c;
			c=nextc();
		}
		if(!isdigit(c)){
			fsp->peekc=c;
			return 0;
		}
		while(isdigit(c)){
			if(s!=&argstr[NARGSTR]) *s++=c;
			c=nextc();
		}
	}
	fsp->peekc=c;
	*s='\0';
	return 1;
}
/*
 * Read n numeric arguments, storing them in
 * x[0], ..., x[n-1]
 */
void
numargs(int n){
	int i, c;
	for(i=0;i!=n;i++){
		do{
			c=nextc();
		}while(strchr(" \t\n", c) || c!='.' && c!='+' && c!='-' && ispunct(c));
		fsp->peekc=c;
		if(!numstring()){
			fprint(2, "line %d: number expected\n", fsp->lineno);
			exits("input error");
		}
		x[i]=atof(argstr)*fsp->scale;
	}
}
/*
 * Read a list of lists of control vertices, storing points in x[.],
 * pointers in pts[.] and counts in cnt[.]
 */
void
polyarg(void){
	int nleft, l, r, c;
	double **ptsp=pts, *xp=x;
	int *cntp=cnt;
	do{
		c=nextc();
	}while(c==' ' || c=='\t');
	if(c=='{'){
		l='{';
		r='}';
	}
	else{
		l=r='\n';
		fsp->peekc=c;
	}
	nleft=1;
	*cntp=0;
	*ptsp=xp;
	for(;;){
		c=nextc();
		if(c==r){
			if(*cntp){
				if(*cntp&1){
					fprint(2, "line %d: phase error\n",
						fsp->lineno);
					exits("bad input");
				}
				*cntp/=2;
				if(ptsp==&pts[NPTS]){
					fprint(2, "line %d: out of polygons\n",
						fsp->lineno);
					exits("exceeded limit");
				}
				*++ptsp=xp;
				*++cntp=0;
			}
			if(--nleft==0) return;
		}
		else switch(c){
		case Beof:  return;
		case ' ':  break;
		case '\t': break;
		case '\n': break;
		case '.': case '+': case '-':
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			fsp->peekc=c;
			if(!numstring()){
				fprint(2, "line %d: expected number\n", fsp->lineno);
				exits("bad input");
			}
			if(xp==&x[NX]){
				fprint(2, "line %d: out of space\n", fsp->lineno);
				exits("exceeded limit");
			}
			*xp++=atof(argstr);
			++*cntp;
			break;
		default:
			if(c==l) nleft++;
			else if(!ispunct(c)){
				fsp->peekc=c;
				return;
			}
		}
	}
}

int
process(Biobuf *fd){
	char *s;
	int c;
	fsp=fstack;
	fsp->fd=fd;
	fsp->corebuf=0;
	fsp->peekc=Beof;
	fsp->lineno=1;
	fsp->scale=1.;
	for(;;){
		do
			c=nextc();
		while(c==' ' || c=='\t');
		if(c==':'){
			do
				c=nextc();
			while(c!='\n' && c!=Beof);
			if(c==Beof) break;
			continue;
		}
		while(c=='.'){
			c=nextc();
			if(isdigit(c)){
				if(fsp->fd) Bungetc(fsp->fd);
				else --fsp->corebuf;
				c='.';
				break;
			}
		}
		if(c==Beof) break;
		if(c=='\n') continue;
		if(isalpha(c)){
			s=argstr;
			do{
				if(isupper(c)) c=tolower(c);
				if(s!=&argstr[NARGSTR]) *s++=c;
				c=nextc();
			}while(isalpha(c));
			fsp->peekc=c;
			*s='\0';
			for(pplots=plots;pplots->cc;pplots++)
				if(strncmp(argstr, pplots->cc, pplots->numc)==0)
					break;
			if(pplots->cc==0){
				fprint(2, "line %d, %s unknown\n", fsp->lineno,
					argstr);
				exits("bad command");
			}
		}
		else{
			fsp->peekc=c;
		}
		if(!pplots){
			fprint(2, "line %d, no command!\n", fsp->lineno);
			exits("no command");
		}
		switch(pplots-plots){
		case ARC:	numargs(7); rarc(x[0],x[1],x[2],x[3],x[4],x[5],x[6]); break;
		case BOX:	numargs(4); box(x[0], x[1], x[2], x[3]); break;
		case CALL:	strarg();   call(argstr); pplots=0; break;
		case CFILL:	strarg();   cfill(argstr); pplots=0; break;
		case CIRC:	numargs(3); circ(x[0], x[1], x[2]); break;
		case CLOSEPL:	strarg();   closepl(); pplots=0; break;
		case COLOR:	strarg();   color(argstr); pplots=0; break;
		case CSPLINE:	polyarg();  splin(4, cnt, pts); break;
		case DEFINE:	strarg();   define(argstr); pplots=0; break;
		case DISK:	numargs(3); plotdisc(x[0], x[1], x[2]); break;
		case DSPLINE:	polyarg();  splin(3, cnt, pts); break;
		case ERASE:	strarg();   erase(); pplots=0; break;
		case FILL:	polyarg();  fill(cnt, pts); break;
		case FRAME:	numargs(4); frame(x[0], x[1], x[2], x[3]); break;
		case FSPLINE:	polyarg();  splin(1, cnt, pts); break;
		case GRADE:	numargs(1); grade(x[0]); break;
		case IDLE:	strarg();   idle(); pplots=0; break;
		case INCLUDE:	strarg();   include(argstr); pplots=0; break;
		case LINE:	numargs(4); plotline(x[0], x[1], x[2], x[3]); break;
		case LSPLINE:	polyarg();  splin(2, cnt, pts); break;
		case MOVE:	numargs(2); move(x[0], x[1]); break;
		case OPENPL:	strarg();   openpl(argstr); pplots=0; break;
		case PARABOLA:	numargs(6); parabola(x[0],x[1],x[2],x[3],x[4],x[5]); break;
		case PAUSE:	strarg();   ppause(); pplots=0; break;
		case PEN:	strarg();   pen(argstr); pplots=0; break;
		case POINT:	numargs(2); dpoint(x[0], x[1]); break;
		case POLY:	polyarg();  plotpoly(cnt, pts); break;
		case RANGE:	numargs(4); range(x[0], x[1], x[2], x[3]); break;
		case RESTORE:	strarg();   restore(); pplots=0; break;
		case RMOVE:	numargs(2); rmove(x[0], x[1]); break;
		case RVEC:	numargs(2); rvec(x[0], x[1]); break;
		case SAVE:	strarg();   save(); pplots=0; break;
		case SBOX:	numargs(4); sbox(x[0], x[1], x[2], x[3]); break;
		case SPLINE:	polyarg();  splin(0, cnt, pts); break;
		case TEXT:	strarg();   text(argstr); pplots=0; break;
		case VEC:	numargs(2); vec(x[0], x[1]); break;
		default:
			fprint(2, "plot: missing case %ld\n", pplots-plots);
			exits("internal error");
		}
	}
	return 1;
}
char *names = 0;
char *enames = 0;
char *bstash = 0;
char *estash = 0;
unsigned size = 1024;
char *nstash = 0;
void define(char *a){
	char	*ap;
	short	i, j;
	int curly = 0;
	ap = a;
	while(isalpha((uchar)*ap))ap++;
	if(ap == a){
		fprint(2,"no name with define\n");
		exits("define");
	}
	i = ap - a;
	if(names+i+1 > enames){
		names = malloc((unsigned)512);
		enames = names + 512;
	}
	fptr->name = names;
	strncpy(names, a,i);
	names += i;
	*names++ = '\0';
	if(!bstash){
		bstash = nstash = malloc(size);
		estash = bstash + size;
	}
	fptr->stash = nstash;
	while(*ap != '{')
		if(*ap == '\n'){
			if((ap=Brdline(fsp->fd, '\n'))==0){
				fprint(2,"unexpected end of file\n");
				exits("eof");
			}
		}
		else ap++;
	while((j=Bgetc(fsp->fd))!= Beof){
		if(j == '{')curly++;
		else if(j == '}'){
			if(curly == 0)break;
			else curly--;
		}
		*nstash++ = j;
		if(nstash == estash){
			free(bstash);
			size += 1024;
			bstash = realloc(bstash,size);
			estash = bstash+size;
		}
	}
	*nstash++ = '\0';
	if(fptr++ >= &flibr[MAXL]){
		fprint(2,"Too many objects\n");
		exits("too many objects");
	}
}
void call(char *a){
	char *ap;
	struct fcall *f;
	char sav;
	double SC;
	ap = a;
	while(isalpha((uchar)*ap))ap++;
	sav = *ap;
	*ap = '\0';
	for(f=flibr;f<fptr;f++){
		if (!(strcmp(a, f->name)))
			break;
	}
	if(f == fptr){
		fprint(2, "object %s not defined\n",a);
		exits("undefined");
	}
	*ap = sav;
	while (isspace((uchar)*ap) || *ap == ',')
		ap++;
	if (*ap != '\0')
		SC = atof(ap);
	else SC = 1.;
	if(++fsp==&fstack[NFSTACK]){
		fprint(2, "input stack overflow\n");
		exits("blew stack");
	}
	fsp->peekc=Beof;
	fsp->lineno=1;
	fsp->corebuf=f->stash;
	fsp->fd=0;
	fsp->scale=fsp[-1].scale*SC;
}
void include(char *a){
	Biobuf *fd;
	fd=Bopen(a, OREAD);
	if(fd==0){
		perror(a);
		exits("can't include");
	}
	if(++fsp==&fstack[NFSTACK]){
		fprint(2, "input stack overflow\n");
		exits("blew stack");
	}
	fsp->peekc=Beof;
	fsp->lineno=1;
	fsp->corebuf=0;
	fsp->fd=fd;
}
/*
 * Doesn't work.  Why?
 */
int server(void){
	int fd, p[2];
	char buf[32];
	pipe(p);
	fd = create("/srv/plot", 1, 0666);
	sprint(buf, "%d", p[1]);
	write(fd, buf, strlen(buf));
	close(fd);
	close(p[1]);
	return p[0];
}
