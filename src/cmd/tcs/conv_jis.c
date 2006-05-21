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
#include	"kuten208.h"
#include	"jis.h"

/*
	a state machine for interpreting all sorts of encodings
*/
static void
alljis(int c, Rune **r, long input_loc)
{
	static enum { state0, state1, state2, state3, state4 } state = state0;
	static int set8 = 0;
	static int japan646 = 0;
	static int lastc;
	int n;
	long l;

again:
	switch(state)
	{
	case state0:	/* idle state */
		if(c == ESC){ state = state1; return; }
		if(c < 0) return;
		if(!set8 && (c < 128)){
			if(japan646){
				switch(c)
				{
				case '\\':	emit(0xA5); return;	/* yen */
				case '~':	emit(0xAF); return;	/* spacing macron */
				default:	emit(c); return;
				}
			} else {
				emit(c);
				return;
			}
		}
		if(c < 0x21){	/* guard against bogus characters in JIS mode */
			if(squawk)
				EPR "%s: non-JIS character %02x in %s near byte %ld\n", argv0, c, file, input_loc);
			emit(c);
			return;
		}
		lastc = c; state = state4; return;

	case state1:	/* seen an escape */
		if(c == '$'){ state = state2; return; }
		if(c == '('){ state = state3; return; }
		emit(ESC); state = state0; goto again;

	case state2:	/* may be shifting into JIS */
		if((c == '@') || (c == 'B')){
			set8 = 1; state = state0; return;
		}
		emit(ESC); emit('$'); state = state0; goto again;

	case state3:	/* may be shifting out of JIS */
		if((c == 'J') || (c == 'H') || (c == 'B')){
			japan646 = (c == 'J');
			set8 = 0; state = state0; return;
		}
		emit(ESC); emit('('); state = state0; goto again;

	case state4:	/* two part char */
		if(c < 0){
			if(squawk)
				EPR "%s: unexpected EOF in %s\n", argv0, file);
			c = 0x21 | (lastc&0x80);
		}
		if(CANS2J(lastc, c)){	/* ms dos sjis */
			int hi = lastc, lo = c;
			S2J(hi, lo);			/* convert to 208 */
			n = hi*100 + lo - 3232;		/* convert to kuten208 */
		} else
			n = (lastc&0x7F)*100 + (c&0x7f) - 3232;	/* kuten208 */
		if((n >= KUTEN208MAX) || ((l = tabkuten208[n]) == -1)){
			nerrors++;
			if(squawk)
				EPR "%s: unknown kuten208 %d (from 0x%x,0x%x) near byte %ld in %s\n", argv0, n, lastc, c, input_loc, file);
			if(!clean)
				emit(BADMAP);
		} else {
			if(l < 0){
				l = -l;
				if(squawk)
					EPR "%s: ambiguous kuten208 %d (mapped to 0x%lx) near byte %ld in %s\n", argv0, n, l, input_loc, file);
			}
			emit(l);
		}
		state = state0;
	}
}

/*
	a state machine for interpreting ms-kanji == shift-jis.
*/
static void
ms(int c, Rune **r, long input_loc)
{
	static enum { state0, state1, state2, state3, state4 } state = state0;
	static int set8 = 0;
	static int japan646 = 0;
	static int lastc;
	int n;
	long l;

again:
	switch(state)
	{
	case state0:	/* idle state */
		if(c == ESC){ state = state1; return; }
		if(c < 0) return;
		if(!set8 && (c < 128)){
			if(japan646){
				switch(c)
				{
				case '\\':	emit(0xA5); return;	/* yen */
				case '~':	emit(0xAF); return;	/* spacing macron */
				default:	emit(c); return;
				}
			} else {
				emit(c);
				return;
			}
		}
		lastc = c; state = state4; return;

	case state1:	/* seen an escape */
		if(c == '$'){ state = state2; return; }
		if(c == '('){ state = state3; return; }
		emit(ESC); state = state0; goto again;

	case state2:	/* may be shifting into JIS */
		if((c == '@') || (c == 'B')){
			set8 = 1; state = state0; return;
		}
		emit(ESC); emit('$'); state = state0; goto again;

	case state3:	/* may be shifting out of JIS */
		if((c == 'J') || (c == 'H') || (c == 'B')){
			japan646 = (c == 'J');
			set8 = 0; state = state0; return;
		}
		emit(ESC); emit('('); state = state0; goto again;

	case state4:	/* two part char */
		if(c < 0){
			if(squawk)
				EPR "%s: unexpected EOF in %s\n", argv0, file);
			c = 0x21 | (lastc&0x80);
		}
		if(CANS2J(lastc, c)){	/* ms dos sjis */
			int hi = lastc, lo = c;
			S2J(hi, lo);			/* convert to 208 */
			n = hi*100 + lo - 3232;		/* convert to kuten208 */
		} else {
			nerrors++;
			if(squawk)
				EPR "%s: illegal byte pair (0x%x,0x%x) near byte %ld in %s\n", argv0, lastc, c, input_loc, file);
			if(!clean)
				emit(BADMAP);
			state = state0;
			goto again;
		}
		if((n >= KUTEN208MAX) || ((l = tabkuten208[n]) == -1)){
			nerrors++;
			if(squawk)
				EPR "%s: unknown kuten208 %d (from 0x%x,0x%x) near byte %ld in %s\n", argv0, n, lastc, c, input_loc, file);
			if(!clean)
				emit(BADMAP);
		} else {
			if(l < 0){
				l = -l;
				if(squawk)
					EPR "%s: ambiguous kuten208 %d (mapped to 0x%lx) near byte %ld in %s\n", argv0, n, l, input_loc, file);
			}
			emit(l);
		}
		state = state0;
	}
}

