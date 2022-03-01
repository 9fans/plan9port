#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <auth.h>
#include <fcall.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>

typedef struct Graph		Graph;
typedef struct Machine	Machine;

enum
{
	Ncolor	= 6,
	Ysqueeze	= 2,	/* vertical squeezing of label text */
	Labspace	= 2,	/* room around label */
	Dot		= 2,	/* height of dot */
	Opwid	= 5,	/* strlen("add  ") or strlen("drop ") */
	Nlab		= 3,	/* max number of labels on y axis */
	Lablen	= 16,	/* max length of label */
	Lx		= 4,	/* label tick length */

	STACK	= 8192,
	XSTACK	= 32768
};

enum
{
	V80211,
	Vbattery,
	Vcontext,
	Vcpu,
	Vether,
	Vethererr,
	Vetherin,
	Vetherout,
	Vfault,
	Vfork,
	Vidle,
	Vintr,
	Vload,
	Vmem,
	Vswap,
	Vsys,
	Vsyscall,
	Vuser,
	Nvalue
};

char*
labels[Nvalue] =
{
	"802.11",
	"battery",
	"context",
	"cpu",
	"ether",
	"ethererr",
	"etherin",
	"etherout",
	"fault",
	"fork",
	"idle",
	"intr",
	"load",
	"mem",
	"swap",
	"sys",
	"syscall",
	"user"
};

struct Graph
{
	int		colindex;
	Rectangle	r;
	int		*data;
	int		ndata;
	char		*label;
	int		value;
	void		(*update)(Graph*, long, ulong);
	Machine	*mach;
	int		overflow;
	Image	*overtmp;
	ulong	vmax;
};

struct Machine
{
	char		*name;
	int		fd;
	int		pid;
	int		dead;
	int		absolute[Nvalue];
	ulong	last[Nvalue];
	ulong	val[Nvalue][2];
	ulong	load;
	ulong	nload;
};

char	*menu2str[Nvalue+1];
char xmenu2str[Nvalue+1][40];

Menu	menu2 = {menu2str, 0};
int		present[Nvalue];
Image	*cols[Ncolor][3];
Graph	*graph;
Machine	*mach;
Font		*mediumfont;
char		*fontname;
char		*mysysname;
char		argchars[] = "8bcCeEfiIlmnsw";
int		pids[1024];
int 		parity;	/* toggled to avoid patterns in textured background */
int		nmach;
int		ngraph;	/* totaly number is ngraph*nmach */
double	scale = 1.0;
int		logscale = 0;
int		ylabels = 0;
int		oldsystem = 0;
int 		sleeptime = 1000;
int		changedvmax;

Mousectl *mc;
Keyboardctl *kc;

void
killall(char *s)
{
	int i;

	for(i=0; i<nmach; i++)
		if(mach[i].pid)
			postnote(PNPROC, mach[i].pid, "kill");
	threadexitsall(s);
}

void*
emalloc(ulong sz)
{
	void *v;
	v = malloc(sz);
	if(v == nil) {
		fprint(2, "stats: out of memory allocating %ld: %r\n", sz);
		killall("mem");
	}
	memset(v, 0, sz);
	return v;
}

void*
erealloc(void *v, ulong sz)
{
	v = realloc(v, sz);
	if(v == nil) {
		fprint(2, "stats: out of memory reallocating %ld: %r\n", sz);
		killall("mem");
	}
	return v;
}

char*
estrdup(char *s)
{
	char *t;
	if((t = strdup(s)) == nil) {
		fprint(2, "stats: out of memory in strdup(%.10s): %r\n", s);
		killall("mem");
	}
	return t;
}

void
mkcol(int i, int c0, int c1, int c2)
{
	cols[i][0] = allocimagemix(display, c0, DWhite);
	cols[i][1] = allocimage(display, Rect(0,0,1,1), CMAP8, 1, c1);
	cols[i][2] = allocimage(display, Rect(0,0,1,1), CMAP8, 1, c2);
}

