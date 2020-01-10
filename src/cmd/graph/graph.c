#include <u.h>
#include <libc.h>
#include <stdio.h>
#include "iplot.h"
#define	INF	1.e+37
#define	F	.25

struct xy {
	int	xlbf;		/*flag:explicit lower bound*/
	int 	xubf;		/*flag:explicit upper bound*/
	int	xqf;		/*flag:explicit quantum*/
	double (*xf)(double);	/*transform function, e.g. log*/
	float	xa,xb;		/*scaling coefficients*/
	float	xlb,xub;	/*lower and upper bound*/
	float	xquant;		/*quantum*/
	float	xoff;		/*screen offset fraction*/
	float	xsize;		/*screen fraction*/
	int	xbot,xtop;	/*screen coords of border*/
	float	xmult;		/*scaling constant*/
} xd,yd;
struct val {
	float xv;
	float yv;
	int lblptr;
} *xx;

char *labels;
int labelsiz;

int tick = 50;
int top = 4000;
int bot = 200;
float absbot;
int	n;
int	erasf = 1;
int	gridf = 2;
int	symbf = 0;
int	absf = 0;
int	transf;
int	equf;
int	brkf;
int	ovlay = 1;
float	dx;
char	*plotsymb;

#define BSIZ 80
char	labbuf[BSIZ];
char	titlebuf[BSIZ];

char *modes[] = {
	"disconnected",
	"solid",
	"dotted",
	"dotdashed",
	"shortdashed",
	"longdashed"
};
int mode = 1;
double ident(double x){
	return(x);
}

struct z {
	float lb,ub,mult,quant;
};

struct {
	char *name;
	int next;
} palette[256];

static char* colors[] = {
	"blue",
	"cyan",
	"green",
	"kblack",
	"magenta",
	"red",
	"white",
	"yellow"
};
static void
initpalette(void)
{
	int i;

	for(i=0; i<nelem(colors); i++){
		palette[(uchar)colors[i][0]].name = colors[i];
		palette[(uchar)colors[i][0]].next = colors[i][0];
	}
}

int pencolor = 'k';

void init(struct xy *);
void setopt(int, char *[]);
void readin(void);
void transpose(void);
void getlim(struct xy *, struct val *);
void equilibrate(struct xy *, struct xy *);
void scale(struct xy *);
void limread(struct xy *, int *, char ***);
int numb(float *, int *, char ***);
void colread(int *, char ***);
int copystring(int);
struct z setloglim(int, int, float, float);
struct z setlinlim(int, int, float, float);
void axes(void);
int setmark(int *, struct xy *);
void submark(int *, int *, float, struct xy *);
void plot(void);
int getfloat(float *);
int getstring(void);
void title(void);
void badarg(void);
int conv(float, struct xy *, int *);
int symbol(int, int, int);
void axlab(char, struct xy *, char *);

int main(int argc,char *argv[]){

	initpalette();
	openpl();
	range(0,0,4096,4096);
	init(&xd);
	init(&yd);
	xd.xsize = yd.xsize = 1.;
	xx = (struct val *)malloc((unsigned)sizeof(struct val));
	labels = malloc(1);
	labels[labelsiz++] = 0;
	setopt(argc,argv);
	if(erasf)
		erase();
	readin();
	transpose();
	getlim(&xd,(struct val *)&xx->xv);
	getlim(&yd,(struct val *)&xx->yv);
	if(equf) {
		equilibrate(&xd,&yd);
		equilibrate(&yd,&xd);
	}
	scale(&xd);
	scale(&yd);
	axes();
	title();
	plot();
	closepl();
	exits(0);
	return 0;	/* gcc */
}

void init(struct xy *p){
	p->xf = ident;
	p->xmult = 1;
}

