#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"

static Rune Lheader[] = {
	'N', 'e', 'w', ' ',
	'C', 'u', 't', ' ',
	'P', 'a', 's', 't', 'e', ' ',
	'S', 'n', 'a', 'r', 'f', ' ',
	'S', 'o', 'r', 't', ' ',
	'Z', 'e', 'r', 'o', 'x', ' ',
	'D', 'e', 'l', 'c', 'o', 'l', ' ',
	0
};

void
colinit(Column *c, Rectangle r)
{
	Rectangle r1;
	Text *t;

	draw(screen, r, display->white, nil, ZP);
	c->r = r;
	c->w = nil;
	c->nw = 0;
	t = &c->tag;
	t->w = nil;
	t->col = c;
	r1 = r;
	r1.max.y = r1.min.y + font->height;
	textinit(t, fileaddtext(nil, t), r1, &reffont, tagcols);
	t->what = Columntag;
	r1.min.y = r1.max.y;
	r1.max.y += Border;
	draw(screen, r1, display->black, nil, ZP);
	textinsert(t, 0, Lheader, 38, TRUE);
	textsetselect(t, t->file->b.nc, t->file->b.nc);
	draw(screen, t->scrollr, colbutton, nil, colbutton->r.min);
	c->safe = TRUE;
}

Window*
coladd(Column *c, Window *w, Window *clone, int y)
{
	Rectangle r, r1;
	Window *v;
	int i, j, minht, ymax, buggered;

	v = nil;
	r = c->r;
	r.min.y = c->tag.fr.r.max.y+Border;
	if(y<r.min.y && c->nw>0){	/* steal half of last window by default */
		v = c->w[c->nw-1];
		y = v->body.fr.r.min.y+Dy(v->body.fr.r)/2;
	}
	/* look for window we'll land on */
	for(i=0; i<c->nw; i++){
		v = c->w[i];
		if(y < v->r.max.y)
			break;
	}
	buggered = 0;
	if(c->nw > 0){
		if(i < c->nw)
			i++;	/* new window will go after v */
		/*
		 * if landing window (v) is too small, grow it first.
		 */
		minht = v->tag.fr.font->height+Border+1;
		j = 0;
		while(!c->safe || v->body.fr.maxlines<=3 || Dy(v->body.all) <= minht){
			if(++j > 10){
				buggered = 1;	/* too many windows in column */
				break;
			}
			colgrow(c, v, 1);
		}

		/*
		 * figure out where to split v to make room for w
		 */

		/* new window stops where next window begins */
		if(i < c->nw)
			ymax = c->w[i]->r.min.y-Border;
		else
			ymax = c->r.max.y;

		/* new window must start after v's tag ends */
		y = max(y, v->tagtop.max.y+Border);

		/* new window must start early enough to end before ymax */
		y = min(y, ymax - minht);

		/* if y is too small, too many windows in column */
		if(y < v->tagtop.max.y+Border)
			buggered = 1;

		/*
		 * resize & redraw v
		 */
		r = v->r;
		r.max.y = ymax;
		draw(screen, r, textcols[BACK], nil, ZP);
		r1 = r;
		y = min(y, ymax-(v->tag.fr.font->height*v->taglines+v->body.fr.font->height+Border+1));
		r1.max.y = min(y, v->body.fr.r.min.y+v->body.fr.nlines*v->body.fr.font->height);
		r1.min.y = winresize(v, r1, FALSE, FALSE);
		r1.max.y = r1.min.y+Border;
		draw(screen, r1, display->black, nil, ZP);

		/*
		 * leave r with w's coordinates
		 */
		r.min.y = r1.max.y;
	}
	if(w == nil){
		w = emalloc(sizeof(Window));
		w->col = c;
		draw(screen, r, textcols[BACK], nil, ZP);
		wininit(w, clone, r);
	}else{
		w->col = c;
		winresize(w, r, FALSE, TRUE);
	}
	w->tag.col = c;
	w->tag.row = c->row;
	w->body.col = c;
	w->body.row = c->row;
	c->w = realloc(c->w, (c->nw+1)*sizeof(Window*));
	memmove(c->w+i+1, c->w+i, (c->nw-i)*sizeof(Window*));
	c->nw++;
	c->w[i] = w;
	c->safe = TRUE;

	/* if there were too many windows, redraw the whole column */
	if(buggered)
		colresize(c, c->r);

	savemouse(w);
	/* near the button, but in the body */
	moveto(mousectl, addpt(w->tag.scrollr.max, Pt(3, 3)));
	barttext = &w->body;
	return w;
}

