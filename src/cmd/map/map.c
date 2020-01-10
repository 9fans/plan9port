#include <u.h>
#include <libc.h>
#include <stdio.h>
#include "map.h"
#include "iplot.h"

#define NVERT 20	/* max number of vertices in a -v polygon */
#define HALFWIDTH 8192	/* output scaled to fit in -HALFWIDTH,HALFWIDTH */
#define LONGLINES (HALFWIDTH*4)	/* permissible segment lengths */
#define SHORTLINES (HALFWIDTH/8)
#define SCALERATIO 10	/* of abs to rel data (see map(5)) */
#define RESOL 2.	/* coarsest resolution for tracing grid (degrees) */
#define TWO_THRD 0.66666666666666667

int normproj(double, double, double *, double *);
int posproj(double, double, double *, double *);
int picut(struct place *, struct place *, double *);
double reduce(double);
short getshort(FILE *);
char *mapindex(char *);
proj projection;


static char *mapdir = "#9/map";	/* default map directory */
struct file {
	char *name;
	char *color;
	char *style;
};
static struct file dfltfile = {
	"world", BLACK, SOLID	/* default map */
};
static struct file *file = &dfltfile;	/* list of map files */
static int nfile = 1;			/* length of list */
static char *currcolor = BLACK;		/* current color */
static char *gridcolor = BLACK;
static char *bordcolor = BLACK;

extern struct index index[];
int halfwidth = HALFWIDTH;

static int (*cut)(struct place *, struct place *, double *);
static int (*limb)(double*, double*, double);
static void dolimb(void);
static int onlimb;
static int poles;
static double orientation[3] = { 90., 0., 0. };	/* -o option */
static int oriented;	/* nonzero if -o option occurred */
static int upright;		/* 1 if orientation[0]==90, -1 if -90, else 0*/
static int delta = 1;	/* -d setting */
static double limits[4] = {	/* -l parameters */
	-90., 90., -180., 180.
};
static double klimits[4] = {	/* -k parameters */
	-90., 90., -180., 180.
};
static int limcase;
static double rlimits[4];	/* limits expressed in radians */
static double lolat, hilat, lolon, hilon;
static double window[4] = {	/* option -w */
	-90., 90., -180., 180.
};
static int windowed;	/* nozero if option -w */
static struct vert { double x, y; } v[NVERT+2];	/*clipping polygon*/
static struct edge { double a, b, c; } e[NVERT]; /* coeffs for linear inequality */
static int nvert;	/* number of vertices in clipping polygon */

static double rwindow[4];	/* window, expressed in radians */
static double params[2];		/* projection params */
/* bounds on output values before scaling; found by coarse survey */
static double xmin = 100.;
static double xmax = -100.;
static double ymin = 100.;
static double ymax = -100.;
static double xcent, ycent;
static double xoff, yoff;
double xrange, yrange;
static int left = -HALFWIDTH;
static int right = HALFWIDTH;
static int bottom = -HALFWIDTH;
static int top = HALFWIDTH;
static int longlines = SHORTLINES; /* drop longer segments */
static int shortlines = SHORTLINES;
static int bflag = 1;	/* 0 for option -b */
static int s1flag = 0;	/* 1 for option -s1 */
static int s2flag = 0;	/* 1 for option -s2 */
static int rflag = 0;	/* 1 for option -r */
static int kflag = 0;	/* 1 if option -k occurred */
static int xflag = 0;	/* 1 for option -x */
       int vflag = 1;	/* -1 if option -v occurred */
static double position[3];	/* option -p */
static double center[3] = {0., 0., 0.};	/* option -c */
static struct coord crot;		/* option -c */
static double grid[3] = { 10., 10., RESOL };	/* option -g */
static double dlat, dlon;	/* resolution for tracing grid in lat and lon */
static double scaling;	/* to compute final integer output */
static struct file *track;	/* options -t and -u */
static int ntrack;		/* number of tracks present */
static char *symbolfile;	/* option -y */

void	clamp(double *px, double v);
void	clipinit(void);
double	diddle(struct place *, double, double);
double	diddle(struct place *, double, double);
void	dobounds(double, double, double, double, int);
void	dogrid(double, double, double, double);
int	duple(struct place *, double);
#define fmax map_fmax
#define fmin map_fmin
double	fmax(double, double);
double	fmin(double, double);
void	getdata(char *);
int	gridpt(double, double, int);
int	inpoly(double, double);
int	inwindow(struct place *);
void	pathnames(void);
int	pnorm(double);
void	radbds(double *w, double *rw);
void	revlon(struct place *, double);
void	satellite(struct file *);
int	seeable(double, double);
void	windlim(void);
void	realcut(void);

