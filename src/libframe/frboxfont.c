#include <u.h>
#include <libc.h>
#include <draw.h>
#include <mouse.h>
#include <frame.h>

static void
setboxfont(Frame *f, int nb, Font *font)
{
	Frbox *b;

	b = &f->box[nb];
	if(b->font == font)
		return;
	b->font = font;
	if(b->nrune >= 0)
		b->wid = stringnwidth(FRBOXFONT(f, b), (char*)b->ptr, b->nrune);
}

void
frsetboxfont(Frame *f, ulong p0, ulong p1, Font *font)
{
	int nb;
	ulong p;
	Frbox *b;

	if(p0 >= p1 || p1 > (ulong)f->nchars)
		return;
	_frfindbox(f, 0, 0, p1);
	nb = _frfindbox(f, 0, 0, p0);
	p = p0;
	while(p < p1 && nb < f->nbox){
		b = &f->box[nb];
		setboxfont(f, nb, font);
		p += (b->nrune < 0 ? 1 : b->nrune);
		nb++;
	}
	f->modified = 1;
}

void
frrelayout(Frame *f)
{
	Point pt, org;

	if(f->b == nil)
		return;
	org = Pt(f->r.min.x, f->r.min.y);
	pt = _frdraw(f, org);
	_frclean(f, org, 0, f->nbox);
	if(pt.y >= f->r.max.y)
		f->nlines = f->maxlines;
	else
		f->nlines = (pt.y-f->r.min.y)/f->lineheight + (pt.x>f->r.min.x);
	if(f->p0 > f->nchars)
		f->p0 = f->nchars;
	if(f->p1 > f->nchars)
		f->p1 = f->nchars;
}