void
colinit(void)
{
	if(fontname)
		mediumfont = openfont(display, fontname);
	if(mediumfont == nil)
		mediumfont = openfont(display, "/lib/font/bit/pelm/latin1.8.font");
	if(mediumfont == nil)
		mediumfont = font;

	/* Peach */
	mkcol(0, 0xFFAAAAFF, 0xFFAAAAFF, 0xBB5D5DFF);
	/* Aqua */
	mkcol(1, DPalebluegreen, DPalegreygreen, DPurpleblue);
	/* Yellow */
	mkcol(2, DPaleyellow, DDarkyellow, DYellowgreen);
	/* Green */
	mkcol(3, DPalegreen, DMedgreen, DDarkgreen);
	/* Blue */
	mkcol(4, 0x00AAFFFF, 0x00AAFFFF, 0x0088CCFF);
	/* Grey */
	cols[5][0] = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0xEEEEEEFF);
	cols[5][1] = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0xCCCCCCFF);
	cols[5][2] = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0x888888FF);
}

void
label(Point p, int dy, char *text)
{
	char *s;
	Rune r[2];
	int w, maxw, maxy;

	p.x += Labspace;
	maxy = p.y+dy;
	maxw = 0;
	r[1] = '\0';
	for(s=text; *s; ){
		if(p.y+mediumfont->height-Ysqueeze > maxy)
			break;
		w = chartorune(r, s);
		s += w;
		w = runestringwidth(mediumfont, r);
		if(w > maxw)
			maxw = w;
		runestring(screen, p, display->black, ZP, mediumfont, r);
		p.y += mediumfont->height-Ysqueeze;
	}
}

Point
paritypt(int x)
{
	return Pt(x+parity, 0);
}

Point
datapoint(Graph *g, int x, ulong v, ulong vmax)
{
	Point p;
	double y;

	p.x = x;
	y = ((double)v)/(vmax*scale);
	if(logscale){
		/*
		 * Arrange scale to cover a factor of 1000.
		 * vmax corresponds to the 100 mark.
		 * 10*vmax is the top of the scale.
		 */
		if(y <= 0.)
			y = 0;
		else{
			y = log10(y);
			/* 1 now corresponds to the top; -2 to the bottom; rescale */
			y = (y+2.)/3.;
		}
	}
	p.y = g->r.max.y - Dy(g->r)*y - Dot;
	if(p.y < g->r.min.y)
		p.y = g->r.min.y;
	if(p.y > g->r.max.y-Dot)
		p.y = g->r.max.y-Dot;
	return p;
}

void
drawdatum(Graph *g, int x, ulong prev, ulong v, ulong vmax)
{
	int c;
	Point p, q;

	c = g->colindex;
	p = datapoint(g, x, v, vmax);
	q = datapoint(g, x, prev, vmax);
	if(p.y < q.y){
		draw(screen, Rect(p.x, g->r.min.y, p.x+1, p.y), cols[c][0], nil, paritypt(p.x));
		draw(screen, Rect(p.x, p.y, p.x+1, q.y+Dot), cols[c][2], nil, ZP);
		draw(screen, Rect(p.x, q.y+Dot, p.x+1, g->r.max.y), cols[c][1], nil, ZP);
	}else{
		draw(screen, Rect(p.x, g->r.min.y, p.x+1, q.y), cols[c][0], nil, paritypt(p.x));
		draw(screen, Rect(p.x, q.y, p.x+1, p.y+Dot), cols[c][2], nil, ZP);
		draw(screen, Rect(p.x, p.y+Dot, p.x+1, g->r.max.y), cols[c][1], nil, ZP);
	}

}

void
redraw(Graph *g, int vmax)
{
	int i, c;

	if(vmax != g->vmax){
		g->vmax = vmax;
		changedvmax = 1;
	}
	c = g->colindex;
	draw(screen, g->r, cols[c][0], nil, paritypt(g->r.min.x));
	for(i=1; i<Dx(g->r); i++)
		drawdatum(g, g->r.max.x-i, g->data[i-1], g->data[i], vmax);
	drawdatum(g, g->r.min.x, g->data[i], g->data[i], vmax);
	g->overflow = 0;
}

void
update1(Graph *g, long v, ulong vmax)
{
	char buf[32];
	int overflow;

	if(v < 0)
		v = 0;
	if(vmax != g->vmax){
		g->vmax = vmax;
		changedvmax = 1;
	}
	if(g->overflow && g->overtmp!=nil)
		draw(screen, g->overtmp->r, g->overtmp, nil, g->overtmp->r.min);
	draw(screen, g->r, screen, nil, Pt(g->r.min.x+1, g->r.min.y));
	drawdatum(g, g->r.max.x-1, g->data[0], v, vmax);
	memmove(g->data+1, g->data, (g->ndata-1)*sizeof(g->data[0]));
	g->data[0] = v;
	g->overflow = 0;
	if(logscale)
		overflow = (v>10*vmax*scale);
	else
		overflow = (v>vmax*scale);
	if(overflow && g->overtmp!=nil){
		g->overflow = 1;
		draw(g->overtmp, g->overtmp->r, screen, nil, g->overtmp->r.min);
		sprint(buf, "%ld", v);
		string(screen, g->overtmp->r.min, display->black, ZP, mediumfont, buf);
	}
}