void
colclose(Column *c, Window *w, int dofree)
{
	Rectangle r;
	int i, didmouse, up;

	/* w is locked */
	if(!c->safe)
		colgrow(c, w, 1);
	for(i=0; i<c->nw; i++)
		if(c->w[i] == w)
			goto Found;
	error("can't find window");
  Found:
	r = w->r;
	w->tag.col = nil;
	w->body.col = nil;
	w->col = nil;
	didmouse = restoremouse(w);
	if(dofree){
		windelete(w);
		winclose(w);
	}
	c->nw--;
	memmove(c->w+i, c->w+i+1, (c->nw-i)*sizeof(Window*));
	c->w = realloc(c->w, c->nw*sizeof(Window*));
	if(c->nw == 0){
		draw(screen, r, display->white, nil, ZP);
		return;
	}
	up = 0;
	if(i == c->nw){		/* extend last window down */
		w = c->w[i-1];
		r.min.y = w->r.min.y;
		r.max.y = c->r.max.y;
	}else{			/* extend next window up */
		up = 1;
		w = c->w[i];
		r.max.y = w->r.max.y;
	}
	draw(screen, r, textcols[BACK], nil, ZP);
	if(c->safe) {
		if(!didmouse && up)
			w->showdel = TRUE;
		winresize(w, r, FALSE, TRUE);
		if(!didmouse && up)
			movetodel(w);
	}
}

void
colcloseall(Column *c)
{
	int i;
	Window *w;

	if(c == activecol)
		activecol = nil;
	textclose(&c->tag);
	for(i=0; i<c->nw; i++){
		w = c->w[i];
		winclose(w);
	}
	c->nw = 0;
	free(c->w);
	free(c);
	clearmouse();
}

void
colmousebut(Column *c)
{
	moveto(mousectl, divpt(addpt(c->tag.scrollr.min, c->tag.scrollr.max), 2));
}

void
colresize(Column *c, Rectangle r)
{
	int i, old, new;
	Rectangle r1, r2;
	Window *w;

	clearmouse();
	r1 = r;
	r1.max.y = r1.min.y + c->tag.fr.font->height;
	textresize(&c->tag, r1, TRUE);
	draw(screen, c->tag.scrollr, colbutton, nil, colbutton->r.min);
	r1.min.y = r1.max.y;
	r1.max.y += Border;
	draw(screen, r1, display->black, nil, ZP);
	r1.max.y = r.max.y;
	new = Dy(r) - c->nw*(Border + font->height);
	old = Dy(c->r) - c->nw*(Border + font->height);
	for(i=0; i<c->nw; i++){
		w = c->w[i];
		w->maxlines = 0;
		if(i == c->nw-1)
			r1.max.y = r.max.y;
		else{
			r1.max.y = r1.min.y;
			if(new > 0 && old > 0 && Dy(w->r) > Border+font->height){
				r1.max.y += (Dy(w->r)-Border-font->height)*new/old + Border + font->height;
			}
		}
		r1.max.y = max(r1.max.y, r1.min.y + Border+font->height);
		r2 = r1;
		r2.max.y = r2.min.y+Border;
		draw(screen, r2, display->black, nil, ZP);
		r1.min.y = r2.max.y;
		r1.min.y = winresize(w, r1, FALSE, i==c->nw-1);
	}
	c->r = r;
}

static
int
colcmp(const void *a, const void *b)
{
	Rune *r1, *r2;
	int i, nr1, nr2;

	r1 = (*(Window**)a)->body.file->name;
	nr1 = (*(Window**)a)->body.file->nname;
	r2 = (*(Window**)b)->body.file->name;
	nr2 = (*(Window**)b)->body.file->nname;
	for(i=0; i<nr1 && i<nr2; i++){
		if(*r1 != *r2)
			return *r1-*r2;
		r1++;
		r2++;
	}
	return nr1-nr2;
}

void
colsort(Column *c)
{
	int i, y;
	Rectangle r, r1, *rp;
	Window **wp, *w;

	if(c->nw == 0)
		return;
	clearmouse();
	rp = emalloc(c->nw*sizeof(Rectangle));
	wp = emalloc(c->nw*sizeof(Window*));
	memmove(wp, c->w, c->nw*sizeof(Window*));
	qsort(wp, c->nw, sizeof(Window*), colcmp);
	for(i=0; i<c->nw; i++)
		rp[i] = wp[i]->r;
	r = c->r;
	r.min.y = c->tag.fr.r.max.y;
	draw(screen, r, textcols[BACK], nil, ZP);
	y = r.min.y;
	for(i=0; i<c->nw; i++){
		w = wp[i];
		r.min.y = y;
		if(i == c->nw-1)
			r.max.y = c->r.max.y;
		else
			r.max.y = r.min.y+Dy(w->r)+Border;
		r1 = r;
		r1.max.y = r1.min.y+Border;
		draw(screen, r1, display->black, nil, ZP);
		r.min.y = r1.max.y;
		y = winresize(w, r, FALSE, i==c->nw-1);
	}
	free(rp);
	free(c->w);
	c->w = wp;
}

