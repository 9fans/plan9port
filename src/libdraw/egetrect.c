#include <u.h>
#include <libc.h>
#include <draw.h>
#include <cursor.h>
#include <event.h>

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
egetrect(int but, Mouse *m)
{
	Rectangle r, rc;

	but = 1<<(but-1);
	esetcursor(&sweep);
	while(m->buttons)
		*m = emouse();
	while(!(m->buttons & but)){
		*m = emouse();
		if(m->buttons & (7^but))
			goto Return;
	}
	r.min = m->xy;
	r.max = m->xy;
	do{
		rc = canonrect(r);
		edrawgetrect(rc, 1);
		*m = emouse();
		edrawgetrect(rc, 0);
		r.max = m->xy;
	}while(m->buttons == but);

    Return:
	esetcursor(0);
	if(m->buttons & (7^but)){
		rc.min.x = rc.max.x = 0;
		rc.min.y = rc.max.y = 0;
		while(m->buttons)
			*m = emouse();
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

void
edrawgetrect(Rectangle rc, int up)
{
	int i;
	Rectangle r, rects[4];

	if(up && tmp[0]!=nil)
		if(Dx(tmp[0]->r)<Dx(rc) || Dy(tmp[2]->r)<Dy(rc))
			freetmp();

	if(tmp[0] == 0){
		r = Rect(0, 0, Dx(screen->r), W);
		tmp[0] = allocimage(display, r, screen->chan, 0, -1);
		tmp[1] = allocimage(display, r, screen->chan, 0, -1);
		r = Rect(0, 0, W, Dy(screen->r));
		tmp[2] = allocimage(display, r, screen->chan, 0, -1);
		tmp[3] = allocimage(display, r, screen->chan, 0, -1);
		red = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DRed);
		if(tmp[0]==0 || tmp[1]==0 || tmp[2]==0 || tmp[3]==0 || red==0)
			drawerror(display, "getrect: allocimage failed");
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