/*
	a state machine for interpreting ujis == EUC
*/
static void
ujis(int c, Rune **r, long input_loc)
{
	static enum { state0, state1 } state = state0;
	static int lastc;
	int n;
	long l;

	switch(state)
	{
	case state0:	/* idle state */
		if(c < 0) return;
		if(c < 128){
			emit(c);
			return;
		}
		if(c == 0x8e){	/* codeset 2 */
			nerrors++;
			if(squawk)
				EPR "%s: unknown codeset 2 near byte %ld in %s\n", argv0, input_loc, file);
			if(!clean)
				emit(BADMAP);
			return;
		}
		if(c == 0x8f){	/* codeset 3 */
			nerrors++;
			if(squawk)
				EPR "%s: unknown codeset 3 near byte %ld in %s\n", argv0, input_loc, file);
			if(!clean)
				emit(BADMAP);
			return;
		}
		lastc = c;
		state = state1;
		return;

	case state1:	/* two part char */
		if(c < 0){
			if(squawk)
				EPR "%s: unexpected EOF in %s\n", argv0, file);
			c = 0xA1;
		}
		n = (lastc&0x7F)*100 + (c&0x7F) - 3232;	/* kuten208 */
		if((n >= KUTEN208MAX) || ((l = tabkuten208[n]) == -1)){
			nerrors++;
			if(squawk)
				EPR "%s: unknown kuten208 %d (from 0x%x,0x%x) near byte %ld in %s\n", argv0, n, lastc, c, input_loc, file);
			if(!clean)
				emit(BADMAP);
		} else {
			if(l < 0){
				l = -l;
				if(squawk)
					EPR "%s: ambiguous kuten208 %d (mapped to 0x%lx) near byte %ld in %s\n", argv0, n, l, input_loc, file);
			}
			emit(l);
		}
		state = state0;
	}
}

/*
	a state machine for interpreting jis-kanji == 2022-JP
*/
static void
jis(int c, Rune **r, long input_loc)
{
	static enum { state0, state1, state2, state3, state4 } state = state0;
	static int set8 = 0;
	static int japan646 = 0;
	static int lastc;
	int n;
	long l;

again:
	switch(state)
	{
	case state0:	/* idle state */
		if(c == ESC){ state = state1; return; }
		if(c < 0) return;
		if(!set8 && (c < 128)){
			if(japan646){
				switch(c)
				{
				case '\\':	emit(0xA5); return;	/* yen */
				case '~':	emit(0xAF); return;	/* spacing macron */
				default:	emit(c); return;
				}
			} else {
				emit(c);
				return;
			}
		}
		lastc = c; state = state4; return;

	case state1:	/* seen an escape */
		if(c == '$'){ state = state2; return; }
		if(c == '('){ state = state3; return; }
		emit(ESC); state = state0; goto again;

	case state2:	/* may be shifting into JIS */
		if((c == '@') || (c == 'B')){
			set8 = 1; state = state0; return;
		}
		emit(ESC); emit('$'); state = state0; goto again;

	case state3:	/* may be shifting out of JIS */
		if((c == 'J') || (c == 'H') || (c == 'B')){
			japan646 = (c == 'J');
			set8 = 0; state = state0; return;
		}
		emit(ESC); emit('('); state = state0; goto again;

	case state4:	/* two part char */
		if(c < 0){
			if(squawk)
				EPR "%s: unexpected EOF in %s\n", argv0, file);
			c = 0x21 | (lastc&0x80);
		}
		if((lastc&0x80) != (c&0x80)){	/* guard against latin1 in jis */
			emit(lastc);
			state = state0;
			goto again;
		}
		n = (lastc&0x7F)*100 + (c&0x7f) - 3232;	/* kuten208 */
		if((n >= KUTEN208MAX) || ((l = tabkuten208[n]) == -1)){
			nerrors++;
			if(squawk)
				EPR "%s: unknown kuten208 %d (from 0x%x,0x%x) near byte %ld in %s\n", argv0, n, lastc, c, input_loc, file);
			if(!clean)
				emit(BADMAP);
		} else {
			if(l < 0){
				l = -l;
				if(squawk)
					EPR "%s: ambiguous kuten208 %d (mapped to 0x%lx) near byte %ld in %s\n", argv0, n, l, input_loc, file);
			}
			emit(l);
		}
		state = state0;
	}
}

