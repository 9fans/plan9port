#include <u.h>
#include <libc.h>
#include <bin.h>
#include <bio.h>
#include <regexp.h>
#include "../../../libregexp/regcomp.h"
#include "dfa.h"

void rdump(Reprog*);
void dump(Dreprog*);

/*
 * Standard NFA determinization and DFA minimization.
 */
typedef struct Deter Deter;
typedef struct Reiset Reiset;

void ddump(Deter*);

/* state of determinization */
struct Deter
{
	jmp_buf kaboom;	/* jmp on error */

	Bin *bin;		/* bin for temporary allocations */

	Reprog *p;	/* program being determinized */
	uint ninst;		/* number of instructions in program */

	Reiset *alloc;	/* chain of all Reisets */
	Reiset **last;

	Reiset **hash;	/* hash of all Reisets */
	uint nhash;

	Reiset *tmp;	/* temporaries for walk */
	uchar *bits;

	Rune *c;		/* ``interesting'' characters */
	uint nc;
};

/* set of Reinsts: perhaps we should use a bit list instead of the indices? */
struct Reiset
{
	uint *inst;		/* indices of instructions in set */
	uint ninst;		/* size of set */

	Reiset *next;	/* d.alloc chain */
	Reiset *hash;	/* d.hash chain */
	Reiset **delta;	/* where to go on each interesting char */
	uint id;		/* assigned id during minimization */
	uint isfinal;	/* is an accepting (final) state */
};

static Reiset*
ralloc(Deter *d, int ninst)
{
	Reiset *t;

	t = binalloc(&d->bin, sizeof(Reiset)+2*d->nc*sizeof(Reiset*)+sizeof(uint)*ninst, 0);
	if(t == nil)
		longjmp(d->kaboom, 1);
	t->delta = (Reiset**)&t[1];
	t->inst = (uint*)&t->delta[2*d->nc];
	return t;
}

/* find the canonical form a given Reiset */
static Reiset*
findreiset(Deter *d, Reiset *s)
{
	int i, szinst;
	uint h;
	Reiset *t;

	h = 0;
	for(i=0; i<s->ninst; i++)
		h = h*1000003 + s->inst[i];
	h %= d->nhash;

	szinst = s->ninst*sizeof(s->inst[0]);
	for(t=d->hash[h]; t; t=t->hash)
		if(t->ninst==s->ninst && memcmp(t->inst, s->inst, szinst)==0)
			return t;

	t = ralloc(d, s->ninst);
	t->hash = d->hash[h];
	d->hash[h] = t;

	*d->last = t;
	d->last = &t->next;
	t->next = 0;

	t->ninst = s->ninst;
	memmove(t->inst, s->inst, szinst);

	/* delta is filled in later */

	return t;
}

/* convert bits to a real reiset */
static Reiset*
bits2reiset(Deter *d, uchar *bits)
{
	int k;
	Reiset *s;

	s = d->tmp;
	s->ninst = 0;
	for(k=0; k<d->ninst; k++)
		if(bits[k])
			s->inst[s->ninst++] = k;
	return findreiset(d, s);
}

/* add n to state set; if n < k, need to go around again */
static int
add(int n, uchar *bits, int k)
{
	if(bits[n])
		return 0;
	bits[n] = 1;
	return n < k;
}

/* update bits to follow all the empty (non-character-related) transitions possible */
static void
followempty(Deter *d, uchar *bits, int bol, int eol)
{
	int again, k;
	Reinst *i;

	do{
		again = 0;
		for(i=d->p->firstinst, k=0; k < d->ninst; i++, k++){
			if(!bits[k])
				continue;
			switch(i->type){
			case RBRA:
			case LBRA:
				again |= add(i->u2.next - d->p->firstinst, bits, k);
				break;
			case OR:
				again |= add(i->u2.left - d->p->firstinst, bits, k);
				again |= add(i->u1.right - d->p->firstinst, bits, k);
				break;
			case BOL:
				if(bol)
					again |= add(i->u2.next - d->p->firstinst, bits, k);
				break;
			case EOL:
				if(eol)
					again |= add(i->u2.next - d->p->firstinst, bits, k);
				break;
			}
		}
	}while(again);

	/*
	 * Clear bits for useless transitions.  We could do this during
	 * the switch above, but then we have no guarantee of termination
	 * if we get a loop in the regexp.
	 */
	for(i=d->p->firstinst, k=0; k < d->ninst; i++, k++){
		if(!bits[k])
			continue;
		switch(i->type){
		case RBRA:
		case LBRA:
		case OR:
		case BOL:
		case EOL:
			bits[k] = 0;
			break;
		}
	}
}