int
option(char *s)
{

	if(s[0]=='-' && (s[1]<'0'||s[1]>'9'))
		return(s[1]!='.'&&s[1]!=0);
	else
		return(0);
}

void
conv(int k, struct coord *g)
{
	g->l = (0.0001/SCALERATIO)*k;
	sincos(g);
}

int
main(int argc, char *argv[])
{
	int i,k;
	char *s, *t, *style;
	double x, y;
	double lat, lon;
	double *wlim;
	double dd;
	if(sizeof(short)!=2)
		abort();	/* getshort() won't work */
	mapdir = unsharp(mapdir);
	s = getenv("MAP");
	if(s)
		file[0].name = s;
	s = getenv("MAPDIR");
	if(s)
		mapdir = s;
	if(argc<=1)
		error("usage: map projection params options");
	for(k=0;index[k].name;k++) {
		s = index[k].name;
		t = argv[1];
		while(*s == *t){
			if(*s==0) goto found;
			s++;
			t++;
		}
	}
	fprintf(stderr,"projections:\n");
	for(i=0;index[i].name;i++)  {
		fprintf(stderr,"%s",index[i].name);
		for(k=0; k<index[i].npar; k++)
			fprintf(stderr," p%d", k);
		fprintf(stderr,"\n");
	}
	exits("error");
found:
	argv += 2;
	argc -= 2;
	cut = index[k].cut;
	limb = index[k].limb;
	poles = index[k].poles;
	for(i=0;i<index[k].npar;i++) {
		if(i>=argc||option(argv[i])) {
			fprintf(stderr,"%s needs %d params\n",index[k].name,index[k].npar);
			exits("error");
		}
		params[i] = atof(argv[i]);
	}
	argv += i;
	argc -= i;
	while(argc>0&&option(argv[0])) {
		argc--;
		argv++;
		switch(argv[-1][1]) {
		case 'm':
			if(file == &dfltfile) {
				file = 0;
				nfile = 0;
			}
			while(argc && !option(*argv)) {
				file = realloc(file,(nfile+1)*sizeof(*file));
				file[nfile].name = *argv;
				file[nfile].color = currcolor;
				file[nfile].style = SOLID;
				nfile++;
				argv++;
				argc--;
			}
			break;
		case 'b':
			bflag = 0;
			for(nvert=0;nvert<NVERT&&argc>=2;nvert++) {
				if(option(*argv))
					break;
				v[nvert].x = atof(*argv++);
				argc--;
				if(option(*argv))
					break;
				v[nvert].y = atof(*argv++);
				argc--;
			}
			if(nvert>=NVERT)
				error("too many clipping vertices");
			break;
		case 'g':
			gridcolor = currcolor;
			for(i=0;i<3&&argc>i&&!option(argv[i]);i++)
				grid[i] = atof(argv[i]);
			switch(i) {
			case 0:
				grid[0] = grid[1] = 0.;
				break;
			case 1:
				grid[1] = grid[0];
			}
			argc -= i;
			argv += i;
			break;
		case 't':
			style = SOLID;
			goto casetu;
		case 'u':
			style = DOTDASH;
		casetu:
			while(argc && !option(*argv)) {
				track = realloc(track,(ntrack+1)*sizeof(*track));
				track[ntrack].name = *argv;
				track[ntrack].color = currcolor;
				track[ntrack].style = style;
				ntrack++;
				argv++;
				argc--;
			}
			break;
		case 'r':
			rflag++;
			break;
		case 's':
			switch(argv[-1][2]) {
			case '1':
				s1flag++;
				break;
			case 0:		/* compatibility */
			case '2':
				s2flag++;
			}
			break;
		case 'o':
			for(i=0;i<3&&i<argc&&!option(argv[i]);i++)
				orientation[i] = atof(argv[i]);
			oriented++;
			argv += i;
			argc -= i;
			break;
		case 'l':
			bordcolor = currcolor;
			for(i=0;i<argc&&i<4&&!option(argv[i]);i++)
				limits[i] = atof(argv[i]);
			argv += i;
			argc -= i;
			break;
		case 'k':
			kflag++;
			for(i=0;i<argc&&i<4&&!option(argv[i]);i++)
				klimits[i] = atof(argv[i]);
			argv += i;
			argc -= i;
			break;
		case 'd':
			if(argc>0&&!option(argv[0])) {
				delta = atoi(argv[0]);
				argv++;
				argc--;
			}
			break;
		case 'w':
			bordcolor = currcolor;
			windowed++;
			for(i=0;i<argc&&i<4&&!option(argv[i]);i++)
				window[i] = atof(argv[i]);
			argv += i;
			argc -= i;
			break;
		case 'c':
			for(i=0;i<3&&argc>i&&!option(argv[i]);i++)
				center[i] = atof(argv[i]);
			argc -= i;
			argv += i;
			break;
		case 'p':
			for(i=0;i<3&&argc>i&&!option(argv[i]);i++)
				position[i] = atof(argv[i]);
			argc -= i;
			argv += i;
			if(i!=3||position[2]<=0)
				error("incomplete positioning");
			break;
		case 'y':
			if(argc>0&&!option(argv[0])) {
				symbolfile = argv[0];
				argc--;
				argv++;
			}
			break;
		case 'v':
			if(index[k].limb == 0)
				error("-v does not apply here");
			vflag = -1;
			break;
		case 'x':
			xflag = 1;
			break;
		case 'C':
			if(argc && !option(*argv)) {
				currcolor = colorcode(*argv);
				argc--;
				argv++;
			}
			break;
		}
	}
	if(argc>0)
		error("error in arguments");
	pathnames();
	clamp(&limits[0],-90.);
	clamp(&limits[1],90.);
	clamp(&klimits[0],-90.);
	clamp(&klimits[1],90.);
	clamp(&window[0],-90.);
	clamp(&window[1],90.);
	radbds(limits,rlimits);
	limcase = limits[2]<-180.?0:
		  limits[3]>180.?2:
		  1;
	if(
		window[0]>=window[1]||
		window[2]>=window[3]||
		window[0]>90.||
		window[1]<-90.||
		window[2]>180.||
		window[3]<-180.)
		error("unreasonable window");
	windlim();
	radbds(window,rwindow);
	upright = orientation[0]==90? 1: orientation[0]==-90? -1: 0;
	if(index[k].spheroid && !upright)
		error("can't tilt the spheroid");
	if(limits[2]>limits[3])
		limits[3] += 360;
	if(!oriented)
		orientation[2] = (limits[2]+limits[3])/2;
	orient(orientation[0],orientation[1],orientation[2]);
	projection = (*index[k].prog)(params[0],params[1]);
	if(projection == 0)
		error("unreasonable projection parameters");
	clipinit();
	grid[0] = fabs(grid[0]);
	grid[1] = fabs(grid[1]);
	if(!kflag)
		for(i=0;i<4;i++)
			klimits[i] = limits[i];
	if(klimits[2]>klimits[3])
		klimits[3] += 360;
	lolat = limits[0];
	hilat = limits[1];
	lolon = limits[2];
	hilon = limits[3];
	if(lolon>=hilon||lolat>=hilat||lolat<-90.||hilat>90.)
		error("unreasonable limits");
	wlim = kflag? klimits: window;
	dlat = fmin(hilat-lolat,wlim[1]-wlim[0])/16;
	dlon = fmin(hilon-lolon,wlim[3]-wlim[2])/32;
	dd = fmax(dlat,dlon);
	while(grid[2]>fmin(dlat,dlon)/2)
		grid[2] /= 2;
	realcut();
	if(nvert<=0) {
		for(lat=klimits[0];lat<klimits[1]+dd-FUZZ;lat+=dd) {
			if(lat>klimits[1])
				lat = klimits[1];
			for(lon=klimits[2];lon<klimits[3]+dd-FUZZ;lon+=dd) {
				i = (kflag?posproj:normproj)
					(lat,lon+(lon<klimits[3]?FUZZ:-FUZZ),
				   &x,&y);
				if(i*vflag <= 0)
					continue;
				if(x<xmin) xmin = x;
				if(x>xmax) xmax = x;
				if(y<ymin) ymin = y;
				if(y>ymax) ymax = y;
			}
		}
	} else {
		for(i=0; i<nvert; i++) {
			x = v[i].x;
			y = v[i].y;
			if(x<xmin) xmin = x;
			if(x>xmax) xmax = x;
			if(y<ymin) ymin = y;
			if(y>ymax) ymax = y;
		}
	}
	xrange = xmax - xmin;
	yrange = ymax - ymin;
	if(xrange<=0||yrange<=0)
		error("map seems to be empty");
	scaling = 2;	/*plotting area from -1 to 1*/
	if(position[2]!=0) {
		if(posproj(position[0]-.5,position[1],&xcent,&ycent)==0||
		   posproj(position[0]+.5,position[1],&x,&y)==0)
			error("unreasonable position");
		scaling /= (position[2]*hypot(x-xcent,y-ycent));
		if(posproj(position[0],position[1],&xcent,&ycent)==0)
			error("unreasonable position");
	} else {
		scaling /= (xrange>yrange?xrange:yrange);
		xcent = (xmin+xmax)/2;
		ycent = (ymin+ymax)/2;
	}
	xoff = center[0]/scaling;
	yoff = center[1]/scaling;
	crot.l = center[2]*RAD;
	sincos(&crot);
	scaling *= HALFWIDTH*0.9;
	if(symbolfile)
		getsyms(symbolfile);
	if(!s2flag) {
		openpl();
		erase();
	}
	range(left,bottom,right,top);
	comment("grid","");
	colorx(gridcolor);
	pen(DOTTED);
	if(grid[0]>0.)
		for(lat=ceil(lolat/grid[0])*grid[0];
		    lat<=hilat;lat+=grid[0])
			dogrid(lat,lat,lolon,hilon);
	if(grid[1]>0.)
		for(lon=ceil(lolon/grid[1])*grid[1];
		    lon<=hilon;lon+=grid[1])
			dogrid(lolat,hilat,lon,lon);
	comment("border","");
	colorx(bordcolor);
	pen(SOLID);
	if(bflag) {
		dolimb();
		dobounds(lolat,hilat,lolon,hilon,0);
		dobounds(window[0],window[1],window[2],window[3],1);
	}
	lolat = floor(limits[0]/10)*10;
	hilat = ceil(limits[1]/10)*10;
	lolon = floor(limits[2]/10)*10;
	hilon = ceil(limits[3]/10)*10;
	if(lolon>hilon)
		hilon += 360.;
	/*do tracks first so as not to lose the standard input*/
	for(i=0;i<ntrack;i++) {
		longlines = LONGLINES;
		satellite(&track[i]);
		longlines = shortlines;
	}
	for(i=0;i<nfile;i++) {
		comment("mapfile",file[i].name);
		colorx(file[i].color);
		pen(file[i].style);
		getdata(file[i].name);
	}
	move(right,bottom);
	if(!s1flag)
		closepl();
	return 0;
}

