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
#include	"ksc.h"

/*
	contributed by kuro@vodka.Eng.Sun.COM (Teruhiko Kurosaka)
*/

/*
	a state machine for interpreting shift-ksc.
*/

#define	SS2	0x8e
#define	SS3	0x8f
/*
 * Convert EUC in Koran locale to Unicode.
 * Only codeset 0 and 1 are used.
 */
void
ukscproc(int c, Rune **r, long input_loc)
{
	static enum { init, cs1last /*, cs2, cs3first, cs3last*/} state = init;
	static int korean646 = 1; /* fixed to 1 for now. */
	static int lastc;
	int n;
	long l;

	switch(state)
	{
	case init:
		if (c < 0){
			return;
		}else if (c < 128){
			if(korean646 && (c=='\\')){
				emit(0x20A9);
			} else {
				emit(c);
			}
/*		}else if (c==SS2){
			state = cs2;
		}else if (c==SS3){
			state = cs3first;
 */		}else{
			lastc = c;
			state = cs1last;
		}
		return;

	case cs1last: /* 2nd byte of codeset 1 (KSC 5601) */
		if(c < 0){
			if(squawk)
				EPR "%s: unexpected EOF in %s\n", argv0, file);
			c = 0x21 | (lastc&0x80);
		}
		n = ((lastc&0x7f)-33)*94 + (c&0x7f)-33;
 		if((n >= ksc5601max) || ((l = tabksc5601[n]) < 0)){
			nerrors++;
			if(squawk)
				EPR "%s: unknown ksc5601 %d (from 0x%x,0x%x) near byte %ld in %s\n", argv0, n, lastc, c, input_loc, file);
			if(!clean)
				emit(BADMAP);
		} else {
			emit(l);
		}
		state = init;
		return;
	default:
		if(squawk)
			EPR "%s: ukscproc: unknown state %d\n",
				argv0, init);
	}
}

void
uksc_in(int fd, long *notused, struct convert *out)
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
			ukscproc(ibuf[i], &r, nin++);
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
	ukscproc(-1, &r, nin);
	if(r > ob)
		OUT(out, ob, r-ob);
	OUT(out, ob, 0);
}

void
uksc_out(Rune *base, int n, long *notused)
{
	char *p;
	int i;
	Rune r;
	long l;
	static int first = 1;

	USED(notused);
	if(first){
		first = 0;
		for(i = 0; i < NRUNE; i++)
			tab[i] = -1;
		for(i = 0; i < ksc5601max; i++)
			if((l = tabksc5601[i]) != -1){
				if(l < 0)
					tab[-l] = i;
				else
					tab[l] = i;
			}
	}
	nrunes += n;
	p = obuf;
	for(i = 0; i < n; i++){
		r = base[i];
		if(r < 128)
			*p++ = r;
		else {
			if(tab[r] != -1){
				*p++ = 0x80 | (tab[r]/94 + 0x21);
				*p++ = 0x80 | (tab[r]%94 + 0x21);
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
