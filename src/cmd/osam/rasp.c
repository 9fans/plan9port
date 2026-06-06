#include "sam.h"
/*
 * GROWDATASIZE must be big enough that all errors go out as Hgrowdata's,
 * so they will be scrolled into visibility in the ~~sam~~ window (yuck!).
 */
#define	GROWDATASIZE	50	/* if size is <= this, send data with grow */

void	rcut(List*, Posn, Posn);
int	rterm(List*, Posn);
void	rgrow(List*, Posn, Posn);

static	Posn	growpos;
static	Posn	grown;
static	Posn	shrinkpos;
static	Posn	shrunk;

/*
 * rasp routines inform the terminal of changes to the file.
 *
 * a rasp is a list of spans within the file, and an indication
 * of whether the terminal knows about the span.
 *
 * optimize by coalescing multiple updates to the same span
 * if it is not known by the terminal.
 *
 * other possible optimizations: flush terminal's rasp by cut everything,
 * insert everything if rasp gets too large.
 */

/*
 * only called for initial load of file
 */
void
raspload(File *f)
{
	if(f->rasp == nil)
		return;
	grown = f->b.nc;
	growpos = 0;
	if(f->b.nc)
		rgrow(f->rasp, 0, f->b.nc);
	raspdone(f, 1);
}

void
raspstart(File *f)
{
	if(f->rasp == nil)
		return;
	grown = 0;
	shrunk = 0;
	outbuffered = 1;
}

void
raspdone(File *f, int toterm)
{
	if(f->dot.r.p1 > f->b.nc)
		f->dot.r.p1 = f->b.nc;
	if(f->dot.r.p2 > f->b.nc)
		f->dot.r.p2 = f->b.nc;
	if(f->mark.p1 > f->b.nc)
		f->mark.p1 = f->b.nc;
	if(f->mark.p2 > f->b.nc)
		f->mark.p2 = f->b.nc;
	if(f->rasp == nil)
		return;
	if(grown)
		outTsll(Hgrow, f->tag, growpos, grown);
	else if(shrunk)
		outTsll(Hcut, f->tag, shrinkpos, shrunk);
	if(toterm)
		outTs(Hcheck0, f->tag);
	outflush();
	outbuffered = 0;
	if(f == cmd){
		cmdpt += cmdptadv;
		cmdptadv = 0;
	}
}

void
raspflush(File *f)
{
	if(grown){
		outTsll(Hgrow, f->tag, growpos, grown);
		grown = 0;
	}
	else if(shrunk){
		outTsll(Hcut, f->tag, shrinkpos, shrunk);
		shrunk = 0;
	}
	outflush();
}

void
raspdelete(File *f, uint p1, uint p2, int toterm)
{
	long n;

	n = p2 - p1;
	if(n == 0)
		return;

	if(p2 <= f->dot.r.p1){
		f->dot.r.p1 -= n;
		f->dot.r.p2 -= n;
	}
	if(p2 <= f->mark.p1){
		f->mark.p1 -= n;
		f->mark.p2 -= n;
	}

	if(f->rasp == nil)
		return;

	if(f==cmd && p1<cmdpt){
		if(p2 <= cmdpt)
			cmdpt -= n;
		else
			cmdpt = p1;
	}
	if(toterm){
		if(grown){
			outTsll(Hgrow, f->tag, growpos, grown);
			grown = 0;
		}else if(shrunk && shrinkpos!=p1 && shrinkpos!=p2){
			outTsll(Hcut, f->tag, shrinkpos, shrunk);
			shrunk = 0;
		}
		if(!shrunk || shrinkpos==p2)
			shrinkpos = p1;
		shrunk += n;
	}
	rcut(f->rasp, p1, p2);
}

void
raspinsert(File *f, uint p1, Rune *buf, uint n, int toterm)
{
	Range r;

	if(n == 0)
		return;

	if(p1 < f->dot.r.p1){
		f->dot.r.p1 += n;
		f->dot.r.p2 += n;
	}
	if(p1 < f->mark.p1){
		f->mark.p1 += n;
		f->mark.p2 += n;
	}


	if(f->rasp == nil)
		return;
	if(f==cmd && p1<cmdpt)
		cmdpt += n;
	if(toterm){
		if(shrunk){
			outTsll(Hcut, f->tag, shrinkpos, shrunk);
			shrunk = 0;
		}
		if(n>GROWDATASIZE || !rterm(f->rasp, p1)){
			rgrow(f->rasp, p1, n);
			if(grown && growpos+grown!=p1 && growpos!=p1){
				outTsll(Hgrow, f->tag, growpos, grown);
				grown = 0;
			}
			if(!grown)
				growpos = p1;
			grown += n;
		}else{
			if(grown){
				outTsll(Hgrow, f->tag, growpos, grown);
				grown = 0;
			}
			rgrow(f->rasp, p1, n);
			r = rdata(f->rasp, p1, n);
			if(r.p1!=p1 || r.p2!=p1+n)
				panic("rdata in toterminal");
			outTsllS(Hgrowdata, f->tag, p1, n, tmprstr(buf, n));
		}
	}else{
		rgrow(f->rasp, p1, n);
		r = rdata(f->rasp, p1, n);
		if(r.p1!=p1 || r.p2!=p1+n)
			panic("rdata in toterminal");
	}
}