/* Out of perverseness (really to recover from a dubious,
   but documented, convention) the returns from projection
   functions (-1 unplottable, 0 wrong sheet, 1 good) are
   recoded into -1 wrong sheet, 0 unplottable, 1 good. */

int
fixproj(struct place *g, double *x, double *y)
{
	int i = (*projection)(g,x,y);
	return i<0? 0: i==0? -1: 1;
}

int
normproj(double lat, double lon, double *x, double *y)
{
	int i;
	struct place geog;
	latlon(lat,lon,&geog);
/*
	printp(&geog);
*/
	normalize(&geog);
	if(!inwindow(&geog))
		return(-1);
	i = fixproj(&geog,x,y);
	if(rflag)
		*x = -*x;
/*
	printp(&geog);
	fprintf(stderr,"%d %.3f %.3f\n",i,*x,*y);
*/
	return(i);
}

int
posproj(double lat, double lon, double *x, double *y)
{
	int i;
	struct place geog;
	latlon(lat,lon,&geog);
	normalize(&geog);
	i = fixproj(&geog,x,y);
	if(rflag)
		*x = -*x;
	return(i);
}

int
inwindow(struct place *geog)
{
	if(geog->nlat.l<rwindow[0]-FUZZ||
	   geog->nlat.l>rwindow[1]+FUZZ||
	   geog->wlon.l<rwindow[2]-FUZZ||
	   geog->wlon.l>rwindow[3]+FUZZ)
		return(0);
	else return(1);
}