void setopt(int argc, char *argv[]){
	char *p1, *p2;
	float temp;

	xd.xlb = yd.xlb = INF;
	xd.xub = yd.xub = -INF;
	while(--argc > 0) {
		argv++;
again:		switch(argv[0][0]) {
		case '-':
			argv[0]++;
			goto again;
		case 'l': /* label for plot */
			p1 = titlebuf;
			if (argc>=2) {
				argv++;
				argc--;
				p2 = argv[0];
				while (*p1++ = *p2++);
			}
			break;

		case 'd':	/*disconnected,obsolete option*/
		case 'm': /*line mode*/
			mode = 0;
			if(!numb(&temp,&argc,&argv))
				break;
			if(temp>=sizeof(modes)/sizeof(*modes))
				mode = 1;
			else if(temp>=-1)
				mode = temp;
			break;

		case 'o':
			if(numb(&temp,&argc,&argv) && temp>=1)
				ovlay = temp;
			break;
		case 'a': /*automatic abscissas*/
			absf = 1;
			dx = 1;
			if(!numb(&dx,&argc,&argv))
				break;
			if(numb(&absbot,&argc,&argv))
				absf = 2;
			break;

		case 's': /*save screen, overlay plot*/
			erasf = 0;
			break;

		case 'g': /*grid style 0 none, 1 ticks, 2 full*/
			gridf = 0;
			if(!numb(&temp,&argc,&argv))
				temp = argv[0][1]-'0';	/*for caompatibility*/
			if(temp>=0&&temp<=2)
				gridf = temp;
			break;

		case 'c': /*character(s) for plotting*/
			if(argc >= 2) {
				symbf = 1;
				plotsymb = argv[1];
				argv++;
				argc--;
			}
			break;

		case 't':	/*transpose*/
			transf = 1;
			break;
		case 'e':	/*equal scales*/
			equf = 1;
			break;
		case 'b':	/*breaks*/
			brkf = 1;
			break;
		case 'x':	/*x limits */
			limread(&xd,&argc,&argv);
			break;
		case 'y':
			limread(&yd,&argc,&argv);
			break;
		case 'h': /*set height of plot */
			if(!numb(&yd.xsize, &argc,&argv))
				badarg();
			break;
		case 'w': /*set width of plot */
			if(!numb(&xd.xsize, &argc, &argv))
				badarg();
			break;
		case 'r': /* set offset to right */
			if(!numb(&xd.xoff, &argc, &argv))
				badarg();
			break;
		case 'u': /*set offset up the screen*/
			if(!numb(&yd.xoff,&argc,&argv))
				badarg();
			break;
		case 'p': /*pen color*/
			colread(&argc, &argv);
			break;
		default:
			badarg();
		}
	}
}

void limread(struct xy *p, int *argcp, char ***argvp){
	if(*argcp>1 && (*argvp)[1][0]=='l') {
		(*argcp)--;
		(*argvp)++;
		p->xf = log10;
	}
	if(!numb(&p->xlb,argcp,argvp))
		return;
	p->xlbf = 1;
	if(!numb(&p->xub,argcp,argvp))
		return;
	p->xubf = 1;
	if(!numb(&p->xquant,argcp,argvp))
		return;
	p->xqf = 1;
}

#ifdef NOTDEF
isdigit(char c){
	return '0'<=c && c<='9';
}
#endif

int
numb(float *np, int *argcp, char ***argvp){
	char c;

	if(*argcp <= 1)
		return(0);
	while((c=(*argvp)[1][0]) == '+')
		(*argvp)[1]++;
	if(!(isdigit((uchar)c) || c=='-'&&(*argvp)[1][1]<'A' || c=='.'))
		return(0);
	*np = atof((*argvp)[1]);
	(*argcp)--;
	(*argvp)++;
	return(1);
}

void colread(int *argcp, char ***argvp){
	int c, cnext;
	int i, n;

	if(*argcp<=1)
		return;
	n = strlen((*argvp)[1]);
	if(strspn((*argvp)[1], "bcgkmrwy")!=n)
		return;
	pencolor = cnext = (*argvp)[1][0];
	for(i=0; i<n-1; i++){
		c = (unsigned char)(*argvp)[1][i];
		cnext = (unsigned char)(*argvp)[1][i+1];
		palette[c].next = cnext;
	}
	palette[cnext].next = pencolor;
	(*argcp)--;
	(*argvp)++;
}