#define	M	0x80000000L
#define	P(i)	r->posnptr[i]
#define	T(i)	(P(i)&M)	/* in terminal */
#define	L(i)	(P(i)&~M)	/* length of this piece */

void
rcut(List *r, Posn p1, Posn p2)
{
	Posn p, x;
	int i;

	if(p1 == p2)
		panic("rcut 0");
	for(p=0,i=0; i<r->nused && p+L(i)<=p1; p+=L(i++))
		;
	if(i == r->nused)
		panic("rcut 1");
	if(p < p1){	/* chop this piece */
		if(p+L(i) < p2){
			x = p1-p;
			p += L(i);
		}else{
			x = L(i)-(p2-p1);
			p = p2;
		}
		if(T(i))
			P(i) = x|M;
		else
			P(i) = x;
		i++;
	}
	while(i<r->nused && p+L(i)<=p2){
		p += L(i);
		dellist(r, i);
	}
	if(p < p2){
		if(i == r->nused)
			panic("rcut 2");
		x = L(i)-(p2-p);
		if(T(i))
			P(i) = x|M;
		else
			P(i) = x;
	}
	/* can we merge i and i-1 ? */
	if(i>0 && i<r->nused && T(i-1)==T(i)){
		x = L(i-1)+L(i);
		dellist(r, i--);
		if(T(i))
			P(i)=x|M;
		else
			P(i)=x;
	}
}

void
rgrow(List *r, Posn p1, Posn n)
{
	Posn p;
	int i;

	if(n == 0)
		panic("rgrow 0");
	for(p=0,i=0; i<r->nused && p+L(i)<=p1; p+=L(i++))
		;
	if(i == r->nused){	/* stick on end of file */
		if(p!=p1)
			panic("rgrow 1");
		if(i>0 && !T(i-1))
			P(i-1)+=n;
		else
			inslist(r, i, n);
	}else if(!T(i))		/* goes in this empty piece */
		P(i)+=n;
	else if(p==p1 && i>0 && !T(i-1))	/* special case; simplifies life */
		P(i-1)+=n;
	else if(p==p1)
		inslist(r, i, n);
	else{			/* must break piece in terminal */
		inslist(r, i+1, (L(i)-(p1-p))|M);
		inslist(r, i+1, n);
		P(i) = (p1-p)|M;
	}
}

int
rterm(List *r, Posn p1)
{
	Posn p;
	int i;

	for(p = 0,i = 0; i<r->nused && p+L(i)<=p1; p+=L(i++))
		;
	if(i==r->nused)
		return i > 0 && T(i-1);
	return T(i);
}

Range
rdata(List *r, Posn p1, Posn n)
{
	Posn p;
	int i;
	Range rg;

	if(n==0)
		panic("rdata 0");
	for(p = 0,i = 0; i<r->nused && p+L(i)<=p1; p+=L(i++))
		;
	if(i==r->nused)
		panic("rdata 1");
	if(T(i)){
		n-=L(i)-(p1-p);
		if(n<=0){
			rg.p1 = rg.p2 = p1;
			return rg;
		}
		p+=L(i++);
		p1 = p;
	}
	if(T(i) || i==r->nused)
		panic("rdata 2");
	if(p+L(i)<p1+n)
		n = L(i)-(p1-p);
	rg.p1 = p1;
	rg.p2 = p1+n;
	if(p!=p1){
		inslist(r, i+1, L(i)-(p1-p));
		P(i)=p1-p;
		i++;
	}
	if(L(i)!=n){
		inslist(r, i+1, L(i)-n);
		P(i)=n;
	}
	P(i)|=M;
	/* now i is set; can we merge? */
	if(i<r->nused-1 && T(i+1)){
		P(i)=(n+=L(i+1))|M;
		dellist(r, i+1);
	}
	if(i>0 && T(i-1)){
		P(i)=(n+L(i-1))|M;
		dellist(r, i-1);
	}
	return rg;
}