void
usage(void)
{
	fprint(2, "usage: stats [-LY] [-F font] [-O] [-S scale] [-W winsize] [-%s] [machine...]\n", argchars);
	threadexitsall("usage");
}

void
addgraph(int n)
{
	Graph *g, *ograph;
	int i, j;
	static int nadd;

	if(n > Nvalue)
		abort();
	/* avoid two adjacent graphs of same color */
	if(ngraph>0 && graph[ngraph-1].colindex==nadd%Ncolor)
		nadd++;
	ograph = graph;
	graph = emalloc(nmach*(ngraph+1)*sizeof(Graph));
	for(i=0; i<nmach; i++)
		for(j=0; j<ngraph; j++)
			graph[i*(ngraph+1)+j] = ograph[i*ngraph+j];
	free(ograph);
	ngraph++;
	for(i=0; i<nmach; i++){
		g = &graph[i*ngraph+(ngraph-1)];
		memset(g, 0, sizeof(Graph));
		g->value = n;
		g->label = menu2str[n]+Opwid;
		g->update = update1;	/* no other update functions yet */
		g->mach = &mach[i];
		g->colindex = nadd%Ncolor;
	}
	present[n] = 1;
	nadd++;
}

void
dropgraph(int which)
{
	Graph *ograph;
	int i, j, n;

	if(which > nelem(menu2str))
		abort();
	/* convert n to index in graph table */
	n = -1;
	for(i=0; i<ngraph; i++)
		if(strcmp(menu2str[which]+Opwid, graph[i].label) == 0){
			n = i;
			break;
		}
	if(n < 0){
		fprint(2, "stats: internal error can't drop graph\n");
		killall("error");
	}
	ograph = graph;
	graph = emalloc(nmach*(ngraph-1)*sizeof(Graph));
	for(i=0; i<nmach; i++){
		for(j=0; j<n; j++)
			graph[i*(ngraph-1)+j] = ograph[i*ngraph+j];
		free(ograph[i*ngraph+j].data);
		freeimage(ograph[i*ngraph+j].overtmp);
		for(j++; j<ngraph; j++)
			graph[i*(ngraph-1)+j-1] = ograph[i*ngraph+j];
	}
	free(ograph);
	ngraph--;
	present[which] = 0;
}

int initmach(Machine*, char*);

int
addmachine(char *name)
{
	if(ngraph > 0){
		fprint(2, "stats: internal error: ngraph>0 in addmachine()\n");
		usage();
	}
	if(mach == nil)
		nmach = 0;	/* a little dance to get us started with local machine by default */
	mach = erealloc(mach, (nmach+1)*sizeof(Machine));
	memset(mach+nmach, 0, sizeof(Machine));
	if (initmach(mach+nmach, name)){
		nmach++;
		return 1;
	} else
		return 0;
}

void
newvalue(Machine *m, int i, ulong *v, ulong *vmax)
{
	ulong now;

	if(m->last[i] == 0)
		m->last[i] = m->val[i][0];

	if(i == Vload){
		/*
		 * Invert the ewma to obtain the 5s load statistics.
		 * Ewma is load' = (1884/2048)*load + (164/2048)*last5s, so we do
		 * last5s = (load' - (1884/2048)*load) / (164/2048).
		 */
		if(++m->nload%5 == 0){
			now = m->val[i][0];
			m->load = (now - (((vlong)m->last[i]*1884)/2048)) * 2048 / 164;
			m->last[i] = now;
		}
		*v = m->load;
		*vmax = m->val[i][1];
	}else if(m->absolute[i]){
		*v = m->val[i][0];
		*vmax = m->val[i][1];
	}else{
		now = m->val[i][0];
		*v = (vlong)((now - m->last[i])*sleeptime)/1000;
		m->last[i] = now;
		*vmax = m->val[i][1];
	}
	if(*vmax == 0)
		*vmax = 1;
}

