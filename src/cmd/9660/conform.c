#include <u.h>
#include <libc.h>
#include <bio.h>
#include <libsec.h>
#include <ctype.h>
#include "iso9660.h"

/*
 * We keep an array sorted by bad atom pointer.
 * The theory is that since we don't free memory very often,
 * the array will be mostly sorted already and insertions will
 * usually be near the end, so we won't spend much time
 * keeping it sorted.
 */

/*
 * Binary search a Tx list.
 * If no entry is found, return a pointer to
 * where a new such entry would go.
 */
static Tx*
txsearch(char *atom, Tx *t, int n)
{
	while(n > 0) {
		if(atom < t[n/2].bad)
			n = n/2;
		else if(atom > t[n/2].bad) {
			t += n/2+1;
			n -= (n/2+1);
		} else
			return &t[n/2];
	}
	return t;
}

void
addtx(char *b, char *g)
{
	Tx *t;
	Conform *c;

	if(map == nil)
		map = emalloc(sizeof(*map));
	c = map;

	if(c->nt%32 == 0)
		c->t = erealloc(c->t, (c->nt+32)*sizeof(c->t[0]));
	t = txsearch(b, c->t, c->nt);
	if(t < c->t+c->nt && t->bad == b) {
		fprint(2, "warning: duplicate entry for %s in _conform.map\n", b);
		return;
	}

	if(t != c->t+c->nt)
		memmove(t+1, t, (c->t+c->nt - t)*sizeof(Tx));
	t->bad = b;
	t->good = g;
	c->nt++;
}

char*
conform(char *s, int isdir)
{
	Tx *t;
	char buf[10], *g;
	Conform *c;

	c = map;
	s = atom(s);
	if(c){
		t = txsearch(s, c->t, c->nt);
		if(t < c->t+c->nt && t->bad == s)
			return t->good;
	}

	sprint(buf, "%c%.6d", isdir ? 'D' : 'F', c ? c->nt : 0);
	g = atom(buf);
	addtx(s, g);
	return g;
}

#ifdef NOTUSED
static int
isalldigit(char *s)
{
	while(*s)
		if(!isdigit(*s++))
			return 0;
	return 1;
}
#endif

static int
goodcmp(const void *va, const void *vb)
{
	Tx *a, *b;

	a = (Tx*)va;
	b = (Tx*)vb;
	return strcmp(a->good, b->good);
}

static int
badatomcmp(const void *va, const void *vb)
{
	Tx *a, *b;

	a = (Tx*)va;
	b = (Tx*)vb;
	if(a->good < b->good)
		return -1;
	if(a->good > b->good)
		return 1;
	return 0;
}

void
wrconform(Cdimg *cd, int n, ulong *pblock, ulong *plength)
{
	char buf[1024];
	int i;
	Conform *c;

	c = map;
	*pblock = cd->nextblock;
	if(c==nil || n==c->nt){
		*plength = 0;
		return;
	}

	Cwseek(cd, cd->nextblock*Blocksize);
	qsort(c->t, c->nt, sizeof(c->t[0]), goodcmp);
	for(i=n; i<c->nt; i++) {
		snprint(buf, sizeof buf, "%s %s\n", c->t[i].good, c->t[i].bad);
		Cwrite(cd, buf, strlen(buf));
	}
	qsort(c->t, c->nt, sizeof(c->t[0]), badatomcmp);
	*plength = Cwoffset(cd) - *pblock*Blocksize;
	chat("write _conform.map at %lud+%lud\n", *pblock, *plength);
	Cpadblock(cd);
}