/*
 * Where does s go if it sees rune r?
 * Eol is true if a $ matches the string at the position just after r.
 */
static Reiset*
transition(Deter *d, Reiset *s, Rune r, uint eol)
{
	int k;
	uchar *bits;
	Reinst *i, *inst0;
	Rune *rp, *ep;

	bits = d->bits;
	memset(bits, 0, d->ninst);

	inst0 = d->p->firstinst;
	for(k=0; k < s->ninst; k++){
		i = inst0 + s->inst[k];
		switch(i->type){
		default:
			werrstr("bad reprog: got type %d", i->type);
			longjmp(d->kaboom, 1);
		case RBRA:
		case LBRA:
		case OR:
		case BOL:
		case EOL:
			werrstr("internal error: got type %d", i->type);
			longjmp(d->kaboom, 1);

		case RUNE:
			if(r == i->u1.r)
				bits[i->u2.next - inst0] = 1;
			break;
		case ANY:
			if(r != L'\n')
				bits[i->u2.next - inst0] = 1;
			break;
		case ANYNL:
			bits[i->u2.next - inst0] = 1;
			break;
		case NCCLASS:
			if(r == L'\n')
				break;
			/* fall through */
		case CCLASS:
			ep = i->u1.cp->end;
			for(rp = i->u1.cp->spans; rp < ep; rp += 2)
				if(rp[0] <= r && r <= rp[1])
					break;
			if((rp < ep) ^! (i->type == CCLASS))
				bits[i->u2.next - inst0] = 1;
			break;
		case END:
			break;
		}
	}

	followempty(d, bits, r=='\n', eol);
	return bits2reiset(d, bits);
}

static int
countinst(Reprog *pp)
{
	int n;
	Reinst *l;

	n = 0;
	l = pp->firstinst;
	while(l++->type)
		n++;
	return n;
}

static void
set(Deter *d, u32int **tab, Rune r)
{
	u32int *u;

	if((u = tab[r/4096]) == nil){
		u = binalloc(&d->bin, 4096/8, 1);
		if(u == nil)
			longjmp(d->kaboom, 1);
		tab[r/4096] = u;
	}
	u[(r%4096)/32] |= 1<<(r%32);
}

/*
 * Compute the list of important characters.
 * Other characters behave like the ones that surround them.
 */
static void
findchars(Deter *d, Reprog *p)
{
	u32int *tab[65536/4096], *u, x;
	Reinst *i;
	Rune *rp, *ep;
	int k, m, n, a;

	memset(tab, 0, sizeof tab);
	set(d, tab, 0);
	set(d, tab, 0xFFFF);
	for(i=p->firstinst; i->type; i++){
		switch(i->type){
		case ANY:
			set(d, tab, L'\n'-1);
			set(d, tab, L'\n');
			set(d, tab, L'\n'+1);
			break;
		case RUNE:
			set(d, tab, i->u1.r-1);
			set(d, tab, i->u1.r);
			set(d, tab, i->u1.r+1);
			break;
		case NCCLASS:
			set(d, tab, L'\n'-1);
			set(d, tab, L'\n');
			set(d, tab, L'\n'+1);
			/* fall through */
		case CCLASS:
			ep = i->u1.cp->end;
			for(rp = i->u1.cp->spans; rp < ep; rp += 2){
				set(d, tab, rp[0]-1);
				set(d, tab, rp[0]);
				set(d, tab, rp[1]);
				set(d, tab, rp[1]+1);
			}
			break;
		}
	}

	n = 0;
	for(k=0; k<nelem(tab); k++){
		if((u = tab[k]) == nil)
			continue;
		for(m=0; m<4096/32; m++){
			if((x = u[m]) == 0)
				continue;
			for(a=0; a<32; a++)
				if(x&(1<<a))
					n++;
		}
	}

	d->c = binalloc(&d->bin, (n+1)*sizeof(Rune), 0);
	if(d->c == 0)
		longjmp(d->kaboom, 1);
	d->nc = n;

	n = 0;
	for(k=0; k<nelem(tab); k++){
		if((u = tab[k]) == nil)
			continue;
		for(m=0; m<4096/32; m++){
			if((x = u[m]) == 0)
				continue;
			for(a=0; a<32; a++)
				if(x&(1<<a))
					d->c[n++] = k*4096+m*32+a;
		}
	}

	d->c[n] = 0;
	if(n != d->nc)
		abort();
}