void
labelstrs(Graph *g, char strs[Nlab][Lablen], int *np)
{
	int j;
	ulong vmax;

	vmax = g->vmax;
	if(logscale){
		for(j=1; j<=2; j++)
			sprint(strs[j-1], "%g", scale*pow(10., j)*(double)vmax/100.);
		*np = 2;
	}else{
		for(j=1; j<=3; j++)
			sprint(strs[j-1], "%g", scale*(double)j*(double)vmax/4.0);
		*np = 3;
	}
}

int
labelwidth(void)
{
	int i, j, n, w, maxw;
	char strs[Nlab][Lablen];

	maxw = 0;
	for(i=0; i<ngraph; i++){
		/* choose value for rightmost graph */
		labelstrs(&graph[ngraph*(nmach-1)+i], strs, &n);
		for(j=0; j<n; j++){
			w = stringwidth(mediumfont, strs[j]);
			if(w > maxw)
				maxw = w;
		}
	}
	return maxw;
}

void
resize(void)
{
	int i, j, k, n, startx, starty, x, y, dx, dy, ly, ondata, maxx, wid, nlab;
	Graph *g;
	Rectangle machr, r;
	ulong v, vmax;
	char buf[128], labs[Nlab][Lablen];

	draw(screen, screen->r, display->white, nil, ZP);

	/* label left edge */
	x = screen->r.min.x;
	y = screen->r.min.y + Labspace+mediumfont->height+Labspace;
	dy = (screen->r.max.y - y)/ngraph;
	dx = Labspace+stringwidth(mediumfont, "0")+Labspace;
	startx = x+dx+1;
	starty = y;
	for(i=0; i<ngraph; i++,y+=dy){
		draw(screen, Rect(x, y-1, screen->r.max.x, y), display->black, nil, ZP);
		draw(screen, Rect(x, y, x+dx, screen->r.max.y), cols[graph[i].colindex][0], nil, paritypt(x));
		label(Pt(x, y), dy, graph[i].label);
		draw(screen, Rect(x+dx, y, x+dx+1, screen->r.max.y), cols[graph[i].colindex][2], nil, ZP);
	}

	/* label top edge */
	dx = (screen->r.max.x - startx)/nmach;
	for(x=startx, i=0; i<nmach; i++,x+=dx){
		draw(screen, Rect(x-1, starty-1, x, screen->r.max.y), display->black, nil, ZP);
		j = dx/stringwidth(mediumfont, "0");
	/*	n = mach[i].nproc; */
		n = 1;
		if(n>1 && j>=1+3+(n>10)+(n>100)){	/* first char of name + (n) */
			j -= 3+(n>10)+(n>100);
			if(j <= 0)
				j = 1;
			snprint(buf, sizeof buf, "%.*s(%d)", j, mach[i].name, n);
		}else
			snprint(buf, sizeof buf, "%.*s", j, mach[i].name);
		string(screen, Pt(x+Labspace, screen->r.min.y + Labspace), display->black, ZP, mediumfont, buf);
	}

	maxx = screen->r.max.x;

	/* label right, if requested */
	if(ylabels && dy>Nlab*(mediumfont->height+1)){
		wid = labelwidth();
		if(wid < (maxx-startx)-30){
			/* else there's not enough room */
			maxx -= 1+Lx+wid;
			draw(screen, Rect(maxx, starty, maxx+1, screen->r.max.y), display->black, nil, ZP);
			y = starty;
			for(j=0; j<ngraph; j++, y+=dy){
				/* choose value for rightmost graph */
				g = &graph[ngraph*(nmach-1)+j];
				labelstrs(g, labs, &nlab);
				r = Rect(maxx+1, y, screen->r.max.x, y+dy-1);
				if(j == ngraph-1)
					r.max.y = screen->r.max.y;
				draw(screen, r, cols[g->colindex][0], nil, paritypt(r.min.x));
				for(k=0; k<nlab; k++){
					ly = y + (dy*(nlab-k)/(nlab+1));
					draw(screen, Rect(maxx+1, ly, maxx+1+Lx, ly+1), display->black, nil, ZP);
					ly -= mediumfont->height/2;
					string(screen, Pt(maxx+1+Lx, ly), display->black, ZP, mediumfont, labs[k]);
				}
			}
		}
	}

	/* create graphs */
	for(i=0; i<nmach; i++){
		machr = Rect(startx+i*dx, starty, maxx, screen->r.max.y);
		if(i < nmach-1)
			machr.max.x = startx+(i+1)*dx - 1;
		y = starty;
		for(j=0; j<ngraph; j++, y+=dy){
			g = &graph[i*ngraph+j];
			/* allocate data */
			ondata = g->ndata;
			g->ndata = Dx(machr)+1;	/* may be too many if label will be drawn here; so what? */
			g->data = erealloc(g->data, g->ndata*sizeof(ulong));
			if(g->ndata > ondata)
				memset(g->data+ondata, 0, (g->ndata-ondata)*sizeof(ulong));
			/* set geometry */
			g->r = machr;
			g->r.min.y = y;
			g->r.max.y = y+dy - 1;
			if(j == ngraph-1)
				g->r.max.y = screen->r.max.y;
			draw(screen, g->r, cols[g->colindex][0], nil, paritypt(g->r.min.x));
			g->overflow = 0;
			r = g->r;
			r.max.y = r.min.y+mediumfont->height;
			r.max.x = r.min.x+stringwidth(mediumfont, "9999999");
			freeimage(g->overtmp);
			g->overtmp = nil;
			if(r.max.x <= g->r.max.x)
				g->overtmp = allocimage(display, r, screen->chan, 0, -1);
			newvalue(g->mach, g->value, &v, &vmax);
			redraw(g, vmax);
		}
	}

	flushimage(display, 1);
}

