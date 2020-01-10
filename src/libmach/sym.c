#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

int machdebug = 0;

Fhdr *fhdrlist;
static Fhdr *last;

static void
relocsym(Symbol *dst, Symbol *src, ulong base)
{
	if(dst != src)
		*dst = *src;
	if(dst->loc.type == LADDR)
		dst->loc.addr += base;
	if(dst->hiloc.type == LADDR)
		dst->hiloc.addr += base;
}

void
_addhdr(Fhdr *h)
{
	h->next = nil;
	if(fhdrlist == nil){
		fhdrlist = h;
		last = h;
	}else{
		last->next = h;
		last = h;
	}
}

void
_delhdr(Fhdr *h)
{
	Fhdr *p;

	if(h == fhdrlist)
		fhdrlist = h->next;
	else{
		for(p=fhdrlist; p && p->next!=h; p=p->next)
			;
		if(p){
			p->next = h->next;
			if(p->next == nil)
				last = p;
		}
	}
	h->next = nil;
}

Fhdr*
findhdr(char *name)
{
	int len, plen;
	Fhdr *p;

	len = strlen(name);
	for(p=fhdrlist; p; p=p->next){
		plen = strlen(p->filename);
		if(plen >= len)
		if(strcmp(p->filename+plen-len, name) == 0)
		if(plen == len || p->filename[plen-len-1] == '/')
			return p;
	}
	return nil;
}

int
pc2file(u64int pc, char *file, uint nfile, ulong *line)
{
	Fhdr *p;

	for(p=fhdrlist; p; p=p->next)
		if(p->pc2file && p->pc2file(p, pc-p->base, file, nfile, line) >= 0)
			return 0;
	werrstr("no source file for 0x%lux", pc);
	return -1;
}

int
pc2line(u64int pc, ulong *line)
{
	char tmp[10];	/* just in case */
	return pc2file(pc, tmp, sizeof tmp, line);
}

int
file2pc(char *file, ulong line, u64int *addr)
{
	Fhdr *p;

	for(p=fhdrlist; p; p=p->next)
		if(p->file2pc && p->file2pc(p, file, line, addr) >= 0){
			*addr += p->base;
			return 0;
		}
	werrstr("no instructions at %s:%lud", file, line);
	return -1;
}

int
line2pc(u64int basepc, ulong line, u64int *pc)
{
	Fhdr *p;

	for(p=fhdrlist; p; p=p->next)
		if(p->line2pc && p->line2pc(p, basepc-p->base, line, pc) >= 0){
			*pc += p->base;
			return 0;
		}
	werrstr("no instructions on line %lud", line);
	return -1;
}

int
fnbound(u64int pc, u64int *bounds)
{
	Fhdr *p;
	Loc l;
	Symbol *s;

	for(p=fhdrlist; p; p=p->next){
		l = locaddr(pc - p->base);
		if((s = ffindsym(p, l, CANY)) != nil){
			if(s->loc.type != LADDR){
				werrstr("function %s has weird location %L", s->name, s->loc);
				return -1;
			}
			bounds[0] = s->loc.addr + p->base;
			if(s->hiloc.type != LADDR){
				werrstr("can't find upper bound for function %s", s->name);
				return -1;
			}
			bounds[1] = s->hiloc.addr + p->base;
			return 0;
		}
	}
	werrstr("no function contains 0x%lux", pc);
	return -1;
}

int
fileline(u64int pc, char *a, uint n)
{
	ulong line;

	if(pc2file(pc, a, n, &line) < 0)
		return -1;
	seprint(a+strlen(a), a+n, ":%lud", line);
	return 0;
}

Symbol*
flookupsym(Fhdr *fhdr, char *name)
{
	Symbol **a, *t;
	uint n, m;
	int i;

	a = fhdr->byname;
	n = fhdr->nsym;
	if(a == nil)
		return nil;

	while(n > 0){
		m = n/2;
		t = a[m];
		i = strcmp(name, t->name);
		if(i < 0)
			n = m;
		else if(i > 0){
			n -= m+1;
			a += m+1;
		}else{
			/* found! */
			m += a - fhdr->byname;
			a = fhdr->byname;
			assert(strcmp(name, a[m]->name) == 0);
			while(m > 0 && strcmp(name, a[m-1]->name) == 0)
				m--;
			return a[m];
		}
	}
	return nil;
}

Symbol*
flookupsymx(Fhdr *fhdr, char *name)
{
	Symbol **a, *t;
	uint n, m;
	int i;

	a = fhdr->byxname;
	n = fhdr->nsym;
	if(a == nil)
		return nil;

	while(n > 0){
		m = n/2;
		t = a[m];
		i = strcmp(name, t->xname);
		if(i < 0)
			n = m;
		else if(i > 0){
			n -= m+1;
			a += m+1;
		}else{
			/* found! */
			m += a - fhdr->byxname;
			a = fhdr->byxname;
			assert(strcmp(name, a[m]->xname) == 0);
			while(m > 0 && strcmp(name, a[m-1]->xname) == 0)
				m--;
			return a[m];
		}
	}
	return nil;
}