int
inlimits(struct place *g)
{
	if(rlimits[0]-FUZZ>g->nlat.l||
	   rlimits[1]+FUZZ<g->nlat.l)
		return(0);
	switch(limcase) {
	case 0:
		if(rlimits[2]+TWOPI-FUZZ>g->wlon.l&&
		   rlimits[3]+FUZZ<g->wlon.l)
			return(0);
		break;
	case 1:
		if(rlimits[2]-FUZZ>g->wlon.l||
		   rlimits[3]+FUZZ<g->wlon.l)
			return(0);
		break;
	case 2:
		if(rlimits[2]>g->wlon.l&&
		   rlimits[3]-TWOPI+FUZZ<g->wlon.l)
			return(0);
		break;
	}
	return(1);
}


long patch[18][36];

void
getdata(char *mapfile)
{
	char *indexfile;
	int kx,ky,c;
	int k;
	long b;
	long *p;
	int ip, jp;
	int n;
	struct place g;
	int i, j;
	double lat, lon;
	int conn;
	FILE *ifile, *xfile;

	indexfile = mapindex(mapfile);
	xfile = fopen(indexfile,"r");
	if(xfile==NULL)
		filerror("can't find map index", indexfile);
	free(indexfile);
	for(i=0,p=patch[0];i<18*36;i++,p++)
		*p = 1;
	while(!feof(xfile) && fscanf(xfile,"%d%d%ld",&i,&j,&b)==3)
		patch[i+9][j+18] = b;
	fclose(xfile);
	ifile = fopen(mapfile,"r");
	if(ifile==NULL)
		filerror("can't find map data", mapfile);
	for(lat=lolat;lat<hilat;lat+=10.)
		for(lon=lolon;lon<hilon;lon+=10.) {
			if(!seeable(lat,lon))
				continue;
			i = pnorm(lat);
			j = pnorm(lon);
			if((b=patch[i+9][j+18])&1)
				continue;
			fseek(ifile,b,0);
			while((ip=getc(ifile))>=0&&(jp=getc(ifile))>=0){
				if(ip!=(i&0377)||jp!=(j&0377))
					break;
				n = getshort(ifile);
				conn = 0;
				if(n > 0) {	/* absolute coordinates */
					kx = ky = 0;	/* set */
					for(k=0;k<n;k++){
						kx = SCALERATIO*getshort(ifile);
						ky = SCALERATIO*getshort(ifile);
						if (((k%delta) != 0) && (k != (n-1)))
							continue;
						conv(kx,&g.nlat);
						conv(ky,&g.wlon);
						conn = plotpt(&g,conn);
					}
				} else {	/* differential, scaled by SCALERATI0 */
					n = -n;
					kx = SCALERATIO*getshort(ifile);
					ky = SCALERATIO*getshort(ifile);
					for(k=0; k<n; k++) {
						c = getc(ifile);
						if(c&0200) c|= ~0177;
						kx += c;
						c = getc(ifile);
						if(c&0200) c|= ~0177;
						ky += c;
						if(k%delta!=0&&k!=n-1)
							continue;
						conv(kx,&g.nlat);
						conv(ky,&g.wlon);
						conn = plotpt(&g,conn);
					}
				}
				if(k==1) {
					conv(kx,&g.nlat);
					conv(ky,&g.wlon);
					plotpt(&g,conn);
				}
			}
		}
	fclose(ifile);
}