void
eresized(int new)
{
	lockdisplay(display);
	if(new && getwindow(display, Refnone) < 0) {
		fprint(2, "stats: can't reattach to window\n");
		killall("reattach");
	}
	resize();
	unlockdisplay(display);
}

void
mousethread(void *v)
{
	Mouse m;
	int i;

	USED(v);

	while(readmouse(mc) == 0){
		m = mc->m;
		if(m.buttons == 4){
			for(i=0; i<Nvalue; i++)
				if(present[i])
					memmove(menu2str[i], "drop ", Opwid);
				else
					memmove(menu2str[i], "add  ", Opwid);
			lockdisplay(display);
			i = menuhit(3, mc, &menu2, nil);
			if(i >= 0){
				if(!present[i])
					addgraph(i);
				else if(ngraph > 1)
					dropgraph(i);
				resize();
			}
			unlockdisplay(display);
		}
	}
}

void
resizethread(void *v)
{
	USED(v);

	while(recv(mc->resizec, 0) == 1){
		lockdisplay(display);
		if(getwindow(display, Refnone) < 0)
			sysfatal("attach to window: %r");
		resize();
		unlockdisplay(display);
	}
}

void
keyboardthread(void *v)
{
	Rune r;

	while(recv(kc->c, &r) == 1)
		if(r == 0x7F || r == 'q')
			killall("quit");
}

void machproc(void*);
void updateproc(void*);

int
threadmaybackground(void)
{
	return 1;
}

