#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <bio.h>
#include <mouse.h>
#include <keyboard.h>

void keyboardthread(void *v);
void mousethread(void *v);
void resizethread(void *v);
void updateproc(void *v);

enum {
	STACK = 8196,
};

int nokill;
int textmode;
char *title;

Biobuf b;
Image *light;
Image *dark;
Image *text;
Keyboardctl *kc;
Mousectl *mc;

void
initcolor(void)
{
	text = display->black;
	light = allocimagemix(display, DPalegreen, DWhite);
	dark = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DDarkgreen);
}

Rectangle rbar;
vlong n, d;
int last;
int lastp = -1;

char backup[80];

void
drawbar(void)
{
	int i, j;
	int p;
	char buf[400], bar[200];
	static char lastbar[200];

	if(n > d || n < 0 || d <= 0)
		return;

	i = (Dx(rbar)*n)/d;
	p = (n*100LL)/d;
	if(textmode){
		if(Dx(rbar) > 150){
			rbar.min.x = 0;
			rbar.max.x = 150;
			return;
		}
		bar[0] = '|';
		for(j=0; j<i; j++)
			bar[j+1] = '#';
		for(; j<Dx(rbar); j++)
			bar[j+1] = '-';
		bar[j++] = '|';
		bar[j++] = ' ';
		sprint(bar+j, "%3d%% ", p);
		for(i=0; bar[i]==lastbar[i] && bar[i]; i++)
			;
		memset(buf, '\b', strlen(lastbar)-i);
		strcpy(buf+strlen(lastbar)-i, bar+i);
		if(buf[0])
			write(1, buf, strlen(buf));
		strcpy(lastbar, bar);
		return;
	}

	if(lastp == p && last == i)
		return;

	if(lastp != p){
		sprint(buf, "%d%%", p);
		stringbg(screen, addpt(screen->r.min, Pt(Dx(rbar)-30, 4)), text, ZP, display->defaultfont, buf, light, ZP);
		lastp = p;
	}

	if(last != i){
		draw(screen, Rect(rbar.min.x+last, rbar.min.y, rbar.min.x+i, rbar.max.y),
			dark, nil, ZP);
		last = i;
	}
	flushimage(display, 1);
}

void
resize()
{
	Point p, q;
	Rectangle r;

	r = screen->r;
	draw(screen, r, light, nil, ZP);
	p = string(screen, addpt(r.min, Pt(4,4)), text, ZP,
		display->defaultfont, title);

	p.x = r.min.x+4;
	p.y += display->defaultfont->height+4;

	q = subpt(r.max, Pt(4,4));
	rbar = Rpt(p, q);
	border(screen, rbar, -2, dark, ZP);
	last = 0;
	lastp = -1;

	flushimage(display, 1);
	drawbar();
}

void
keyboardthread(void *v)
{
	Rune r;

	while(recv(kc->c , &r) == 1){
		if ((r == 0x7F || r == 0x03 || r == 'q') && !nokill)
			threadexitsall("interrupt");
	}
}

void
mousethread(void *v)
{
	USED(v);

	while(recv(mc->c, 0) == 1); /* to unblock mc->c */
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
updateproc(void *v)
{
	char *p, *f[2];

	sleep(1000);
	while((p = Brdline(&b, '\n'))){
		p[Blinelen(&b)-1] = '\0';
		if(tokenize(p, f, 2) != 2)
			continue;
		n = strtoll(f[0], 0, 0);
		d = strtoll(f[1], 0, 0);
		if(!textmode){
			lockdisplay(display);
			drawbar();
			unlockdisplay(display);
		} else
			drawbar();
	}
	threadexitsall("success");
}

void
usage(void)
{
	fprint(2, "usage: statusbar [-kt] [-W winsize] 'title'\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	char *p;
	int lfd;

	p = "300x40@100,100";

	ARGBEGIN{
	case 'W':
		p = ARGF();
		break;
	case 't':
		textmode = 1;
		break;
	case 'k':
		nokill = 1;
		break;
	default:
		usage();
	}ARGEND;

	if(argc != 1)
		usage();

	winsize = p;

	title = argv[0];

	lfd = dup(0, -1);
	Binit(&b, lfd, OREAD);

	rbar = Rect(0, 0, 60, 1);
	if (!textmode){
		if(initdraw(0, nil, "bar") < 0)
			sysfatal("initdraw: %r");
		initcolor();
		if((mc = initmouse(nil, screen)) == nil)
			sysfatal("initmouse: %r");
		if((kc = initkeyboard(nil)) == nil)
			sysfatal("initkeyboard: %r");
		display->locking = 1;
		threadcreate(resizethread, nil, STACK);
		threadcreate(keyboardthread, nil, STACK);
		threadcreate(mousethread, nil, STACK);
		resize();
		unlockdisplay(display);
	}
	proccreate(updateproc, nil, STACK);
}