void readin(void){
	int i, t;
	struct val *temp;

	if(absf==1) {
		if(xd.xlbf)
			absbot = xd.xlb;
		else if(xd.xf==log10)
			absbot = 1;
	}
	for(;;) {
		temp = (struct val *)realloc((char*)xx,
			(unsigned)(n+ovlay)*sizeof(struct val));
		if(temp==0)
			return;
		xx = temp;
		if(absf)
			xx[n].xv = n*dx/ovlay + absbot;
		else
			if(!getfloat(&xx[n].xv))
				return;
		t = 0;	/* silence compiler */
		for(i=0;i<ovlay;i++) {
			xx[n+i].xv = xx[n].xv;
			if(!getfloat(&xx[n+i].yv))
				return;
			xx[n+i].lblptr = -1;
			t = getstring();
			if(t>0)
				xx[n+i].lblptr = copystring(t);
			if(t<0 && i+1<ovlay)
				return;
		}
		n += ovlay;
		if(t<0)
			return;
	}
}

void transpose(void){
	int i;
	float f;
	struct xy t;
	if(!transf)
		return;
	t = xd; xd = yd; yd = t;
	for(i= 0;i<n;i++) {
		f = xx[i].xv; xx[i].xv = xx[i].yv; xx[i].yv = f;
	}
}

int copystring(int k){
	char *temp;
	int i;
	int q;

	temp = realloc(labels,(unsigned)(labelsiz+1+k));
	if(temp==0)
		return(0);
	labels = temp;
	q = labelsiz;
	for(i=0;i<=k;i++)
		labels[labelsiz++] = labbuf[i];
	return(q);
}

float modceil(float f, float t){

	t = fabs(t);
	return(ceil(f/t)*t);
}

float
modfloor(float f, float t){
	t = fabs(t);
	return(floor(f/t)*t);
}

void getlim(struct xy *p, struct val *v){
	int i;

	i = 0;
	do {
		if(!p->xlbf && p->xlb>v[i].xv)
			p->xlb = v[i].xv;
		if(!p->xubf && p->xub<v[i].xv)
			p->xub = v[i].xv;
		i++;
	} while(i < n);
}

void setlim(struct xy *p){
	float t,delta,sign;
	struct z z;
	int mark[50];
	float lb,ub;
	int lbf,ubf;

	lb = p->xlb;
	ub = p->xub;
	delta = ub-lb;
	if(p->xqf) {
		if(delta*p->xquant <=0 )
			badarg();
		return;
	}
	sign = 1;
	lbf = p->xlbf;
	ubf = p->xubf;
	if(delta < 0) {
		sign = -1;
		t = lb;
		lb = ub;
		ub = t;
		t = lbf;
		lbf = ubf;
		ubf = t;
	}
	else if(delta == 0) {
		if(ub > 0) {
			ub = 2*ub;
			lb = 0;
		}
		else
			if(lb < 0) {
				lb = 2*lb;
				ub = 0;
			}
			else {
				ub = 1;
				lb = -1;
			}
	}
	if(p->xf==log10 && lb>0 && ub>lb) {
		z = setloglim(lbf,ubf,lb,ub);
		p->xlb = z.lb;
		p->xub = z.ub;
		p->xmult *= z.mult;
		p->xquant = z.quant;
		if(setmark(mark,p)<2) {
			p->xqf = lbf = ubf = 1;
			lb = z.lb; ub = z.ub;
		} else
			return;
	}
	z = setlinlim(lbf,ubf,lb,ub);
	if(sign > 0) {
		p->xlb = z.lb;
		p->xub = z.ub;
	} else {
		p->xlb = z.ub;
		p->xub = z.lb;
	}
	p->xmult *= z.mult;
	p->xquant = sign*z.quant;
}

