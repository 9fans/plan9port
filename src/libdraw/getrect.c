#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>

#define	W	Borderwidth

static Image *tmp[4];
static Image *red;

static Cursor sweep={
	{-7, -7},
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0x07,
	 0xE0, 0x07, 0xE0, 0x07, 0xE3, 0xF7, 0xE3, 0xF7,
	 0xE3, 0xE7, 0xE3, 0xF7, 0xE3, 0xFF, 0xE3, 0x7F,
	 0xE0, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,},
	{0x00, 0x00, 0x7F, 0xFE, 0x40, 0x02, 0x40, 0x02,
	 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x41, 0xE2,
	 0x41, 0xC2, 0x41, 0xE2, 0x41, 0x72, 0x40, 0x38,
	 0x40, 0x1C, 0x40, 0x0E, 0x7F, 0xE6, 0x00, 0x00,}
};

static
void
brects(Rectangle r, Rectangle rp[4])
{
	if(Dx(r) < 2*W)
		r.max.x = r.min.x+2*W;
	if(Dy(r) < 2*W)
		r.max.y = r.min.y+2*W;
	rp[0] = Rect(r.min.x, r.min.y, r.max.x, r.min.y+W);
	rp[1] = Rect(r.min.x, r.max.y-W, r.max.x, r.max.y);
	rp[2] = Rect(r.min.x, r.min.y+W, r.min.x+W, r.max.y-W);
	rp[3] = Rect(r.max.x-W, r.min.y+W, r.max.x, r.max.y-W);
}

Rectangle
getrect(int but, Mousectl *mc)
{
	Rectangle r, rc;

	but = 1<<(but-1);
	setcursor(mc, &sweep);
	while(mc->m.buttons)
		readmouse(mc);
	while(!(mc->m.buttons & but)){
		readmouse(mc);
		if(mc->m.buttons & (7^but))
			goto Return;
	}
	r.min = mc->m.xy;
	r.max = mc->m.xy;
	do{
		rc = canonrect(r);
		drawgetrect(rc, 1);
		readmouse(mc);
		drawgetrect(rc, 0);
		r.max = mc->m.xy;
	}while(mc->m.buttons == but);

    Return:
	setcursor(mc, nil);
	if(mc->m.buttons & (7^but)){
		rc.min.x = rc.max.x = 0;
		rc.min.y = rc.max.y = 0;
		while(mc->m.buttons)
			readmouse(mc);
	}
	return rc;
}

static
void
freetmp(void)
{
	freeimage(tmp[0]);
	freeimage(tmp[1]);
	freeimage(tmp[2]);
	freeimage(tmp[3]);
	freeimage(red);
	tmp[0] = tmp[1] = tmp[2] = tmp[3] = red = nil;
}

static
int
max(int a, int b)
{
	if(a > b)
		return a;
	return b;
}

void
drawgetrect(Rectangle rc, int up)
{
	int i;
	Rectangle r, rects[4];

	/*
	 * BUG: if for some reason we have two of these going on at once
	 * when we must grow the tmp buffers, we lose data.  Also if tmp
	 * is unallocated and we ask to restore the screen, it would be nice
	 * to complain, but we silently make a mess.
	 */
	if(up && tmp[0]!=nil)
		if(Dx(tmp[0]->r)<Dx(rc) || Dy(tmp[2]->r)<Dy(rc))
			freetmp();
	if(tmp[0] == 0){
		r = Rect(0, 0, max(Dx(display->screenimage->r), Dx(rc)), W);
		tmp[0] = allocimage(display, r, screen->chan, 0, -1);
		tmp[1] = allocimage(display, r, screen->chan, 0, -1);
		r = Rect(0, 0, W, max(Dy(display->screenimage->r), Dy(rc)));
		tmp[2] = allocimage(display, r, screen->chan, 0, -1);
		tmp[3] = allocimage(display, r, screen->chan, 0, -1);
		red = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DRed);
		if(tmp[0]==0 || tmp[1]==0 || tmp[2]==0 || tmp[3]==0 || red==0){
			freetmp();
			drawerror(display, "getrect: allocimage failed");
		}
	}
	brects(rc, rects);
	if(!up){
		for(i=0; i<4; i++)
			draw(screen, rects[i], tmp[i], nil, ZP);
		return;
	}
	for(i=0; i<4; i++){
		draw(tmp[i], Rect(0, 0, Dx(rects[i]), Dy(rects[i])), screen, nil, rects[i].min);
		draw(screen, rects[i], red, nil, ZP);
	}
}