/*
 * convert the Deter and Reisets into a Dreprog.
 * if dp and c are nil, just return the count of Drecases needed.
 */
static int
buildprog(Deter *d, Reiset **id2set, int nid, Dreprog *dp, Drecase *c)
{
	int i, j, id, n, nn;
	Dreinst *di;
	Reiset *s;

	nn = 0;
	di = 0;
	for(i=0; i<nid; i++){
		s = id2set[i];
		if(c){
			di = &dp->inst[i];
			di->isfinal = s->isfinal;
		}
		n = 0;
		id = -1;
		for(j=0; j<2*d->nc; j++){
			if(s->delta[j]->id != id){
				id = s->delta[j]->id;
				if(c){
					c[n].start = ((j/d->nc)<<16) | d->c[j%d->nc];
					c[n].next = &dp->inst[id];
				}
				n++;
			}
		}
		if(c){
			if(n == 1 && c[0].next == di)
				di->isloop = 1;
			di->c = c;
			di->nc = n;
			c += n;
		}
		nn += n;
	}
	return nn;
}

Dreprog*
dregcvt(Reprog *p)
{
	uchar *bits;
	uint again, n, nid, id;
	Deter d;
	Reiset **id2set, *s, *t, *start[4];
	Dreprog *dp;
	Drecase *c;

	memset(&d, 0, sizeof d);

	if(setjmp(d.kaboom)){
		binfree(&d.bin);
		return nil;
	}

	d.p = p;
	d.ninst = countinst(p);

	d.last = &d.alloc;

	n = d.ninst;
	/* round up to power of two; this loop is the least of our efficiency problems */
	while(n&(n-1))
		n++;
	d.nhash = n;
	d.hash = binalloc(&d.bin, d.nhash*sizeof(Reinst*), 1);

	/* get list of important runes */
	findchars(&d, p);

#ifdef DUMP
	print("relevant chars are: «%S»\n", d.c+1);
#endif

	d.bits = bits = binalloc(&d.bin, d.ninst, 0);
	d.tmp = ralloc(&d, d.ninst);

	/*
	 * Convert to DFA
	 */

	/* 4 start states, depending on initial bol, eol */
	for(n=0; n<4; n++){
		memset(bits, 0, d.ninst);
		bits[p->startinst - p->firstinst] = 1;
		followempty(&d, bits, n&1, n&2);
		start[n] = bits2reiset(&d, bits);
	}

	/* explore the reiset space */
	for(s=d.alloc; s; s=s->next)
		for(n=0; n<2*d.nc; n++)
			s->delta[n] = transition(&d, s, d.c[n%d.nc], n/d.nc);

#ifdef DUMP
	nid = 0;
	for(s=d.alloc; s; s=s->next)
		s->id = nid++;
	ddump(&d);
#endif

	/*
	 * Minimize.
	 */

	/* first class division is final or not */
	for(s=d.alloc; s; s=s->next){
		s->isfinal = 0;
		for(n=0; n<s->ninst; n++)
			if(p->firstinst[s->inst[n]].type == END)
				s->isfinal = 1;
		s->id = s->isfinal;
	}

	/* divide states with different transition tables in id space */
	nid = 2;
	do{
		again = 0;
		for(s=d.alloc; s; s=s->next){
			id = -1;
			for(t=s->next; t; t=t->next){
				if(s->id != t->id)
					continue;
				for(n=0; n<2*d.nc; n++){
					/* until we finish the for(t) loop, s->id and id are same */
					if((s->delta[n]->id == t->delta[n]->id)
					|| (s->delta[n]->id == s->id && t->delta[n]->id == id)
					|| (s->delta[n]->id == id && t->delta[n]->id == s->id))
						continue;
					break;
				}
				if(n == 2*d.nc)
					continue;
				if(id == -1)
					id = nid++;
				t->id = id;
				again = 1;
			}
		}
	}while(again);

#ifdef DUMP
	ddump(&d);
#endif

	/* build dreprog */
	id2set = binalloc(&d.bin, nid*sizeof(Reiset*), 1);
	if(id2set == nil)
		longjmp(d.kaboom, 1);
	for(s=d.alloc; s; s=s->next)
		id2set[s->id] = s;

	n = buildprog(&d, id2set, nid, nil, nil);
	dp = mallocz(sizeof(Dreprog)+nid*sizeof(Dreinst)+n*sizeof(Drecase), 1);
	if(dp == nil)
		longjmp(d.kaboom, 1);
	c = (Drecase*)&dp->inst[nid];
	buildprog(&d, id2set, nid, dp, c);

	for(n=0; n<4; n++)
		dp->start[n] = &dp->inst[start[n]->id];
	dp->ninst = nid;

	binfree(&d.bin);
	return dp;
}

