#ifdef	PLAN9
#include	<u.h>
#include	<libc.h>
#include	<bio.h>
#else
#include	<stdio.h>
#include	<unistd.h>
#include	"plan9.h"
#endif
#include	"hdr.h"
#include	"conv.h"
#include	"gb.h"

/*
	a state machine for interpreting gb.
*/
void
gbproc(int c, Rune **r, long input_loc)
{
	static enum { state0, state1 } state = state0;
	static int lastc;
	long n, ch, cold = c;

	switch(state)
	{
	case state0:	/* idle state */
		if(c < 0)
			return;
		if(c >= 0xA1){
			lastc = c;
			state = state1;
			return;
		}
		emit(c);
		return;

	case state1:	/* seen a font spec */
		if(c >= 0xA1)
			n = (lastc-0xA0)*100 + (c-0xA0);
		else {
			nerrors++;
			if(squawk)
				EPR "%s: bad gb glyph %d (from 0x%x,0x%lx) near byte %ld in %s\n", argv0, c-0xA0, lastc, cold, input_loc, file);
			if(!clean)
				emit(BADMAP);
			state = state0;
			return;
		}
		ch = tabgb[n];
		if(ch < 0){
			nerrors++;
			if(squawk)
				EPR "%s: unknown gb %ld (from 0x%x,0x%lx) near byte %ld in %s\n", argv0, n, lastc, cold, input_loc, file);
			if(!clean)
				emit(BADMAP);
		} else
			emit(ch);
		state = state0;
	}
}

void
gb_in(int fd, long *notused, struct convert *out)
{
	Rune ob[N];
	Rune *r, *re;
	uchar ibuf[N];
	int n, i;
	long nin;

	USED(notused);
	r = ob;
	re = ob+N-3;
	nin = 0;
	while((n = read(fd, ibuf, sizeof ibuf)) > 0){
		for(i = 0; i < n; i++){
			gbproc(ibuf[i], &r, nin++);
			if(r >= re){
				OUT(out, ob, r-ob);
				r = ob;
			}
		}
		if(r > ob){
			OUT(out, ob, r-ob);
			r = ob;
		}
	}
	gbproc(-1, &r, nin);
	if(r > ob)
		OUT(out, ob, r-ob);
	OUT(out, ob, 0);
}

void
gb_out(Rune *base, int n, long *notused)
{
	char *p;
	int i;
	Rune r;
	static int first = 1;

	USED(notused);
	if(first){
		first = 0;
		for(i = 0; i < NRUNE; i++)
			tab[i] = -1;
		for(i = 0; i < GBMAX; i++)
			if(tabgb[i] != -1)
				tab[tabgb[i]] = i;
	}
	nrunes += n;
	p = obuf;
	for(i = 0; i < n; i++){
		r = base[i];
		if(r < 128)
			*p++ = r;
		else {
			if(tab[r] != -1){
				r = tab[r];
				*p++ = 0xA0 + (r/100);
				*p++ = 0xA0 + (r%100);
				continue;
			}
			if(squawk)
				EPR "%s: rune 0x%x not in output cs\n", argv0, r);
			nerrors++;
			if(clean)
				continue;
			*p++ = BYTEBADMAP;
		}
	}
	noutput += p-obuf;
	if(p > obuf)
		write(1, obuf, p-obuf);
}
