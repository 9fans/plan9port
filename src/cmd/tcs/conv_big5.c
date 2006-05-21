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
#include	"big5.h"

/*
	a state machine for interpreting big5 (hk format).
*/
void
big5proc(int c, Rune **r, long input_loc)
{
	static enum { state0, state1 } state = state0;
	static int lastc;
	long n, ch, f, cold = c;

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
		if(c == 26)
			c = '\n';
		emit(c);
		return;

	case state1:	/* seen a font spec */
		if(c >= 64 && c <= 126)
			c -= 64;
		else if(c >= 161 && c <= 254)
			c = c-161 + 63;
		else {
			nerrors++;
			if(squawk)
				EPR "%s: bad big5 glyph (from 0x%x,0x%lx) near byte %ld in %s\n",
					argv0, lastc, cold, input_loc, file);
			if(!clean)
				emit(BADMAP);
			state = state0;
			return;
		}
		if(lastc >= 161 && lastc <= 254)
			f = lastc - 161;
		else {
			nerrors++;
			if(squawk)
				EPR "%s: bad big5 font %d (from 0x%x,0x%lx) near byte %ld in %s\n",
					argv0, lastc-161, lastc, cold, input_loc, file);
			if(!clean)
				emit(BADMAP);
			state = state0;
			return;
		}
		n = f*BIG5FONT + c;
		if(n < BIG5MAX)
			ch = tabbig5[n];
		else
			ch = -1;
		if(ch < 0){
			nerrors++;
			if(squawk)
				EPR "%s: unknown big5 %ld (from 0x%x,0x%lx) near byte %ld in %s\n",
					argv0, n, lastc, cold, input_loc, file);
			if(!clean)
				emit(BADMAP);
		} else
			emit(ch);
		state = state0;
	}
}

void
big5_in(int fd, long *notused, struct convert *out)
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
			big5proc(ibuf[i], &r, nin++);
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
	big5proc(-1, &r, nin);
	if(r > ob)
		OUT(out, ob, r-ob);
	OUT(out, ob, 0);
}

void
big5_out(Rune *base, int n, long *notused)
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
		for(i = 0; i < BIG5MAX; i++)
			if(tabbig5[i] != -1)
				tab[tabbig5[i]] = i;
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
				if(r >= BIG5MAX){
					*p++ = (char)0xA1;
					*p++ = r-BIG5MAX;
					continue;
				} else {
					*p++ = 0xA1 + (r/BIG5FONT);
					r = r%BIG5FONT;
					if(r <= 62) r += 64;
					else r += 0xA1-63;
					*p++ = r;
					continue;
				}
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