int
lookupsym(char *fn, char *var, Symbol *s)
{
	Symbol *t, s1;
	Fhdr *p;
	char *nam;

	nam = fn ? fn : var;
	if(nam == nil)
		return -1;
	t = nil;
	for(p=fhdrlist; p; p=p->next)
		if((t=flookupsym(p, nam)) != nil
		|| (t=flookupsymx(p, nam)) != nil){
			relocsym(&s1, t, p->base);
			break;
		}

	if(t == nil)
		goto err;
	if(fn && var)
		return lookuplsym(&s1, var, s);
	*s = s1;
	return 0;

err:
	werrstr("unknown symbol %s%s%s", fn ? fn : "",
		fn && var ? ":" : "", var ? var : "");
	return -1;
}

int
findexsym(Fhdr *fp, uint i, Symbol *s)
{
	if(i >= fp->nsym)
		return -1;
	relocsym(s, &fp->sym[i], fp->base);
	return 0;
}

int
indexsym(uint ndx, Symbol *s)
{
	uint t;
	Fhdr *p;

	for(p=fhdrlist; p; p=p->next){
		t = p->nsym;
		if(t < ndx)
			ndx -= t;
		else{
			relocsym(s, &p->sym[ndx], p->base);
			return 0;
		}
	}
	return -1;
}

Symbol*
ffindsym(Fhdr *fhdr, Loc loc, uint class)
{
	Symbol *a, *t;
	int n, i, hi, lo;
	int cmp;

	a = fhdr->sym;
	n = fhdr->nsym;
	if(a == nil || n <= 0)
		return nil;

	/*
	 * We have a list of possibly duplicate locations in a.
	 * We want to find the largest index i such that
	 * a[i] <= loc.  This cannot be done with a simple
	 * binary search.  Instead we binary search to find
	 * where the location should be.
	 */
	lo = 0;
	hi = n;
	while(lo < hi){
		i = (lo+hi)/2;
		cmp = loccmp(&loc, &a[i].loc);
		if(cmp < 0)	/* loc < a[i].loc */
			hi = i;
		if(cmp > 0)	/* loc > a[i].loc */
			lo = i+1;
		if(cmp == 0)
			goto found;
	}

	/* found position where value would go, but not there -- go back one */
	if(lo == 0)
		return nil;
	i = lo-1;

found:
	/*
	 * might be in a run of all-the-same -- go back to beginning of run.
	 * if runs were long, could binary search for a[i].loc instead.
	 */
	while(i > 0 && loccmp(&a[i-1].loc, &a[i].loc) == 0)
		i--;

	t = &a[i];
	if(t->hiloc.type && loccmp(&loc, &t->hiloc) >= 0)
		return nil;
	if(class != CANY && class != t->class)
		return nil;
	return t;
}

int
findsym(Loc loc, uint class, Symbol *s)
{
	Fhdr *p, *bestp;
	Symbol *t, *best;
	long bestd, d;
	Loc l;

	l = loc;
	best = nil;
	bestp = nil;
	bestd = 0;
	for(p=fhdrlist; p; p=p->next){
		if(l.type == LADDR)
			l.addr = loc.addr - p->base;
		if((t = ffindsym(p, l, CANY)) != nil){
			d = l.addr - t->loc.addr;
			if(0 <= d && d < 4096)
			if(best == nil || d < bestd){
				best = t;
				bestp = p;
				bestd = d;
			}
		}
	}
	if(best){
		if(class != CANY && class != best->class)
			goto err;
		relocsym(s, best, bestp->base);
		return 0;
	}
err:
	werrstr("could not find symbol at %L", loc);
	return -1;
}

int
lookuplsym(Symbol *s1, char *name, Symbol *s2)
{
	Fhdr *p;

	p = s1->fhdr;
	if(p->lookuplsym && p->lookuplsym(p, s1, name, s2) >= 0){
		relocsym(s2, s2, p->base);
		return 0;
	}
	return -1;
}

int
indexlsym(Symbol *s1, uint ndx, Symbol *s2)
{
	Fhdr *p;

	p = s1->fhdr;
	if(p->indexlsym && p->indexlsym(p, s1, ndx, s2) >= 0){
		relocsym(s2, s2, p->base);
		return 0;
	}
	return -1;
}

int
findlsym(Symbol *s1, Loc loc, Symbol *s2)
{
	Fhdr *p;

	p = s1->fhdr;
	if(p->findlsym && p->findlsym(p, s1, loc, s2) >= 0){
		relocsym(s2, s2, p->base);
		return 0;
	}
	return -1;
}

