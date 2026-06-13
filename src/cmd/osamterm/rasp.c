#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <frame.h>
#include "flayer.h"
#include "samterm.h"

void
rinit(Rasp *r)
{
	r->nrunes=0;
	r->sect=0;
}

void
rclear(Rasp *r)
{
	Section *s, *ns;

	for(s=r->sect; s; s=ns){
		ns = s->next;
		free(s->text);
		free(s);
	}
	r->sect = 0;
}

Section*
rsinsert(Rasp *r, Section *s)	/* insert before s */
{
	Section *t;
	Section *u;

	t = alloc(sizeof(Section));
	if(r->sect == s){	/* includes empty list case: r->sect==s==0 */
		r->sect = t;
		t->next = s;
	}else{
		u = r->sect;
		if(u == 0)
			panic("rsinsert 1");
		do{
			if(u->next == s){
				t->next = s;
				u->next = t;
				goto Return;
			}
			u=u->next;
		}while(u);
		panic("rsinsert 2");
	}
    Return:
	return t;
}

void
rsdelete(Rasp *r, Section *s)
{
	Section *t;

	if(s == 0)
		panic("rsdelete");
	if(r->sect == s){
		r->sect = s->next;
		goto Free;
	}
	for(t=r->sect; t; t=t->next)
		if(t->next == s){
			t->next = s->next;
	Free:
			if(s->text)
				free(s->text);
			free(s);
			return;
		}
	panic("rsdelete 2");
}

void
splitsect(Rasp *r, Section *s, long n0)
{
	if(s == 0)
		panic("splitsect");
	rsinsert(r, s->next);
	if(s->text == 0)
		s->next->text = 0;
	else{
		s->next->text = alloc(RUNESIZE*(TBLOCKSIZE+1));
		Strcpy(s->next->text, s->text+n0);
		s->text[n0] = 0;
	}
	s->next->nrunes = s->nrunes-n0;
	s->nrunes = n0;
}

Section *
findsect(Rasp *r, Section *s, long p, long q)	/* find sect containing q and put q on a sect boundary */
{
	if(s==0 && p!=q)
		panic("findsect");
	for(; s && p+s->nrunes<=q; s=s->next)
		p += s->nrunes;
	if(p != q){
		splitsect(r, s, q-p);
		s = s->next;
	}
	return s;
}

void
rresize(Rasp *r, long a, long old, long new)
{
	Section *s, *t, *ns;

	s = findsect(r, r->sect, 0L, a);
	t = findsect(r, s, a, a+old);
	for(; s!=t; s=ns){
		ns=s->next;
		rsdelete(r, s);
	}
	/* now insert the new piece before t */
	if(new > 0){
		ns=rsinsert(r, t);
		ns->nrunes=new;
		ns->text=0;
	}
	r->nrunes += new-old;
}

void
rdata(Rasp *r, long p0, long p1, Rune *cp)
{
	Section *s, *t, *ns;

	s = findsect(r, r->sect, 0L, p0);
	t = findsect(r, s, p0, p1);
	for(; s!=t; s=ns){
		ns=s->next;
		if(s->text)
			panic("rdata");
		rsdelete(r, s);
	}
	p1 -= p0;
	s = rsinsert(r, t);
	s->text = alloc(RUNESIZE*(TBLOCKSIZE+1));
	memmove(s->text, cp, RUNESIZE*p1);
	s->text[p1] = 0;
	s->nrunes = p1;
}

void
rclean(Rasp *r)
{
	Section *s;

	for(s=r->sect; s; s=s->next)
		while(s->next && (s->text!=0)==(s->next->text!=0)){
			if(s->text){
				if(s->nrunes+s->next->nrunes>TBLOCKSIZE)
					break;
				Strcpy(s->text+s->nrunes, s->next->text);
			}
			s->nrunes += s->next->nrunes;
			rsdelete(r, s->next);
		}
}

void
Strcpy(Rune *to, Rune *from)
{
	do; while(*to++ = *from++);
}

Rune*
rload(Rasp *r, ulong p0, ulong p1, ulong *nrp)
{
	Section *s;
	long p;
	int n, nb;

	nb = 0;
	Strgrow(&scratch, &nscralloc, p1-p0+1);
	scratch[0] = 0;
	for(p=0,s=r->sect; s && p+s->nrunes<=p0; s=s->next)
		p += s->nrunes;
	while(p<p1 && s){
		/*
		 * Subtle and important.  If we are preparing to handle an 'rdata'
		 * call, it's because we have an 'rresize' hole here, so the
		 * screen doesn't have data for that space anyway (it got cut
		 * first).  So pretend it isn't there.
		 */
		if(s->text){
			n = s->nrunes-(p0-p);
			if(n>p1-p0)	/* all in this section */
				n = p1-p0;
			memmove(scratch+nb, s->text+(p0-p), n*RUNESIZE);
			nb += n;
			scratch[nb] = 0;
		}
		p += s->nrunes;
		p0 = p;
		s = s->next;
	}
	if(nrp)
		*nrp = nb;
	return scratch;
}

int
rmissing(Rasp *r, ulong p0, ulong p1)
{
	Section *s;
	long p;
	int n, nm=0;

	for(p=0,s=r->sect; s && p+s->nrunes<=p0; s=s->next)
		p += s->nrunes;
	while(p<p1 && s){
		if(s->text == 0){
			n = s->nrunes-(p0-p);
			if(n > p1-p0)	/* all in this section */
				n = p1-p0;
			nm += n;
		}
		p += s->nrunes;
		p0 = p;
		s = s->next;
	}
	return nm;
}

int
rcontig(Rasp *r, ulong p0, ulong p1, int text)
{
	Section *s;
	long p, n;
	int np=0;

	for(p=0,s=r->sect; s && p+s->nrunes<=p0; s=s->next)
		p += s->nrunes;
	while(p<p1 && s && (text? (s->text!=0) : (s->text==0))){
		n = s->nrunes-(p0-p);
		if(n > p1-p0)	/* all in this section */
			n = p1-p0;
		np += n;
		p += s->nrunes;
		p0 = p;
		s = s->next;
	}
	return np;
}

void
Strgrow(Rune **s, long *n, int want)	/* can always toss the old data when called */
{
	if(*n >= want)
		return;
	free(*s);
	*s = alloc(RUNESIZE*want);
	*n = want;
}
