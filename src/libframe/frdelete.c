#include <u.h>
#include <libc.h>
#include <draw.h>
#include <mouse.h>
#include <frame.h>

int
frdelete(Frame *f, ulong p0, ulong p1)
{
	Point pt0, pt1, ppt0;
	Frbox *b;
	int n0, n1, n, w0;
	ulong cn1;
	Rectangle r;
	int nn0;
	Image *col;

	if(p0>=f->nchars || p0==p1 || f->b==nil)
		return 0;
	if(p1 > f->nchars)
		p1 = f->nchars;
	n0 = _frfindbox(f, 0, 0, p0);
	if(n0 == f->nbox)
		drawerror(f->display, "off end in frdelete");
	n1 = _frfindbox(f, n0, p0, p1);
	pt0 = _frptofcharnb(f, p0, n0);
	pt1 = frptofchar(f, p1);
	if(f->p0 == f->p1)
		frtick(f, frptofchar(f, f->p0), 0);
	nn0 = n0;
	ppt0 = pt0;
	_frfreebox(f, n0, n1-1);
	f->modified = 1;

	/*
	 * Invariants:
	 *  - pt0 points to beginning, pt1 points to end
	 *  - n0 is box containing beginning of stuff being deleted
	 *  - n1, b are box containing beginning of stuff to be kept after deletion
	 *  - cn1 is char position of n1
	 *  - f->p0 and f->p1 are not adjusted until after all deletion is done
	 */
	b = &f->box[n1];
	cn1 = p1;
	while(pt1.x!=pt0.x && n1<f->nbox){
		_frcklinewrap0(f, &pt0, b);
		_frcklinewrap(f, &pt1, b);
		n = _frcanfit(f, pt0, b);
		if(n==0)
			drawerror(f->display, "_frcanfit==0");
		r.min = pt0;
		r.max = pt0;
		r.max.y += f->font->height;
		if(b->nrune > 0){
			w0 = b->wid;
			if(n != b->nrune){
				_frsplitbox(f, n1, n);
				b = &f->box[n1];
			}
			r.max.x += b->wid;
			draw(f->b, r, f->b, nil, pt1);
			cn1 += b->nrune;

			/* blank remainder of line */
			r.min.x = r.max.x;
			r.max.x += w0 - b->wid;
			if(r.max.x > f->r.max.x)
				r.max.x = f->r.max.x;
			draw(f->b, r, f->cols[BACK], nil, r.min);
		}else{
			r.max.x += _frnewwid0(f, pt0, b);
			if(r.max.x > f->r.max.x)
				r.max.x = f->r.max.x;
			col = f->cols[BACK];
			if(f->p0<=cn1 && cn1<f->p1)
				col = f->cols[HIGH];
			draw(f->b, r, col, nil, pt0);
			cn1++;
		}
		_fradvance(f, &pt1, b);
		pt0.x += _frnewwid(f, pt0, b);
		f->box[n0++] = f->box[n1++];
		b++;
	}
	if(n1==f->nbox && pt0.x!=pt1.x)	/* deleting last thing in window; must clean up */
		frselectpaint(f, pt0, pt1, f->cols[BACK]);
	if(pt1.y != pt0.y){
		Point pt2;

		pt2 = _frptofcharptb(f, 32767, pt1, n1);
		if(pt2.y > f->r.max.y)
			drawerror(f->display, "frptofchar in frdelete");
		if(n1 < f->nbox){
			int q0, q1, q2;

			q0 = pt0.y+f->font->height;
			q1 = pt1.y+f->font->height;
			q2 = pt2.y+f->font->height;
			if(q2 > f->r.max.y)
				q2 = f->r.max.y;
			draw(f->b, Rect(pt0.x, pt0.y, pt0.x+(f->r.max.x-pt1.x), q0),
				f->b, nil, pt1);
			draw(f->b, Rect(f->r.min.x, q0, f->r.max.x, q0+(q2-q1)),
				f->b, nil, Pt(f->r.min.x, q1));
			frselectpaint(f, Pt(pt2.x, pt2.y-(pt1.y-pt0.y)), pt2, f->cols[BACK]);
		}else
			frselectpaint(f, pt0, pt2, f->cols[BACK]);
	}
	_frclosebox(f, n0, n1-1);
	if(nn0>0 && f->box[nn0-1].nrune>=0 && ppt0.x-f->box[nn0-1].wid>=(int)f->r.min.x){
		--nn0;
		ppt0.x -= f->box[nn0].wid;
	}
	_frclean(f, ppt0, nn0, n0<f->nbox-1? n0+1 : n0);
	if(f->p1 > p1)
		f->p1 -= p1-p0;
	else if(f->p1 > p0)
		f->p1 = p0;
	if(f->p0 > p1)
		f->p0 -= p1-p0;
	else if(f->p0 > p0)
		f->p0 = p0;
	f->nchars -= p1-p0;
	if(f->p0 == f->p1)
		frtick(f, frptofchar(f, f->p0), 1);
	pt0 = frptofchar(f, f->nchars);
	n = f->nlines;
	f->nlines = (pt0.y-f->r.min.y)/f->font->height+(pt0.x>f->r.min.x);
	return n - f->nlines;
}