void
colgrow(Column *c, Window *w, int but)
{
	Rectangle r, cr;
	int i, j, k, l, y1, y2, *nl, *ny, tot, nnl, onl, dnl, h;
	Window *v;

	for(i=0; i<c->nw; i++)
		if(c->w[i] == w)
			goto Found;
	error("can't find window");

  Found:
	cr = c->r;
	if(but < 0){	/* make sure window fills its own space properly */
		r = w->r;
		if(i==c->nw-1 || c->safe==FALSE)
			r.max.y = cr.max.y;
		else
			r.max.y = c->w[i+1]->r.min.y - Border;
		winresize(w, r, FALSE, TRUE);
		return;
	}
	cr.min.y = c->w[0]->r.min.y;
	if(but == 3){	/* full size */
		if(i != 0){
			v = c->w[0];
			c->w[0] = w;
			c->w[i] = v;
		}
		draw(screen, cr, textcols[BACK], nil, ZP);
		winresize(w, cr, FALSE, TRUE);
		for(i=1; i<c->nw; i++)
			c->w[i]->body.fr.maxlines = 0;
		c->safe = FALSE;
		return;
	}
	/* store old #lines for each window */
	onl = w->body.fr.maxlines;
	nl = emalloc(c->nw * sizeof(int));
	ny = emalloc(c->nw * sizeof(int));
	tot = 0;
	for(j=0; j<c->nw; j++){
		l = c->w[j]->taglines-1 + c->w[j]->body.fr.maxlines;
		nl[j] = l;
		tot += l;
	}
	/* approximate new #lines for this window */
	if(but == 2){	/* as big as can be */
		memset(nl, 0, c->nw * sizeof(int));
		goto Pack;
	}
	nnl = min(onl + max(min(5, w->taglines-1+w->maxlines), onl/2), tot);
	if(nnl < w->taglines-1+w->maxlines)
		nnl = (w->taglines-1+w->maxlines + nnl)/2;
	if(nnl == 0)
		nnl = 2;
	dnl = nnl - onl;
	/* compute new #lines for each window */
	for(k=1; k<c->nw; k++){
		/* prune from later window */
		j = i+k;
		if(j<c->nw && nl[j]){
			l = min(dnl, max(1, nl[j]/2));
			nl[j] -= l;
			nl[i] += l;
			dnl -= l;
		}
		/* prune from earlier window */
		j = i-k;
		if(j>=0 && nl[j]){
			l = min(dnl, max(1, nl[j]/2));
			nl[j] -= l;
			nl[i] += l;
			dnl -= l;
		}
	}
    Pack:
	/* pack everyone above */
	y1 = cr.min.y;
	for(j=0; j<i; j++){
		v = c->w[j];
		r = v->r;
		r.min.y = y1;
		r.max.y = y1+Dy(v->tagtop);
		if(nl[j])
			r.max.y += 1 + nl[j]*v->body.fr.font->height;
		r.min.y = winresize(v, r, c->safe, FALSE);
		r.max.y += Border;
		draw(screen, r, display->black, nil, ZP);
		y1 = r.max.y;
	}
	/* scan to see new size of everyone below */
	y2 = c->r.max.y;
	for(j=c->nw-1; j>i; j--){
		v = c->w[j];
		r = v->r;
		r.min.y = y2-Dy(v->tagtop);
		if(nl[j])
			r.min.y -= 1 + nl[j]*v->body.fr.font->height;
		r.min.y -= Border;
		ny[j] = r.min.y;
		y2 = r.min.y;
	}
	/* compute new size of window */
	r = w->r;
	r.min.y = y1;
	r.max.y = y2;
	h = w->body.fr.font->height;
	if(Dy(r) < Dy(w->tagtop)+1+h+Border)
		r.max.y = r.min.y + Dy(w->tagtop)+1+h+Border;
	/* draw window */
	r.max.y = winresize(w, r, c->safe, TRUE);
	if(i < c->nw-1){
		r.min.y = r.max.y;
		r.max.y += Border;
		draw(screen, r, display->black, nil, ZP);
		for(j=i+1; j<c->nw; j++)
			ny[j] -= (y2-r.max.y);
	}
	/* pack everyone below */
	y1 = r.max.y;
	for(j=i+1; j<c->nw; j++){
		v = c->w[j];
		r = v->r;
		r.min.y = y1;
		r.max.y = y1+Dy(v->tagtop);
		if(nl[j])
			r.max.y += 1 + nl[j]*v->body.fr.font->height;
		y1 = winresize(v, r, c->safe, j==c->nw-1);
		if(j < c->nw-1){	/* no border on last window */
			r.min.y = y1;
			r.max.y += Border;
			draw(screen, r, display->black, nil, ZP);
			y1 = r.max.y;
		}
	}
	free(nl);
	free(ny);
	c->safe = TRUE;
	winmousebut(w);
}

