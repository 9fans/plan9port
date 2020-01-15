#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>

Image *hrhand, *minhand;
Image *dots, *back;

Point
circlept(Point c, int r, int degrees)
{
	double rad;
	rad = (double) degrees * PI/180.0;
	c.x += cos(rad)*r;
	c.y -= sin(rad)*r;
	return c;
}

void
redraw(Image *screen)
{
	static int tm, ntm;
	static Rectangle r;
	static Point c;
	static int rad;
	int i;
	int anghr, angmin;
	static Tm ntms;

	ntm = time(0);
	if(ntm == tm && eqrect(screen->r, r))
		return;

	ntms = *localtime(ntm);
	anghr = 90-(ntms.hour*5 + ntms.min/12)*6;
	angmin = 90-ntms.min*6;
	tm = ntm;
	r = screen->r;
	c = divpt(addpt(r.min, r.max), 2);
	rad = Dx(r) < Dy(r) ? Dx(r) : Dy(r);
	rad /= 2;
	rad -= 8;

	draw(screen, screen->r, back, nil, ZP);
	for(i=0; i<12; i++)
		fillellipse(screen, circlept(c, rad, i*(360/12)), 2, 2, dots, ZP);

	line(screen, c, circlept(c, (rad*3)/4, angmin), 0, 0, 1, minhand, ZP);
	line(screen, c, circlept(c, rad/2, anghr), 0, 0, 1, hrhand, ZP);

	flushimage(display, 1);
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		fprint(2,"can't reattach to window");
	redraw(screen);
}

void
main(int argc, char **argv)
{
	Event e;
	Mouse m;
	Menu menu;
	char *mstr[] = {"exit", 0};
	int key, timer;
	int t;

	USED(argc);
	USED(argv);

	if (initdraw(0, 0, "clock") < 0)
		sysfatal("initdraw failed");
	back = allocimagemix(display, DPalebluegreen, DWhite);

	hrhand = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DDarkblue);
	minhand = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DPaleblue);
	dots = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DBlue);
	redraw(screen);

	einit(Emouse);
	t = (30*1000);
	timer = etimer(0, t);

	menu.item = mstr;
	menu.lasthit = 0;
	for(;;) {
		key = event(&e);
		if(key == Emouse) {
			m = e.mouse;
			if(m.buttons & 4) {
				if(emenuhit(3, &m, &menu) == 0)
					exits(0);
			}
		} else if(key == timer) {
			redraw(screen);
		}
	}
}