int
seeable(double lat0, double lon0)
{
	double x, y;
	double lat, lon;
	for(lat=lat0;lat<=lat0+10;lat+=2*grid[2])
		for(lon=lon0;lon<=lon0+10;lon+=2*grid[2])
			if(normproj(lat,lon,&x,&y)*vflag>0)
				return(1);
	return(0);
}

void
satellite(struct file *t)
{
	char sym[50];
	char lbl[50];
	double scale;
	int conn;
	double lat,lon;
	struct place place;
	static FILE *ifile;

	if(ifile == nil)
		ifile = stdin;
	if(t->name[0]!='-'||t->name[1]!=0) {
		fclose(ifile);
		if((ifile=fopen(t->name,"r"))==NULL)
			filerror("can't find track", t->name);
	}
	comment("track",t->name);
	colorx(t->color);
	pen(t->style);
	for(;;) {
		conn = 0;
		while(!feof(ifile) && fscanf(ifile,"%lf%lf",&lat,&lon)==2){
			latlon(lat,lon,&place);
			if(fscanf(ifile,"%1s",lbl) == 1) {
				if(strchr("+-.0123456789",*lbl)==0)
					break;
				ungetc(*lbl,ifile);
			}
			conn = plotpt(&place,conn);
		}
		if(feof(ifile))
			return;
		fscanf(ifile,"%[^\n]",lbl+1);
		switch(*lbl) {
		case '"':
			if(plotpt(&place,conn))
				text(lbl+1);
			break;
		case ':':
		case '!':
			if(sscanf(lbl+1,"%s %lf",sym,&scale) <= 1)
				scale = 1;
			if(plotpt(&place,conn?conn:-1)) {
				int r = *lbl=='!'?0:rflag?-1:1;
				pen(SOLID);
				if(putsym(&place,sym,scale,r) == 0)
					text(lbl);
				pen(t->style);
			}
			break;
		default:
			if(plotpt(&place,conn))
				text(lbl);
			break;
		}
	}
}

