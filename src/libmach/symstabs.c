#include <u.h>
#include <libc.h>
#include <mach.h>
#include "stabs.h"

static int
strcmpcolon(char *a, char *bcolon)
{
	int i, len;
	char *p;

	p = strchr(bcolon, ':');
	if(p == nil)
		return strcmp(a, bcolon);
	len = p-bcolon;
	i = strncmp(a, bcolon, len);
	if(i)
		return i;
	if(a[len] == 0)
		return 0;
	return 1;
}

static int
stabcvtsym(StabSym *stab, Symbol *sym, char *dir, char *file, int i)
{
	char *p;

	/*
	 * Zero out the : to avoid allocating a new name string.
	 * The type info can be found by looking past the NUL.
	 * This is going to get us in trouble...
	 */
	if((p = strchr(stab->name, ':')) != nil)
		*p++ = 0;
	else
		p = stab->name+strlen(stab->name)+1;

	sym->name = stab->name;
	sym->u.stabs.dir = dir;
	sym->u.stabs.file = file;
	sym->u.stabs.i = i;
	switch(stab->type){
	default:
		return -1;
	case N_FUN:
		sym->class = CTEXT;
		switch(*p){
		default:
			return -1;
		case 'F':	/* global function */
			sym->type = 'T';
			break;
		case 'Q':	/* static procedure */
		case 'f':	/* static function */
		case 'I':	/* nested procedure */
		case 'J':	/* nested function */
			sym->type = 't';
			break;
		}
		sym->loc.type = LADDR;
		sym->loc.addr = stab->value;
		break;
	case N_GSYM:
	case N_PSYM:
	case N_LSYM:
	case N_LCSYM:
		sym->class = CDATA;
		sym->loc.type = LADDR;
		sym->loc.addr = stab->value;
		switch(*p){
		default:
			return -1;
		case 'S':	/* file-scope static variable */
			sym->type = 'd';
			break;
		case 'G':	/* global variable */
			sym->type = 'D';
			sym->loc.type = LNONE;
			break;
		case 'r':	/* register variable */
			sym->class = CAUTO;
			sym->type = 'a';
			sym->loc.type = LREG;
			sym->loc.reg = "XXX";
			break;
		case 's':	/* local variable */
			sym->class = CAUTO;
			sym->type = 'a';
			sym->loc.type = LOFFSET;
			sym->loc.offset = stab->value;
			sym->loc.reg = "XXX";
			break;
		case 'a':	/* by reference */
		case 'D':	/* f.p. parameter */
		case 'i':	/* register parameter */
		case 'p':	/* "normal" parameter */
		case 'P':	/* register parameter */
		case 'v':	/* by reference */
		case 'X':	/* function return variable */
			sym->class = CPARAM;
			sym->type = 'p';
			if(*p == 'i'){
				sym->loc.type = LREG;
				sym->loc.reg = "XXX";
			}else{
				sym->loc.type = LOFFSET;
				sym->loc.offset = stab->value;
				sym->loc.reg = "XXX";
			}
			break;
		}
		break;
	}
	return 0;
}

static int
stabssyminit(Fhdr *fp)
{
	int i;
	char *dir, *file;
	Stab *stabs;
	StabSym sym, lastfun;
	Symbol s, *fun;
	char **inc, **xinc;
	int ninc, minc;
	int locals, autos, params;

	stabs = &fp->stabs;
	if(stabs == nil){
		werrstr("no stabs info");
		return -1;
	}

	dir = nil;
	file = nil;
	inc = nil;
	fun = nil;
	ninc = 0;
	minc = 0;
	locals = 0;
	params = 0;
	autos = 0;
	memset(&lastfun, 0, sizeof lastfun);
	for(i=0; stabsym(stabs, i, &sym)>=0; i++){
		switch(sym.type){
		case N_SO:
			if(sym.name == nil || *sym.name == 0){
				file = nil;
				break;
			}
			if(sym.name[strlen(sym.name)-1] == '/')
				dir = sym.name;
			else
				file = sym.name;
			break;
		case N_BINCL:
			if(ninc >= minc){
				xinc = realloc(inc, (ninc+32)*sizeof(inc[0]));
				if(xinc){
					memset(xinc+ninc, 0, 32*sizeof(inc[0]));
					inc = xinc;
				}
				ninc += 32;
			}
			if(ninc < minc)
				inc[ninc] = sym.name;
			ninc++;
			break;
		case N_EINCL:
			if(ninc > 0)
				ninc--;
			break;
		case N_EXCL:
			/* condensed include - same effect as previous BINCL/EINCL pair */
			break;
		case N_GSYM:	/* global variable */
			/* only includes type, so useless for now */
			break;
		case N_FUN:
			if(sym.name == nil){
				/* marks end of function */
				if(fun){
					fun->hiloc.type = LADDR;
					fun->hiloc.addr = fun->loc.addr + sym.value;
				}
				break;
			}
			if(fun && lastfun.value==sym.value && lastfun.name==sym.name){
				fun->u.stabs.locals = i;
				break;
			}
			/* create new symbol, add it */
			lastfun = sym;
			fun = nil;
			if(stabcvtsym(&sym, &s, dir, file, i) < 0)
				continue;
			if((fun = _addsym(fp, &s)) == nil)
				goto err;
			locals = 0;
			params = 0;
			autos = 0;
			break;
		case N_PSYM:
		case N_LSYM:
		case N_LCSYM:
			if(fun){
				if(fun->u.stabs.frameptr == -1){
					/*
					 * Try to distinguish functions with a real frame pointer
				 	 * from functions with a virtual frame pointer, based on
					 * whether the first parameter is in the right location and
					 * whether the autos have negative offsets.
					 *
					 * This heuristic works most of the time.  On the 386, we
					 * cannot distinguish between a v. function with no autos
					 * but a frame of size 4 and a f.p. function with no autos and
					 * no frame.   Anything else we'll get right.
					 *
					 * Another way to go about this would be to have
					 * mach-specific functions to inspect the function
					 * prologues when we're not sure.  What we have
					 * already should be enough, though.
					 */
					if(params==0 && sym.type == N_PSYM){
						if(sym.value != 8 && sym.value >= 4){
							/* XXX 386 specific, but let's find another system before generalizing */
							fun->u.stabs.frameptr = 0;
							fun->u.stabs.framesize = sym.value - 4;
						}
					}else if(sym.type == N_LSYM){
						if((int32)sym.value >= 0){
							fun->u.stabs.frameptr = 0;
							if(params)
								fun->u.stabs.framesize = 8 - 4;
						}else
							fun->u.stabs.frameptr = 1;
					}
				}
				if(sym.type == N_PSYM)
					params++;
				if(sym.type == N_LSYM)
					autos++;
			}
			break;

		case N_STSYM:	/* static file-scope variable */
			/* create new symbol, add it */
			if(stabcvtsym(&sym, &s, dir, file, i) < 0)
				continue;
			if(_addsym(fp, &s) == nil)
				goto err;
			break;
		}
	}
	USED(locals);
	free(inc);
	return 0;

err:
	free(inc);
	return -1;
}