void
coldragwin(Column *c, Window *w, int but)
{
	Rectangle r;
	int i, b;
	Point p, op;
	Window *v;
	Column *nc;

	clearmouse();
	setcursor2(mousectl, &boxcursor, &boxcursor2);
	b = mouse->buttons;
	op = mouse->xy;
	while(mouse->buttons == b)
		readmouse(mousectl);
	setcursor(mousectl, nil);
	if(mouse->buttons){
		while(mouse->buttons)
			readmouse(mousectl);
		return;
	}

	for(i=0; i<c->nw; i++)
		if(c->w[i] == w)
			goto Found;
	error("can't find window");

  Found:
	if(w->tagexpand)	/* force recomputation of window tag size */
		w->taglines = 1;
	p = mouse->xy;
	if(abs(p.x-op.x)<5 && abs(p.y-op.y)<5){
		colgrow(c, w, but);
		winmousebut(w);
		return;
	}
	/* is it a flick to the right? */
	if(abs(p.y-op.y)<10 && p.x>op.x+30 && rowwhichcol(c->row, p)==c)
		p.x = op.x+Dx(w->r);	/* yes: toss to next column */
	nc = rowwhichcol(c->row, p);
	if(nc!=nil && nc!=c){
		colclose(c, w, FALSE);
		coladd(nc, w, nil, p.y);
		winmousebut(w);
		return;
	}
	if(i==0 && c->nw==1)
		return;			/* can't do it */
	if((i>0 && p.y<c->w[i-1]->r.min.y) || (i<c->nw-1 && p.y>w->r.max.y)
	|| (i==0 && p.y>w->r.max.y)){
		/* shuffle */
		colclose(c, w, FALSE);
		coladd(c, w, nil, p.y);
		winmousebut(w);
		return;
	}
	if(i == 0)
		return;
	v = c->w[i-1];
	if(p.y < v->tagtop.max.y)
		p.y = v->tagtop.max.y;
	if(p.y > w->r.max.y-Dy(w->tagtop)-Border)
		p.y = w->r.max.y-Dy(w->tagtop)-Border;
	r = v->r;
	r.max.y = p.y;
	if(r.max.y > v->body.fr.r.min.y){
		r.max.y -= (r.max.y-v->body.fr.r.min.y)%v->body.fr.font->height;
		if(v->body.fr.r.min.y == v->body.fr.r.max.y)
			r.max.y++;
	}
	r.min.y = winresize(v, r, c->safe, FALSE);
	r.max.y = r.min.y+Border;
	draw(screen, r, display->black, nil, ZP);
	r.min.y = r.max.y;
	if(i == c->nw-1)
		r.max.y = c->r.max.y;
	else
		r.max.y = c->w[i+1]->r.min.y-Border;
	winresize(w, r, c->safe, TRUE);
	c->safe = TRUE;
	winmousebut(w);
}

Text*
colwhich(Column *c, Point p)
{
	int i;
	Window *w;

	if(!ptinrect(p, c->r))
		return nil;
	if(ptinrect(p, c->tag.all))
		return &c->tag;
	for(i=0; i<c->nw; i++){
		w = c->w[i];
		if(ptinrect(p, w->r)){
			if(ptinrect(p, w->tagtop) || ptinrect(p, w->tag.all))
				return &w->tag;
			/* exclude partial line at bottom */
			if(p.x >= w->body.scrollr.max.x && p.y >= w->body.fr.r.max.y)
				return nil;
			return &w->body;
		}
	}
	return nil;
}

int
colclean(Column *c)
{
	int i, clean;

	clean = TRUE;
	for(i=0; i<c->nw; i++)
		clean &= winclean(c->w[i], TRUE);
	return clean;
}
