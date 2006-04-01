#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>

enum
{
	Margin = 4,		/* outside to text */
	Border = 2,		/* outside to selection boxes */
	Blackborder = 2,	/* width of outlining border */
	Vspacing = 2,		/* extra spacing between lines of text */
	Maxunscroll = 25,	/* maximum #entries before scrolling turns on */
	Nscroll = 20,		/* number entries in scrolling part */
	Scrollwid = 14,		/* width of scroll bar */
	Gap = 4			/* between text and scroll bar */
};

static	Image	*menutxt;
static	Image	*back;
static	Image	*high;
static	Image	*bord;
static	Image	*text;
static	Image	*htext;

static
void
menucolors(void)
{
	/* Main tone is greenish, with negative selection */
	back = allocimagemix(display, DPalegreen, DWhite);
	high = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DDarkgreen);	/* dark green */
	bord = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DMedgreen);	/* not as dark green */
	if(back==nil || high==nil || bord==nil)
		goto Error;
	text = display->black;
	htext = back;
	return;

    Error:
	freeimage(back);
	freeimage(high);
	freeimage(bord);
	back = display->white;
	high = display->black;
	bord = display->black;
	text = display->black;
	htext = display->white;
}

/*
 * r is a rectangle holding the text elements.
 * return the rectangle, including its black edge, holding element i.
 */
static Rectangle
menurect(Rectangle r, int i)
{
	if(i < 0)
		return Rect(0, 0, 0, 0);
	r.min.y += (font->height+Vspacing)*i;
	r.max.y = r.min.y+font->height+Vspacing;
	return insetrect(r, Border-Margin);
}

/*
 * r is a rectangle holding the text elements.
 * return the element number containing p.
 */
static int
menusel(Rectangle r, Point p)
{
	r = insetrect(r, Margin);
	if(!ptinrect(p, r))
		return -1;
	return (p.y-r.min.y)/(font->height+Vspacing);
}

static
void
paintitem(Menu *menu, Rectangle textr, int off, int i, int highlight, Image *save, Image *restore)
{
	char *item;
	Rectangle r;
	Point pt;

	if(i < 0)
		return;
	r = menurect(textr, i);
	if(restore){
		draw(screen, r, restore, nil, restore->r.min);
		return;
	}
	if(save)
		draw(save, save->r, screen, nil, r.min);
	item = menu->item? menu->item[i+off] : (*menu->gen)(i+off);
	pt.x = (textr.min.x+textr.max.x-stringwidth(font, item))/2;
	pt.y = textr.min.y+i*(font->height+Vspacing);
	draw(screen, r, highlight? high : back, nil, pt);
	string(screen, pt, highlight? htext : text, pt, font, item);
}

/*
 * menur is a rectangle holding all the highlightable text elements.
 * track mouse while inside the box, return what's selected when button
 * is raised, -1 as soon as it leaves box.
 * invariant: nothing is highlighted on entry or exit.
 */
static int
menuscan(Menu *menu, int but, Mouse *m, Rectangle textr, int off, int lasti, Image *save)
{
	int i;

	paintitem(menu, textr, off, lasti, 1, save, nil);
	flushimage(display, 1);	/* in case display->locking is set */
	*m = emouse();
	while(m->buttons & (1<<(but-1))){
		flushimage(display, 1);	/* in case display->locking is set */
		*m = emouse();
		i = menusel(textr, m->xy);
		if(i != -1 && i == lasti)
			continue;
		paintitem(menu, textr, off, lasti, 0, nil, save);
		if(i == -1)
			return i;
		lasti = i;
		paintitem(menu, textr, off, lasti, 1, save, nil);
	}
	return lasti;
}

static void
menupaint(Menu *menu, Rectangle textr, int off, int nitemdrawn)
{
	int i;

	draw(screen, insetrect(textr, Border-Margin), back, nil, ZP);
	for(i = 0; i<nitemdrawn; i++)
		paintitem(menu, textr, off, i, 0, nil, nil);
}

static void
menuscrollpaint(Rectangle scrollr, int off, int nitem, int nitemdrawn)
{
	Rectangle r;

	draw(screen, scrollr, back, nil, ZP);
	r.min.x = scrollr.min.x;
	r.max.x = scrollr.max.x;
	r.min.y = scrollr.min.y + (Dy(scrollr)*off)/nitem;
	r.max.y = scrollr.min.y + (Dy(scrollr)*(off+nitemdrawn))/nitem;
	if(r.max.y < r.min.y+2)
		r.max.y = r.min.y+2;
	border(screen, r, 1, bord, ZP);
	if(menutxt == 0)
		menutxt = allocimage(display, Rect(0, 0, 1, 1), CMAP8, 1, DDarkgreen);
	if(menutxt)
		draw(screen, insetrect(r, 1), menutxt, nil, ZP);
}