struct z
setloglim(int lbf, int ubf, float lb, float ub){
	float r,s,t;
	struct z z;

	for(s=1; lb*s<1; s*=10) ;
	lb *= s;
	ub *= s;
	for(r=1; 10*r<=lb; r*=10) ;
	for(t=1; t<ub; t*=10) ;
	z.lb = !lbf ? r : lb;
	z.ub = !ubf ? t : ub;
	if(ub/lb<100) {
		if(!lbf) {
			if(lb >= 5*z.lb)
				z.lb *= 5;
			else if(lb >= 2*z.lb)
				z.lb *= 2;
		}
		if(!ubf) {
			if(ub*5 <= z.ub)
				z.ub /= 5;
			else if(ub*2 <= z.ub)
				z.ub /= 2;
		}
	}
	z.mult = s;
	z.quant = r;
	return(z);
}

struct z
setlinlim(int lbf, int ubf, float xlb, float xub){
	struct z z;
	float r,s,delta;
	float ub,lb;

loop:
	ub = xub;
	lb = xlb;
	delta = ub - lb;
	/*scale up by s, a power of 10, so range (delta) exceeds 1*/
	/*find power of 10 quantum, r, such that delta/10<=r<delta*/
	r = s = 1;
	while(delta*s < 10)
		s *= 10;
	delta *= s;
	while(10*r < delta)
		r *= 10;
	lb *= s;
	ub *= s;
	/*set r=(1,2,5)*10**n so that 3-5 quanta cover range*/
	if(r>=delta/2)
		r /= 2;
	else if(r<delta/5)
		r *= 2;
	z.ub = ubf? ub: modceil(ub,r);
	z.lb = lbf? lb: modfloor(lb,r);
	if(!lbf && z.lb<=r && z.lb>0) {
		xlb = 0;
		goto loop;
	}
	else if(!ubf && z.ub>=-r && z.ub<0) {
		xub = 0;
		goto loop;
	}
	z.quant = r;
	z.mult = s;
	return(z);
}

void scale(struct xy *p){
	float edge;

	setlim(p);
	edge = top-bot;
	p->xa = p->xsize*edge/((*p->xf)(p->xub) - (*p->xf)(p->xlb));
	p->xbot = bot + edge*p->xoff;
	p->xtop = p->xbot + (top-bot)*p->xsize;
	p->xb = p->xbot - (*p->xf)(p->xlb)*p->xa + .5;
}

void equilibrate(struct xy *p, struct xy *q){
	if(p->xlbf||	/* needn't test xubf; it implies xlbf*/
	   q->xubf&&q->xlb>q->xub)
		return;
	if(p->xlb>q->xlb) {
		p->xlb = q->xlb;
		p->xlbf = q->xlbf;
	}
	if(p->xub<q->xub) {
		p->xub = q->xub;
		p->xubf = q->xubf;
	}
}

void axes(void){
	int i;
	int mark[50];
	int xn, yn;
	if(gridf==0)
		return;

	line(xd.xbot,yd.xbot,xd.xtop,yd.xbot);
	vec(xd.xtop,yd.xtop);
	vec(xd.xbot,yd.xtop);
	vec(xd.xbot,yd.xbot);

	xn = setmark(mark,&xd);
	for(i=0; i<xn; i++) {
		if(gridf==2)
			line(mark[i],yd.xbot,mark[i],yd.xtop);
		if(gridf==1) {
			line(mark[i],yd.xbot,mark[i],yd.xbot+tick);
			line(mark[i],yd.xtop-tick,mark[i],yd.xtop);
		}
	}
	yn = setmark(mark,&yd);
	for(i=0; i<yn; i++) {
		if(gridf==2)
			line(xd.xbot,mark[i],xd.xtop,mark[i]);
		if(gridf==1) {
			line(xd.xbot,mark[i],xd.xbot+tick,mark[i]);
			line(xd.xtop-tick,mark[i],xd.xtop,mark[i]);
		}
	}
}