int
dregexec(Dreprog *p, char *s, int bol)
{
	Rune r;
	ulong rr;
	Dreinst *i;
	Drecase *c, *ec;
	int best, n;
	char *os;

	i = p->start[(bol ? 1 : 0) | (s[1]=='\n' ? 2 : 0)];
	best = -1;
	os = s;
	for(; *s; s+=n){
		if(i->isfinal)
			best = s - os;
		if(i->isloop){
			if(i->isfinal)
				return strlen(os);
			else
				return best;
		}
		if((*s&0xFF) < Runeself){
			r = *s;
			n = 1;
		}else
			n = chartorune(&r, s);
		c = i->c;
		ec = c+i->nc;
		rr = r;
		if(s[n] == '\n' || s[n] == '\0')
			rr |= 0x10000;
		for(; c<ec; c++){
			if(c->start > rr){
				i = c[-1].next;
				goto Out;
			}
		}
		i = ec[-1].next;
	Out:;
	}
	if(i->isfinal)
		best = s - os;
	return best;
}


#ifdef DUMP
void
ddump(Deter *d)
{
	int i, id;
	Reiset *s;

	for(s=d->alloc; s; s=s->next){
		print("%d ", s->id);
		id = -1;
		for(i=0; i<2*d->nc; i++){
			if(id != s->delta[i]->id){
				if(i==0)
					print(" [");
				else if(i/d->nc)
					print(" [%C$", d->c[i%d->nc]);
				else
					print(" [%C", d->c[i%d->nc]);
				print(" %d]", s->delta[i]->id);
				id = s->delta[i]->id;
			}
		}
		print("\n");
	}
}

void
rdump(Reprog *pp)
{
	Reinst *l;
	Rune *p;

	l = pp->firstinst;
	do{
		print("%ld:\t0%o\t%ld\t%ld", l-pp->firstinst, l->type,
			l->left-pp->firstinst, l->right-pp->firstinst);
		if(l->type == RUNE)
			print("\t%C\n", l->r);
		else if(l->type == CCLASS || l->type == NCCLASS){
			print("\t[");
			if(l->type == NCCLASS)
				print("^");
			for(p = l->cp->spans; p < l->cp->end; p += 2)
				if(p[0] == p[1])
					print("%C", p[0]);
				else
					print("%C-%C", p[0], p[1]);
			print("]\n");
		} else
			print("\n");
	}while(l++->type);
}

void
dump(Dreprog *pp)
{
	int i, j;
	Dreinst *l;

	print("start %ld %ld %ld %ld\n",
		pp->start[0]-pp->inst,
		pp->start[1]-pp->inst,
		pp->start[2]-pp->inst,
		pp->start[3]-pp->inst);

	for(i=0; i<pp->ninst; i++){
		l = &pp->inst[i];
		print("%d:", i);
		for(j=0; j<l->nc; j++){
			print(" [");
			if(j == 0)
				if(l->c[j].start != 1)
					abort();
			if(j != 0)
				print("%C%s", l->c[j].start&0xFFFF, (l->c[j].start&0x10000) ? "$" : "");
			print("-");
			if(j != l->nc-1)
				print("%C%s", (l->c[j+1].start&0xFFFF)-1, (l->c[j+1].start&0x10000) ? "$" : "");
			print("] %ld", l->c[j].next - pp->inst);
		}
		if(l->isfinal)
			print(" final");
		if(l->isloop)
			print(" loop");
		print("\n");
	}
}