int
emenuhit(int but, Mouse *m, Menu *menu)
{
	int i, nitem, nitemdrawn, maxwid, lasti, off, noff, wid, screenitem;
	int scrolling;
	Rectangle r, menur, sc, textr, scrollr;
	Image *b, *save;
	Point pt;
	char *item;

	if(back == nil)
		menucolors();
	sc = screen->clipr;
	replclipr(screen, 0, screen->r);
	maxwid = 0;
	for(nitem = 0;
	    item = menu->item? menu->item[nitem] : (*menu->gen)(nitem);
	    nitem++){
		i = stringwidth(font, item);
		if(i > maxwid)
			maxwid = i;
	}
	if(menu->lasthit<0 || menu->lasthit>=nitem)
		menu->lasthit = 0;
	screenitem = (Dy(screen->r)-10)/(font->height+Vspacing);
	if(nitem>Maxunscroll || nitem>screenitem){
		scrolling = 1;
		nitemdrawn = Nscroll;
		if(nitemdrawn > screenitem)
			nitemdrawn = screenitem;
		wid = maxwid + Gap + Scrollwid;
		off = menu->lasthit - nitemdrawn/2;
		if(off < 0)
			off = 0;
		if(off > nitem-nitemdrawn)
			off = nitem-nitemdrawn;
		lasti = menu->lasthit-off;
	}else{
		scrolling = 0;
		nitemdrawn = nitem;
		wid = maxwid;
		off = 0;
		lasti = menu->lasthit;
	}
	r = insetrect(Rect(0, 0, wid, nitemdrawn*(font->height+Vspacing)), -Margin);
	r = rectsubpt(r, Pt(wid/2, lasti*(font->height+Vspacing)+font->height/2));
	r = rectaddpt(r, m->xy);
	pt = ZP;
	if(r.max.x>screen->r.max.x)
		pt.x = screen->r.max.x-r.max.x;
	if(r.max.y>screen->r.max.y)
		pt.y = screen->r.max.y-r.max.y;
	if(r.min.x<screen->r.min.x)
		pt.x = screen->r.min.x-r.min.x;
	if(r.min.y<screen->r.min.y)
		pt.y = screen->r.min.y-r.min.y;
	menur = rectaddpt(r, pt);
	textr.max.x = menur.max.x-Margin;
	textr.min.x = textr.max.x-maxwid;
	textr.min.y = menur.min.y+Margin;
	textr.max.y = textr.min.y + nitemdrawn*(font->height+Vspacing);
	if(scrolling){
		scrollr = insetrect(menur, Border);
		scrollr.max.x = scrollr.min.x+Scrollwid;
	}else
		scrollr = Rect(0, 0, 0, 0);

	b = allocimage(display, menur, screen->chan, 0, 0);
	if(b == 0)
		b = screen;
	draw(b, menur, screen, nil, menur.min);
	draw(screen, menur, back, nil, ZP);
	border(screen, menur, Blackborder, bord, ZP);
	save = allocimage(display, menurect(textr, 0), screen->chan, 0, -1);
	r = menurect(textr, lasti);
	emoveto(divpt(addpt(r.min, r.max), 2));
	menupaint(menu, textr, off, nitemdrawn);
	if(scrolling)
		menuscrollpaint(scrollr, off, nitem, nitemdrawn);
	while(m->buttons & (1<<(but-1))){
		lasti = menuscan(menu, but, m, textr, off, lasti, save);
		if(lasti >= 0)
			break;
		while(!ptinrect(m->xy, textr) && (m->buttons & (1<<(but-1)))){
			if(scrolling && ptinrect(m->xy, scrollr)){
				noff = ((m->xy.y-scrollr.min.y)*nitem)/Dy(scrollr);
				noff -= nitemdrawn/2;
				if(noff < 0)
					noff = 0;
				if(noff > nitem-nitemdrawn)
					noff = nitem-nitemdrawn;
				if(noff != off){
					off = noff;
					menupaint(menu, textr, off, nitemdrawn);
					menuscrollpaint(scrollr, off, nitem, nitemdrawn);
				}
			}
			flushimage(display, 1);	/* in case display->locking is set */
			*m = emouse();
		}
	}
	draw(screen, menur, b, nil, menur.min);
	if(b != screen)
		freeimage(b);
	freeimage(save);
	replclipr(screen, 0, sc);
	flushimage(display, 1);
	if(lasti >= 0){
		menu->lasthit = lasti+off;
		return menu->lasthit;
	}
	return -1;
}
