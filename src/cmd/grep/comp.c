#include	"grep.h"

/*
 * incremental compiler.
 * add the branch c to the
 * state s.
 */
void
increment(State *s, int c)
{
	int i;
	State *t, **tt;
	Re *re1, *re2;

	nfollow = 0;
	gen++;
	matched = 0;
	for(i=0; i<s->count; i++)
		fol1(s->re[i], c);
	qsort(follow, nfollow, sizeof(*follow), fcmp);
	for(tt=&state0; t = *tt;) {
		if(t->count > nfollow) {
			tt = &t->linkleft;
			goto cont;
		}
		if(t->count < nfollow) {
			tt = &t->linkright;
			goto cont;
		}
		for(i=0; i<nfollow; i++) {
			re1 = t->re[i];
			re2 = follow[i];
			if(re1 > re2) {
				tt = &t->linkleft;
				goto cont;
			}
			if(re1 < re2) {
				tt = &t->linkright;
				goto cont;
			}
		}
		if(!!matched && !t->match) {
			tt = &t->linkleft;
			goto cont;
		}
		if(!matched && !!t->match) {
			tt = &t->linkright;
			goto cont;
		}
		s->next[c] = t;
		return;
	cont:;
	}

	t = sal(nfollow);
	*tt = t;
	for(i=0; i<nfollow; i++) {
		re1 = follow[i];
		t->re[i] = re1;
	}
	s->next[c] = t;
	t->match = matched;
}

int
fcmp(const void *va, const void *vb)
{
	Re **aa, **bb;
	Re *a, *b;

	aa = (Re**)va;
	bb = (Re**)vb;
	a = *aa;
	b = *bb;
	if(a > b)
		return 1;
	if(a < b)
		return -1;
	return 0;
}

void
fol1(Re *r, int c)
{
	Re *r1;

loop:
	if(r->gen == gen)
		return;
	if(nfollow >= maxfollow)
		error("nfollow");
	r->gen = gen;
	switch(r->type) {
	default:
		error("fol1");

	case Tcase:
		if(c >= 0 && c < 256)
		if(r1 = r->u.cases[c])
			follow[nfollow++] = r1;
		if(r = r->next)
			goto loop;
		break;

	case Talt:
	case Tor:
		fol1(r->u.alt, c);
		r = r->next;
		goto loop;

	case Tbegin:
		if(c == '\n' || c == Cbegin)
			follow[nfollow++] = r->next;
		break;

	case Tend:
		if(c == '\n')
			matched = 1;
		break;

	case Tclass:
		if(c >= r->u.x.lo && c <= r->u.x.hi)
			follow[nfollow++] = r->next;
		break;
	}
}

Rune	tab1[] =
{
	0x007f,
	0x07ff,
};
Rune	tab2[] =
{
	0x003f,
	0x0fff,
};

Re2
rclass(Rune p0, Rune p1)
{
	char xc0[6], xc1[6];
	int i, n, m;
	Re2 x;

	if(p0 > p1)
		return re2char(0xff, 0xff);	// no match

	/*
	 * bust range into same length
	 * character sequences
	 */
	for(i=0; i<nelem(tab1); i++) {
		m = tab1[i];
		if(p0 <= m && p1 > m)
			return re2or(rclass(p0, m), rclass(m+1, p1));
	}

	/*
	 * bust range into part of a single page
	 * or into full pages
	 */
	for(i=0; i<nelem(tab2); i++) {
		m = tab2[i];
		if((p0 & ~m) != (p1 & ~m)) {
			if((p0 & m) != 0)
				return re2or(rclass(p0, p0|m), rclass((p0|m)+1, p1));
			if((p1 & m) != m)
				return re2or(rclass(p0, (p1&~m)-1), rclass(p1&~m, p1));
		}
	}

	n = runetochar(xc0, &p0);
	i = runetochar(xc1, &p1);
	if(i != n)
		error("length");

	x = re2char(xc0[0], xc1[0]);
	for(i=1; i<n; i++)
		x = re2cat(x, re2char(xc0[i], xc1[i]));
	return x;
}

int
pcmp(const void *va, const void *vb)
{
	int n;
	Rune *a, *b;

	a = (Rune*)va;
	b = (Rune*)vb;

	n = a[0] - b[0];
	if(n)
		return n;
	return a[1] - b[1];
}

/*
 * convert character chass into
 * run-pair ranges of matches.
 * this is 10646/utf specific and
 * needs to be changed for some
 * other input character set.
 * this is the key to a fast
 * regular search of characters
 * by looking at sequential bytes.
 */
Re2
re2class(char *s)
{
	Rune pairs[200], *p, *q, ov;
	int nc;
	Re2 x;

	nc = 0;
	if(*s == '^') {
		nc = 1;
		s++;
	}

	p = pairs;
	s += chartorune(p, s);
	for(;;) {
		if(*p == '\\')
			s += chartorune(p, s);
		if(*p == 0)
			break;
		p[1] = *p;
		p += 2;
		s += chartorune(p, s);
		if(*p != '-')
			continue;
		s += chartorune(p, s);
		if(*p == '\\')
			s += chartorune(p, s);
		if(*p == 0)
			break;
		p[-1] = *p;
		s += chartorune(p, s);
	}
	*p = 0;
	qsort(pairs, (p-pairs)/2, 2*sizeof(*pairs), pcmp);

	q = pairs;
	for(p=pairs+2; *p; p+=2) {
		if(p[0] > p[1])
			continue;
		if(p[0] > q[1] || p[1] < q[0]) {
			q[2] = p[0];
			q[3] = p[1];
			q += 2;
			continue;
		}
		if(p[0] < q[0])
			q[0] = p[0];
		if(p[1] > q[1])
			q[1] = p[1];
	}
	q[2] = 0;

	p = pairs;
	if(nc) {
		x = rclass(0, p[0]-1);
		ov = p[1]+1;
		for(p+=2; *p; p+=2) {
			x = re2or(x, rclass(ov, p[0]-1));
			ov = p[1]+1;
		}
		x = re2or(x, rclass(ov, 0xffff));
	} else {
		x = rclass(p[0], p[1]);
		for(p+=2; *p; p+=2)
			x = re2or(x, rclass(p[0], p[1]));
	}
	return x;
}