void
main(int argc, char **argv)
{
	int i;
	Reprog *p;
	Dreprog *dp;

	i = 1;
		p = regcomp(argv[i]);
		if(p == 0){
			print("=== %s: bad regexp\n", argv[i]);
		}
	/*	print("=== %s\n", argv[i]); */
	/*	rdump(p); */
		dp = dregcvt(p);
		print("=== dfa\n");
		dump(dp);

	for(i=2; i<argc; i++)
		print("match %d\n", dregexec(dp, argv[i], 0));
	exits(0);
}
#endif

void
Bprintdfa(Biobuf *b, Dreprog *p)
{
	int i, j, nc;

	Bprint(b, "# dreprog\n");
	nc = 0;
	for(i=0; i<p->ninst; i++)
		nc += p->inst[i].nc;
	Bprint(b, "%d %d %ld %ld %ld %ld\n", p->ninst, nc,
		p->start[0]-p->inst, p->start[1]-p->inst,
		p->start[2]-p->inst, p->start[3]-p->inst);
	for(i=0; i<p->ninst; i++){
		Bprint(b, "%d %d %d", p->inst[i].isfinal, p->inst[i].isloop, p->inst[i].nc);
		for(j=0; j<p->inst[i].nc; j++)
			Bprint(b, " %d %ld", p->inst[i].c[j].start, p->inst[i].c[j].next-p->inst);
		Bprint(b, "\n");
	}
}

static char*
egetline(Biobuf *b, int c, jmp_buf jb)
{
	char *p;

	p = Brdline(b, c);
	if(p == nil)
		longjmp(jb, 1);
	p[Blinelen(b)-1] = '\0';
	return p;
}

static void
egetc(Biobuf *b, int c, jmp_buf jb)
{
	if(Bgetc(b) != c)
		longjmp(jb, 1);
}

static int
egetnum(Biobuf *b, int want, jmp_buf jb)
{
	int c;
	int n, first;

	n = 0;
	first = 1;
	while((c = Bgetc(b)) != Beof){
		if(c < '0' || c > '9'){
			if(want == 0){
				Bungetc(b);
				c = 0;
			}
			if(first || c != want){
				werrstr("format error");
				longjmp(jb, 1);
			}
			return n;
		}
		n = n*10 + c - '0';
		first = 0;
	}
	werrstr("unexpected eof");
	longjmp(jb, 1);
	return -1;
}

Dreprog*
Breaddfa(Biobuf *b)
{
	char *s;
	int ninst, nc;
	jmp_buf jb;
	Dreprog *p;
	Drecase *c;
	Dreinst *l;
	int j, k;

	p = nil;
	if(setjmp(jb)){
		free(p);
		return nil;
	}

	s = egetline(b, '\n', jb);
	if(strcmp(s, "# dreprog") != 0){
		werrstr("format error");
		longjmp(jb, 1);
	}

	ninst = egetnum(b, ' ', jb);
	nc = egetnum(b, ' ', jb);

	p = mallocz(sizeof(Dreprog)+ninst*sizeof(Dreinst)+nc*sizeof(Drecase), 1);
	if(p == nil)
		longjmp(jb, 1);
	c = (Drecase*)&p->inst[ninst];

	p->start[0] = &p->inst[egetnum(b, ' ', jb)];
	p->start[1] = &p->inst[egetnum(b, ' ', jb)];
	p->start[2] = &p->inst[egetnum(b, ' ', jb)];
	p->start[3] = &p->inst[egetnum(b, '\n', jb)];

	for(j=0; j<ninst; j++){
		l = &p->inst[j];
		l->isfinal = egetnum(b, ' ', jb);
		l->isloop = egetnum(b, ' ', jb);
		l->nc = egetnum(b, 0, jb);
		l->c = c;
		for(k=0; k<l->nc; k++){
			egetc(b, ' ', jb);
			c->start = egetnum(b, ' ', jb);
			c->next = &p->inst[egetnum(b, 0, jb)];
			c++;
		}
		egetc(b, '\n', jb);
	}
	return p;
}