static int
stabspc2file(Fhdr *fhdr, u64int pc, char *buf, uint nbuf, ulong *pline)
{
	int i;
	Symbol *s;
	StabSym ss;
	ulong line, basepc;
	Loc l;

	l.type = LADDR;
	l.addr = pc;
	if((s = ffindsym(fhdr, l, CTEXT)) == nil
	|| stabsym(&fhdr->stabs, s->u.stabs.i, &ss) < 0)
		return -1;

	line = ss.desc;
	basepc = ss.value;
	for(i=s->u.stabs.i+1; stabsym(&fhdr->stabs, i, &ss) >= 0; i++){
		if(ss.type == N_FUN && ss.name == nil)
			break;
		if(ss.type == N_SLINE){
			if(basepc+ss.value > pc)
				break;
			else
				line = ss.desc;
		}
	}
	*pline = line;
	if(s->u.stabs.dir)
		snprint(buf, nbuf, "%s%s", s->u.stabs.dir, s->u.stabs.file);
	else
		snprint(buf, nbuf, "%s", s->u.stabs.file);
	return 0;
}

static int
stabsline2pc(Fhdr *fhdr, u64int startpc, ulong line, u64int *pc)
{
	int i, trigger;
	Symbol *s;
	StabSym ss;
	ulong basepc;
	Loc l;

	l.type = LADDR;
	l.addr = startpc;
	if((s = ffindsym(fhdr, l, CTEXT)) == nil
	|| stabsym(&fhdr->stabs, s->u.stabs.i, &ss) < 0)
		return -1;

	trigger = 0;
	line = ss.desc;
	basepc = ss.value;
	for(i=s->u.stabs.i+1; stabsym(&fhdr->stabs, i, &ss) >= 0; i++){
		if(ss.type == N_FUN)
			basepc = ss.value;
		if(ss.type == N_SLINE){
			if(basepc+ss.value >= startpc)
				trigger = 1;
			if(trigger && ss.desc >= line){
				*pc = basepc+ss.value;
				return 0;
			}
		}
	}
	return -1;
}

static int
stabslenum(Fhdr *fhdr, Symbol *p, char *name, uint j, Loc l, Symbol *s)
{
	int i;
	StabSym ss;

	for(i=p->u.stabs.locals; stabsym(&fhdr->stabs, i, &ss)>=0; i++){
		if(ss.type == N_FUN && ss.name == nil)
			break;
		switch(ss.type){
		case N_PSYM:
		case N_LSYM:
		case N_LCSYM:
			if(name){
				if(strcmpcolon(name, ss.name) != 0)
					break;
			}else if(l.type){
				/* wait for now */
			}else{
				if(j-- > 0)
					break;
			}
			if(stabcvtsym(&ss, s, p->u.stabs.dir, p->u.stabs.file, i) < 0)
				return -1;
			if(s->loc.type == LOFFSET){
				if(p->u.stabs.frameptr == 0)
					s->loc.reg = mach->sp;
				else
					s->loc.reg = mach->fp;
			}
			if(l.type && loccmp(&l, &s->loc) != 0)
				break;
			return 0;
		}
	}
	return -1;
}

static Loc zl;

static int
stabslookuplsym(Fhdr *fhdr, Symbol *p, char *name, Symbol *s)
{
	return stabslenum(fhdr, p, name, 0, zl, s);
}

static int
stabsindexlsym(Fhdr *fhdr, Symbol *p, uint i, Symbol *s)
{
	return stabslenum(fhdr, p, nil, i, zl, s);
}

static int
stabsfindlsym(Fhdr *fhdr, Symbol *p, Loc l, Symbol *s)
{
	return stabslenum(fhdr, p, nil, 0, l, s);
}

int
symstabs(Fhdr *fp)
{
	if(stabssyminit(fp) < 0)
		return -1;
	fp->pc2file = stabspc2file;
	fp->line2pc = stabsline2pc;
	fp->lookuplsym = stabslookuplsym;
	fp->indexlsym = stabsindexlsym;
	fp->findlsym = stabsfindlsym;
	return 0;
}
