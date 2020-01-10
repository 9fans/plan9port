#include "sam.h"

void
moveto(File *f, Range r)
{
	Posn p1 = r.p1, p2 = r.p2;

	f->dot.r.p1 = p1;
	f->dot.r.p2 = p2;
	if(f->rasp){
		telldot(f);
		outTsl(Hmoveto, f->tag, f->dot.r.p1);
	}
}

void
telldot(File *f)
{
	if(f->rasp == 0)
		panic("telldot");
	if(f->dot.r.p1==f->tdot.p1 && f->dot.r.p2==f->tdot.p2)
		return;
	outTsll(Hsetdot, f->tag, f->dot.r.p1, f->dot.r.p2);
	f->tdot = f->dot.r;
}

void
tellpat(void)
{
	outTS(Hsetpat, &lastpat);
	patset = FALSE;
}

#define	CHARSHIFT	128

void
lookorigin(File *f, Posn p0, Posn ls)
{
	int nl, nc, c;
	Posn p, oldp0;

	if(p0 > f->b.nc)
		p0 = f->b.nc;
	oldp0 = p0;
	p = p0;
	for(nl=nc=c=0; c!=-1 && nl<ls && nc<ls*CHARSHIFT; nc++)
		if((c=filereadc(f, --p)) == '\n'){
			nl++;
			oldp0 = p0-nc;
		}
	if(c == -1)
		p0 = 0;
	else if(nl==0){
		if(p0>=CHARSHIFT/2)
			p0-=CHARSHIFT/2;
		else
			p0 = 0;
	}else
		p0 = oldp0;
	outTsl(Horigin, f->tag, p0);
}

int
alnum(int c)
{
	/*
	 * Hard to get absolutely right.  Use what we know about ASCII
	 * and assume anything above the Latin control characters is
	 * potentially an alphanumeric.
	 */
	if(c<=' ')
		return 0;
	if(0x7F<=c && c<=0xA0)
		return 0;
	if(utfrune("!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", c))
		return 0;
	return 1;
}

int
clickmatch(File *f, int cl, int cr, int dir, Posn *p)
{
	int c;
	int nest = 1;

	for(;;){
		if(dir > 0){
			if(*p >= f->b.nc)
				break;
			c = filereadc(f, (*p)++);
		}else{
			if(*p == 0)
				break;
			c = filereadc(f, --(*p));
		}
		if(c == cr){
			if(--nest==0)
				return 1;
		}else if(c == cl)
			nest++;
	}
	return cl=='\n' && nest==1;
}

Rune*
strrune(Rune *s, Rune c)
{
	Rune c1;

	if(c == 0) {
		while(*s++)
			;
		return s-1;
	}

	while(c1 = *s++)
		if(c1 == c)
			return s-1;
	return 0;
}

void
doubleclick(File *f, Posn p1)
{
	int c, i;
	Rune *r, *l;
	Posn p;

	if(p1 > f->b.nc)
		return;
	f->dot.r.p1 = f->dot.r.p2 = p1;
	for(i=0; left[i]; i++){
		l = left[i];
		r = right[i];
		/* try left match */
		p = p1;
		if(p1 == 0)
			c = '\n';
		else
			c = filereadc(f, p - 1);
		if(strrune(l, c)){
			if(clickmatch(f, c, r[strrune(l, c)-l], 1, &p)){
				f->dot.r.p1 = p1;
				f->dot.r.p2 = p-(c!='\n');
			}
			return;
		}
		/* try right match */
		p = p1;
		if(p1 == f->b.nc)
			c = '\n';
		else
			c = filereadc(f, p);
		if(strrune(r, c)){
			if(clickmatch(f, c, l[strrune(r, c)-r], -1, &p)){
				f->dot.r.p1 = p;
				if(c!='\n' || p!=0 || filereadc(f, 0)=='\n')
					f->dot.r.p1++;
				f->dot.r.p2 = p1+(p1<f->b.nc && c=='\n');
			}
			return;
		}
	}
	/* try filling out word to right */
	p = p1;
	while(p < f->b.nc && alnum(filereadc(f, p++)))
		f->dot.r.p2++;
	/* try filling out word to left */
	p = p1;
	while(--p >= 0 && alnum(filereadc(f, p)))
		f->dot.r.p1--;
}