int
pnorm(double x)
{
	int i;
	i = x/10.;
	i %= 36;
	if(i>=18) return(i-36);
	if(i<-18) return(i+36);
	return(i);
}

void
error(char *s)
{
	fprintf(stderr,"map: \r\n%s\n",s);
	exits("error");
}

void
filerror(char *s, char *f)
{
	fprintf(stderr,"\r\n%s %s\n",s,f);
	exits("error");
}

char *
mapindex(char *s)
{
	char *t = malloc(strlen(s)+3);
	strcpy(t,s);
	strcat(t,".x");
	return t;
}

#define NOPT 32767
static int ox = NOPT;
static int oy = NOPT;

int
cpoint(int xi, int yi, int conn)
{
	int dx = abs(ox-xi);
	int dy = abs(oy-yi);
	if(!xflag && (xi<left||xi>=right || yi<bottom||yi>=top)) {
		ox = oy = NOPT;
		return 0;
	}
	if(conn == -1)		/* isolated plotting symbol */
		{}
	else if(!conn)
		point(xi,yi);
	else {
		if(dx+dy>longlines) {
			ox = oy = NOPT;	/* don't leap across cuts */
			return 0;
		}
		if(dx || dy)
			vec(xi,yi);
	}
	ox = xi, oy = yi;
	return dx+dy<=2? 2: 1;	/* 2=very near; see dogrid */
}


struct place oldg;

int
plotpt(struct place *g, int conn)
{
	int kx,ky;
	int ret;
	double cutlon;
	if(!inlimits(g)) {
		return(0);
}
	normalize(g);
	if(!inwindow(g)) {
		return(0);
}
	switch((*cut)(g,&oldg,&cutlon)) {
	case 2:
		if(conn) {
			ret = duple(g,cutlon)|duple(g,cutlon);
			oldg = *g;
			return(ret);
		}
	case 0:
		conn = 0;
	default:	/* prevent diags about bad return value */
	case 1:
		oldg = *g;
		ret = doproj(g,&kx,&ky);
		if(ret==0 || !onlimb && ret*vflag<=0)
			return(0);
		ret = cpoint(kx,ky,conn);
		return ret;
	}
}

int
doproj(struct place *g, int *kx, int *ky)
{
	int i;
	double x,y,x1,y1;
/*fprintf(stderr,"dopr1 %f %f \n",g->nlat.l,g->wlon.l);*/
	i = fixproj(g,&x,&y);
	if(i == 0)
		return(0);
	if(rflag)
		x = -x;
/*fprintf(stderr,"dopr2 %f %f\n",x,y);*/
	if(!inpoly(x,y)) {
		return 0;
}
	x1 = x - xcent;
	y1 = y - ycent;
	x = (x1*crot.c - y1*crot.s + xoff)*scaling;
	y = (x1*crot.s + y1*crot.c + yoff)*scaling;
	*kx = x + (x>0?.5:-.5);
	*ky = y + (y>0?.5:-.5);
	return(i);
}

int
duple(struct place *g, double cutlon)
{
	int kx,ky;
	int okx,oky;
	struct place ig;
	revlon(g,cutlon);
	revlon(&oldg,cutlon);
	ig = *g;
	invert(&ig);
	if(!inlimits(&ig))
		return(0);
	if(doproj(g,&kx,&ky)*vflag<=0 ||
	   doproj(&oldg,&okx,&oky)*vflag<=0)
		return(0);
	cpoint(okx,oky,0);
	cpoint(kx,ky,1);
	return(1);
}

void
revlon(struct place *g, double cutlon)
{
	g->wlon.l = reduce(cutlon-reduce(g->wlon.l-cutlon));
	sincos(&g->wlon);
}


/*	recognize problems of cuts
 *	move a point across cut to side of its predecessor
 *	if its very close to the cut
 *	return(0) if cut interrupts the line
 *	return(1) if line is to be drawn normally
 *	return(2) if line is so close to cut as to
 *	be properly drawn on both sheets
*/

int
picut(struct place *g, struct place *og, double *cutlon)
{
	*cutlon = PI;
	return(ckcut(g,og,PI));
}