int
unwindframe(Map *map, Regs *regs, u64int *next, Symbol *sym)
{
	Fhdr *p;

	for(p=fhdrlist; p; p=p->next)
		if(p->unwind && p->unwind(p, map, regs, next, sym) >= 0)
			return 0;
	if(mach->unwind && mach->unwind(map, regs, next, sym) >= 0)
		return 0;
	return -1;
}

int
symoff(char *a, uint n, u64int addr, uint class)
{
	Loc l;
	Symbol s;

	l.type = LADDR;
	l.addr = addr;
	if(findsym(l, class, &s) < 0 || addr-s.loc.addr >= 4096){
		snprint(a, n, "%#lux", addr);
		return -1;
	}
	if(addr != s.loc.addr)
		snprint(a, n, "%s+%#lx", s.name, addr-s.loc.addr);
	else
		snprint(a, n, "%s", s.name);
	return 0;
}

/* location, class, name */
static int
byloccmp(const void *va, const void *vb)
{
	int i;
	Symbol *a, *b;

	a = (Symbol*)va;
	b = (Symbol*)vb;
	i = loccmp(&a->loc, &b->loc);
	if(i != 0)
		return i;
	i = a->class - b->class;
	if(i != 0)
		return i;
	return strcmp(a->name, b->name);
}

/* name, location, class */
static int
byxnamecmp(const void *va, const void *vb)
{
	int i;
	Symbol *a, *b;

	a = *(Symbol**)va;
	b = *(Symbol**)vb;
	i = strcmp(a->xname, b->xname);
	if(i != 0)
		return i;
	i = strcmp(a->name, b->name);
	if(i != 0)
		return i;
	i = loccmp(&a->loc, &b->loc);
	if(i != 0)
		return i;
	return a->class - b->class;
}

/* name, location, class */
static int
bynamecmp(const void *va, const void *vb)
{
	int i;
	Symbol *a, *b;

	a = *(Symbol**)va;
	b = *(Symbol**)vb;
	i = strcmp(a->name, b->name);
	if(i != 0)
		return i;
	i = loccmp(&a->loc, &b->loc);
	if(i != 0)
		return i;
	return a->class - b->class;
}

int
symopen(Fhdr *hdr)
{
	int i;
	Symbol *r, *w, *es;

	if(hdr->syminit == 0){
		werrstr("no debugging symbols");
		return -1;
	}
	if(hdr->syminit(hdr) < 0)
		return -1;

	qsort(hdr->sym, hdr->nsym, sizeof(hdr->sym[0]), byloccmp);
	es = hdr->sym+hdr->nsym;
	for(r=w=hdr->sym; r<es; r++){
		if(w > hdr->sym
		&& strcmp((w-1)->name, r->name) ==0
		&& loccmp(&(w-1)->loc, &r->loc) == 0){
			/* skip it */
		}else
			*w++ = *r;
	}
	hdr->nsym = w - hdr->sym;

	hdr->byname = malloc(hdr->nsym*sizeof(hdr->byname[0]));
	if(hdr->byname == nil){
		fprint(2, "could not allocate table to sort by name\n");
	}else{
		for(i=0; i<hdr->nsym; i++)
			hdr->byname[i] = &hdr->sym[i];
		qsort(hdr->byname, hdr->nsym, sizeof(hdr->byname[0]), bynamecmp);
	}

	hdr->byxname = malloc(hdr->nsym*sizeof(hdr->byxname[0]));
	if(hdr->byxname == nil){
		fprint(2, "could not allocate table to sort by xname\n");
	}else{
		for(i=0; i<hdr->nsym; i++)
			hdr->byxname[i] = &hdr->sym[i];
		qsort(hdr->byxname, hdr->nsym, sizeof(hdr->byxname[0]), byxnamecmp);
	}
	return 0;
}

void
symclose(Fhdr *hdr)
{
	_delhdr(hdr);
	if(hdr->symclose)
		hdr->symclose(hdr);
	free(hdr->byname);
	hdr->byname = nil;
	free(hdr->sym);
	hdr->sym = nil;
	hdr->nsym = 0;
}

Symbol*
_addsym(Fhdr *fp, Symbol *sym)
{
	char *t;
	static char buf[65536];
	Symbol *s;

	if(fp->nsym%128 == 0){
		s = realloc(fp->sym, (fp->nsym+128)*sizeof(fp->sym[0]));
		if(s == nil)
			return nil;
		fp->sym = s;
	}
	if(machdebug)
		fprint(2, "sym %s %c %L\n", sym->name, sym->type, sym->loc);
	sym->fhdr = fp;
	t = demangle(sym->name, buf, 1);
	if(t != sym->name){
		t = strdup(t);
		if(t == nil)
			return nil;
	}
	sym->xname = t;
	s = &fp->sym[fp->nsym++];
	*s = *sym;
	return s;
}