int
setmark(int *xmark, struct xy *p){
	int xn = 0;
	float x,xl,xu;
	float q;
	if(p->xf==log10&&!p->xqf) {
		for(x=p->xquant; x<p->xub; x*=10) {
			submark(xmark,&xn,x,p);
			if(p->xub/p->xlb<=100) {
				submark(xmark,&xn,2*x,p);
				submark(xmark,&xn,5*x,p);
			}
		}
	} else {
		xn = 0;
		q = p->xquant;
		if(q>0) {
			xl = modceil(p->xlb+q/6,q);
			xu = modfloor(p->xub-q/6,q)+q/2;
		} else {
			xl = modceil(p->xub-q/6,q);
			xu = modfloor(p->xlb+q/6,q)-q/2;
		}
		for(x=xl; x<=xu; x+=fabs(p->xquant))
			xmark[xn++] = (*p->xf)(x)*p->xa + p->xb;
	}
	return(xn);
}
void submark(int *xmark, int *pxn, float x, struct xy *p){
	if(1.001*p->xlb < x && .999*p->xub > x)
		xmark[(*pxn)++] = log10(x)*p->xa + p->xb;
}

void plot(void){
	int ix,iy;
	int i,j;
	int conn;

	for(j=0;j<ovlay;j++) {
		switch(mode) {
		case -1:
			pen(modes[j%(sizeof modes/sizeof *modes-1)+1]);
			break;
		case 0:
			break;
		default:
			pen(modes[mode]);
		}
		color(palette[pencolor].name);
		conn = 0;
		for(i=j; i<n; i+=ovlay) {
			if(!conv(xx[i].xv,&xd,&ix) ||
			   !conv(xx[i].yv,&yd,&iy)) {
				conn = 0;
				continue;
			}
			if(mode!=0) {
				if(conn != 0)
					vec(ix,iy);
				else
					move(ix,iy);
				conn = 1;
			}
			conn &= symbol(ix,iy,xx[i].lblptr);
		}
		pencolor = palette[pencolor].next;
	}
	pen(modes[1]);
}

int
conv(float xv, struct xy *p, int *ip){
	long ix;
	ix = p->xa*(*p->xf)(xv*p->xmult) + p->xb;
	if(ix<p->xbot || ix>p->xtop)
		return(0);
	*ip = ix;
	return(1);
}

int
getfloat(float *p){
	int i;

	i = scanf("%f",p);
	return(i==1);
}

int
getstring(void){
	int i;
	char junk[20];
	i = scanf("%1s",labbuf);
	if(i==-1)
		return(-1);
	switch(*labbuf) {
	default:
		if(!isdigit((uchar)*labbuf)) {
			ungetc(*labbuf,stdin);
			i = scanf("%s",labbuf);
			break;
		}
	case '.':
	case '+':
	case '-':
		ungetc(*labbuf,stdin);
		return(0);
	case '"':
		i = scanf("%[^\"\n]",labbuf);
		scanf("%[\"]",junk);
		break;
	}
	if(i==-1)
		return(-1);
	return(strlen(labbuf));
}

int
symbol(int ix, int iy, int k){

	if(symbf==0&&k<0) {
		if(mode==0)
			point(ix,iy);
		return(1);
	}
	else {
		move(ix,iy);
		text(k>=0?labels+k:plotsymb);
		move(ix,iy);
		return(!brkf|k<0);
	}
}

void title(void){
	char buf[BSIZ+100];
	buf[0] = ' ';
	buf[1] = ' ';
	buf[2] = ' ';
	strcpy(buf+3,titlebuf);
	if(erasf&&gridf) {
		axlab('x',&xd,buf);
		strcat(buf,",");
		axlab('y',&yd,buf);
	}
	move(xd.xbot,yd.xbot-60);
	text(buf);
}

void axlab(char c, struct xy *p, char *b){
	char *dir;
	dir = p->xlb<p->xub? "<=": ">=";
	sprintf(b+strlen(b), " %g %s %c%s %s %g", p->xlb/p->xmult,
		dir, c, p->xf==log10?" (log)":"", dir, p->xub/p->xmult);
}

void badarg(void){
	fprintf(stderr,"graph: error in arguments\n");
	closepl();
	exits("bad arg");
}