int
nocut(struct place *g, struct place *og, double *cutlon)
{
	USED(g);
	USED(og);
	USED(cutlon);
/*
#pragma	ref g
#pragma	ref og
#pragma	ref cutlon
*/
	return(1);
}

int
ckcut(struct place *g1, struct place *g2, double lon)
{
	double d1, d2;
	double f1, f2;
	int kx,ky;
	d1 = reduce(g1->wlon.l -lon);
	d2 = reduce(g2->wlon.l -lon);
	if((f1=fabs(d1))<FUZZ)
		d1 = diddle(g1,lon,d2);
	if((f2=fabs(d2))<FUZZ) {
		d2 = diddle(g2,lon,d1);
		if(doproj(g2,&kx,&ky)*vflag>0)
			cpoint(kx,ky,0);
	}
	if(f1<FUZZ&&f2<FUZZ)
		return(2);
	if(f1>PI*TWO_THRD||f2>PI*TWO_THRD)
		return(1);
	return(d1*d2>=0);
}

double
diddle(struct place *g, double lon, double d)
{
	double d1;
	d1 = FUZZ/2;
	if(d<0)
		d1 = -d1;
	g->wlon.l = reduce(lon+d1);
	sincos(&g->wlon);
	return(d1);
}

double
reduce(double lon)
{
	if(lon>PI)
		lon -= 2*PI;
	else if(lon<-PI)
		lon += 2*PI;
	return(lon);
}


double tetrapt = 35.26438968;	/* atan(1/sqrt(2)) */

void
dogrid(double lat0, double lat1, double lon0, double lon1)
{
	double slat,slon,tlat,tlon;
	register int conn, oconn;
	slat = tlat = slon = tlon = 0;
	if(lat1>lat0)
		slat = tlat = fmin(grid[2],dlat);
	else
		slon = tlon = fmin(grid[2],dlon);;
	conn = oconn = 0;
	while(lat0<=lat1&&lon0<=lon1) {
		conn = gridpt(lat0,lon0,conn);
		if(projection==Xguyou&&slat>0) {
			if(lat0<-45&&lat0+slat>-45)
				conn = gridpt(-45.,lon0,conn);
			else if(lat0<45&&lat0+slat>45)
				conn = gridpt(45.,lon0,conn);
		} else if(projection==Xtetra&&slat>0) {
			if(lat0<-tetrapt&&lat0+slat>-tetrapt) {
				gridpt(-tetrapt-.001,lon0,conn);
				conn = gridpt(-tetrapt+.001,lon0,0);
			}
			else if(lat0<tetrapt&&lat0+slat>tetrapt) {
				gridpt(tetrapt-.001,lon0,conn);
				conn = gridpt(tetrapt+.001,lon0,0);
			}
		}
		if(conn==0 && oconn!=0) {
			if(slat+slon>.05) {
				lat0 -= slat;	/* steps too big */
				lon0 -= slon;	/* or near bdry */
				slat /= 2;
				slon /= 2;
				conn = oconn = gridpt(lat0,lon0,conn);
			} else
				oconn = 0;
		} else {
			if(conn==2) {
				slat = tlat;
				slon = tlon;
				conn = 1;
			}
			oconn = conn;
	 	}
		lat0 += slat;
		lon0 += slon;
	}
	gridpt(lat1,lon1,conn);
}

static int gridinv;		/* nonzero when doing window bounds */

int
gridpt(double lat, double lon, int conn)
{
	struct place g;
/*fprintf(stderr,"%f %f\n",lat,lon);*/
	latlon(lat,lon,&g);
	if(gridinv)
		invert(&g);
	return(plotpt(&g,conn));
}

/* win=0 ordinary grid lines, win=1 window lines */

void
dobounds(double lolat, double hilat, double lolon, double hilon, int win)
{
	gridinv = win;
	if(lolat>-90 || win && (poles&1)!=0)
		dogrid(lolat+FUZZ,lolat+FUZZ,lolon,hilon);
	if(hilat<90 || win && (poles&2)!=0)
		dogrid(hilat-FUZZ,hilat-FUZZ,lolon,hilon);
	if(hilon-lolon<360 || win && cut==picut) {
		dogrid(lolat,hilat,lolon+FUZZ,lolon+FUZZ);
		dogrid(lolat,hilat,hilon-FUZZ,hilon-FUZZ);
	}
	gridinv = 0;
}