static void
do_in(int fd, void (*procfn)(int, Rune **, long), struct convert *out)
{
	Rune ob[N];
	Rune *r, *re;
	uchar ibuf[N];
	int n, i;
	long nin;

	r = ob;
	re = ob+N-3;
	nin = 0;
	while((n = read(fd, ibuf, sizeof ibuf)) > 0){
		for(i = 0; i < n; i++){
			(*procfn)(ibuf[i], &r, nin++);
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
	(*procfn)(-1, &r, nin);
	if(r > ob)
		OUT(out, ob, r-ob);
	OUT(out, ob, 0);
}

void
jis_in(int fd, long *notused, struct convert *out)
{
	USED(notused);
	do_in(fd, alljis, out);
}

void
ujis_in(int fd, long *notused, struct convert *out)
{
	USED(notused);
	do_in(fd, ujis, out);
}

void
msjis_in(int fd, long *notused, struct convert *out)
{
	USED(notused);
	do_in(fd, ms, out);
}

void
jisjis_in(int fd, long *notused, struct convert *out)
{
	USED(notused);
	do_in(fd, jis, out);
}

static int first = 1;

static void
tab_init(void)
{
	int i;
	long l;

	first = 0;
	for(i = 0; i < NRUNE; i++)
		tab[i] = -1;
	for(i = 0; i < KUTEN208MAX; i++)
		if((l = tabkuten208[i]) != -1){
			if(l < 0)
				tab[-l] = i;
			else
				tab[l] = i;
		}
}


/*	jis-kanji, or ISO 2022-JP	*/
void
jisjis_out(Rune *base, int n, long *notused)
{
	char *p;
	int i;
	Rune r;
	static enum { ascii, japan646, jp2022 } state = ascii;

	USED(notused);
	if(first)
		tab_init();
	nrunes += n;
	p = obuf;
	for(i = 0; i < n; i++){
		r = base[i];
		if(r < 128){
			if(state == jp2022){
				*p++ = ESC; *p++ = '('; *p++ = 'B';
				state = ascii;
			}
			*p++ = r;
		} else {
			if(tab[r] != -1){
				if(state != jp2022){
					*p++ = ESC; *p++ = '$'; *p++ = 'B';
					state = jp2022;
				}
				*p++ = tab[r]/100 + ' ';
				*p++ = tab[r]%100 + ' ';
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

/*	ms-kanji, or Shift-JIS	*/
void
msjis_out(Rune *base, int n, long *notused)
{
	char *p;
	int i, hi, lo;
	Rune r;

	USED(notused);
	if(first)
		tab_init();
	nrunes += n;
	p = obuf;
	for(i = 0; i < n; i++){
		r = base[i];
		if(r < 128)
			*p++ = r;
		else {
			if(tab[r] != -1){
				hi = tab[r]/100 + ' ';
				lo = tab[r]%100 + ' ';
				J2S(hi, lo);
				*p++ = hi;
				*p++ = lo;
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

/*	ujis, or EUC	*/
void
ujis_out(Rune *base, int n, long *notused)
{
	char *p;
	int i;
	Rune r;

	USED(notused);
	if(first)
		tab_init();
	nrunes += n;
	p = obuf;
	for(i = 0; i < n; i++){
		r = base[i];
		if(r < 128)
			*p++ = r;
		else {
			if(tab[r] != -1){
				*p++ = 0x80 | (tab[r]/100 + ' ');
				*p++ = 0x80 | (tab[r]%100 + ' ');
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