void
threadmain(int argc, char *argv[])
{
	int i, j;
	char *s;
	ulong nargs;
	char args[100];

	nmach = 1;
	mysysname = sysname();
	if(mysysname == nil){
		fprint(2, "stats: can't find sysname: %r\n");
		threadexitsall("sysname");
	}

	nargs = 0;
	ARGBEGIN{
	case 'T':
		s = ARGF();
		if(s == nil)
			usage();
		i = atoi(s);
		if(i > 0)
			sleeptime = 1000*i;
		break;
	case 'S':
		s = ARGF();
		if(s == nil)
			usage();
		scale = atof(s);
		if(scale <= 0.)
			usage();
		break;
	case 'L':
		logscale++;
		break;
	case 'F':
		fontname = EARGF(usage());
		break;
	case 'Y':
		ylabels++;
		break;
	case 'O':
		oldsystem = 1;
		break;
	case 'W':
		winsize = EARGF(usage());
		break;
	default:
		if(nargs>=sizeof args || strchr(argchars, ARGC())==nil)
			usage();
		args[nargs++] = ARGC();
	}ARGEND

	for(i=0; i<Nvalue; i++){
		menu2str[i] = xmenu2str[i];
		snprint(xmenu2str[i], sizeof xmenu2str[i], "add  %s", labels[i]);
	}

	if(argc == 0){
		mach = emalloc(nmach*sizeof(Machine));
		initmach(&mach[0], mysysname);
	}else{
		for(i=j=0; i<argc; i++)
			addmachine(argv[i]);
	}

	for(i=0; i<nmach; i++)
		proccreate(machproc, &mach[i], STACK);

	for(i=0; i<nargs; i++)
	switch(args[i]){
	default:
		fprint(2, "stats: internal error: unknown arg %c\n", args[i]);
		usage();
	case '8':
		addgraph(V80211);
		break;
	case 'b':
		addgraph(Vbattery);
		break;
	case 'c':
		addgraph(Vcontext);
		break;
	case 'C':
		addgraph(Vcpu);
		break;
	case 'e':
		addgraph(Vether);
		break;
	case 'E':
		addgraph(Vetherin);
		addgraph(Vetherout);
		break;
	case 'f':
		addgraph(Vfault);
		break;
	case 'i':
		addgraph(Vintr);
		break;
	case 'I':
		addgraph(Vload);
		addgraph(Vidle);
		break;
	case 'l':
		addgraph(Vload);
		break;
	case 'm':
		addgraph(Vmem);
		break;
	case 'n':
		addgraph(Vetherin);
		addgraph(Vetherout);
		addgraph(Vethererr);
		break;
	case 's':
		addgraph(Vsyscall);
		break;
	case 'w':
		addgraph(Vswap);
		break;
	}

	if(ngraph == 0)
		addgraph(Vload);

	for(i=0; i<nmach; i++)
		for(j=0; j<ngraph; j++)
			graph[i*ngraph+j].mach = &mach[i];

	if(initdraw(0, nil, "stats") < 0)
		sysfatal("initdraw: %r");
	colinit();
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");

	display->locking = 1;
	threadcreate(keyboardthread, nil, XSTACK);
	threadcreate(mousethread, nil, XSTACK);
	threadcreate(resizethread, nil, XSTACK);
	proccreate(updateproc, nil, XSTACK);
	resize();
	unlockdisplay(display);
}

void
updateproc(void *z)
{
	int i;
	ulong v, vmax;

	USED(z);
	for(;;){
		parity = 1-parity;
		lockdisplay(display);
		for(i=0; i<nmach*ngraph; i++){
			newvalue(graph[i].mach, graph[i].value, &v, &vmax);
			graph[i].update(&graph[i], v, vmax);
		}
		if(changedvmax){
			changedvmax = 0;
			resize();
		}
		flushimage(display, 1);
		unlockdisplay(display);
		sleep(sleeptime);
	}
}

void
machproc(void *v)
{
	char buf[256], *f[4], *p;
	int i, n, t;
	Machine *m;

	m = v;
	t = 0;
	for(;;){
		n = read(m->fd, buf+t, sizeof buf-t);
		m->dead = 0;
		if(n <= 0)
			break;
		t += n;
		while((p = memchr(buf, '\n', t)) != nil){
			*p++ = 0;
			n = tokenize(buf, f, nelem(f));
			if(n >= 3){
				for(i=0; i<Nvalue; i++){
					if(strcmp(labels[i], f[0]) == 0){
						if(*f[1] == '='){
							m->absolute[i] = 1;
							f[1]++;
						}
						m->val[i][0] = strtoul(f[1], 0, 0);
						m->val[i][1] = strtoul(f[2], 0, 0);
					}
				}
			}
			t -= (p-buf);
			memmove(buf, p, t);
		}
	}
	if(m->fd){
		close(m->fd);
		m->fd = -1;
	}
	if(m->pid){
		postnote(PNPROC, m->pid, "kill");
		m->pid = 0;
	}
}

int
initmach(Machine *m, char *name)
{
	char *args[5], *q;
	int p[2], kfd[3], pid;

	m->name = name;
	if(strcmp(name, mysysname) == 0)
		name = nil;

	if(pipe(p) < 0)
		sysfatal("pipe: %r");

	memset(args, 0, sizeof args);
	args[0] = "auxstats";
	if(name){
		args[1] = name;
		if((q = strchr(name, ':')) != nil){
			*q++ = 0;
			args[2] = q;
		}
	}
	kfd[0] = open("/dev/null", OREAD);
	kfd[1] = p[1];
	kfd[2] = dup(2, -1);
	if((pid = threadspawn(kfd, "auxstats", args)) < 0){
		fprint(2, "spawn: %r\n");
		close(kfd[0]);
		close(p[0]);
		close(p[1]);
		return 0;
	}
	m->fd = p[0];
	m->pid = pid;
	if((q = strchr(m->name, '.')) != nil)
		*q = 0;
	return 1;
}