static void
dolimb(void)
{
	double lat, lon;
	double res = fmin(dlat, dlon)/4;
	int conn = 0;
	int newconn;
	if(limb == 0)
		return;
	onlimb = gridinv = 1;
	for(;;) {
		newconn = (*limb)(&lat, &lon, res);
		if(newconn == -1)
			break;
		conn = gridpt(lat, lon, conn*newconn);
	}
	onlimb = gridinv = 0;
}


void
radbds(double *w, double *rw)
{
	int i;
	for(i=0;i<4;i++)
		rw[i] = w[i]*RAD;
	rw[0] -= FUZZ;
	rw[1] += FUZZ;
	rw[2] -= FUZZ;
	rw[3] += FUZZ;
}

void
windlim(void)
{
	double center = orientation[0];
	double colat;
	if(center>90)
		center = 180 - center;
	if(center<-90)
		center = -180 - center;
	if(fabs(center)>90)
		error("unreasonable orientation");
	colat = 90 - window[0];
	if(center-colat>limits[0])
		limits[0] = center - colat;
	if(center+colat<limits[1])
		limits[1] = center + colat;
}


short
getshort(FILE *f)
{
	int c, r;
	c = getc(f);
	r = (c | getc(f)<<8);
	if (r&0x8000)
		r |= ~0xFFFF;	/* in case short > 16 bits */
	return r;
}

double
fmin(double x, double y)
{
	return(x<y?x:y);
}

double
fmax(double x, double y)
{
	return(x>y?x:y);
}

void
clamp(double *px, double v)
{
	*px = (v<0?fmax:fmin)(*px,v);
}

void
pathnames(void)
{
	int i;
	char *t, *indexfile, *name;
	FILE *f, *fx;
	for(i=0; i<nfile; i++) {
		name = file[i].name;
		if(*name=='/')
			continue;
		indexfile = mapindex(name);
			/* ansi equiv of unix access() call */
		f = fopen(name, "r");
		fx = fopen(indexfile, "r");
		if(f) fclose(f);
		if(fx) fclose(fx);
		free(indexfile);
		if(f && fx)
			continue;
		t = malloc(strlen(name)+strlen(mapdir)+2);
		strcpy(t,mapdir);
		strcat(t,"/");
		strcat(t,name);
		file[i].name = t;
	}
}

void
clipinit(void)
{
	int i;
	double s,t;
	if(nvert<=0)
		return;
	for(i=0; i<nvert; i++) {	/*convert latlon to xy*/
		if(normproj(v[i].x,v[i].y,&v[i].x,&v[i].y)==0)
			error("invisible clipping vertex");
	}
	if(nvert==2) {			/*rectangle with diag specified*/
		nvert = 4;
		v[2] = v[1];
		v[1].x=v[0].x, v[1].y=v[2].y, v[3].x=v[2].x, v[3].y=v[0].y;
	}
	v[nvert] = v[0];
	v[nvert+1] = v[1];
	s = 0;
	for(i=1; i<=nvert; i++) {	/*test for convexity*/
		t = (v[i-1].x-v[i].x)*(v[i+1].y-v[i].y) -
		    (v[i-1].y-v[i].y)*(v[i+1].x-v[i].x);
		if(t<-FUZZ && s>=0) s = 1;
		if(t>FUZZ && s<=0) s = -1;
		if(-FUZZ<=t&&t<=FUZZ || t*s>0) {
			s = 0;
			break;
		}
	}
	if(s==0)
		error("improper clipping polygon");
	for(i=0; i<nvert; i++) {	/*edge equation ax+by=c*/
		e[i].a = s*(v[i+1].y - v[i].y);
		e[i].b = s*(v[i].x - v[i+1].x);
		e[i].c = s*(v[i].x*v[i+1].y - v[i].y*v[i+1].x);
	}
}

int
inpoly(double x, double y)
{
	int i;
	for(i=0; i<nvert; i++) {
		register struct edge *ei = &e[i];
		double val = x*ei->a + y*ei->b - ei->c;
		if(val>10*FUZZ)
			return(0);
	}
	return 1;
}

void
realcut(void)
{
	struct place g;
	double lat;

	if(cut != picut)	/* punt on unusual cuts */
		return;
	for(lat=window[0]; lat<=window[1]; lat+=grid[2]) {
		g.wlon.l = PI;
		sincos(&g.wlon);
		g.nlat.l = lat*RAD;
		sincos(&g.nlat);
		if(!inwindow(&g)) {
			break;
}
		invert(&g);
		if(inlimits(&g)) {
			return;
}
	}
	longlines = shortlines = LONGLINES;
	cut = nocut;		/* not necessary; small eff. gain */
}
