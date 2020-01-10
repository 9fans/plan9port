/*
 * Dwarf pc to source line conversion.
 *
 * Maybe should do the reverse here, but what should the interface look like?
 * One possibility is to use the Plan 9 line2addr interface:
 *
 *	long line2addr(ulong line, ulong basepc)
 *
 * which returns the smallest pc > basepc with line number line (ignoring file name).
 *
 * The encoding may be small, but it sure isn't simple!
 */

#include <u.h>
#include <libc.h>
#include <bio.h>
#include "elf.h"
#include "dwarf.h"

#define trace 0

enum
{
	Isstmt = 1<<0,
	BasicDwarfBlock = 1<<1,
	EndSequence = 1<<2,
	PrologueEnd = 1<<3,
	EpilogueBegin = 1<<4
};

typedef struct State State;
struct State
{
	ulong addr;
	ulong file;
	ulong line;
	ulong column;
	ulong flags;
	ulong isa;
};

int
dwarfpctoline(Dwarf *d, ulong pc, char **cdir, char **dir, char **file, ulong *line, ulong *mtime, ulong *length)
{
	uchar *prog, *opcount, *end;
	ulong off, unit, len, vers, x, start;
	int i, first, op, a, l, quantum, isstmt, linebase, linerange, opcodebase, nf;
	char *files, *dirs, *s;
	DwarfBuf b;
	DwarfSym sym;
	State emit, cur, reset;
	uchar **f, **newf;

	f = nil;

	if(dwarfaddrtounit(d, pc, &unit) < 0
	|| dwarflookuptag(d, unit, TagCompileUnit, &sym) < 0)
		return -1;

	if(!sym.attrs.have.stmtlist){
		werrstr("no line mapping information for 0x%lux", pc);
		return -1;
	}
	off = sym.attrs.stmtlist;
	if(off >= d->line.len){
		fprint(2, "bad stmtlist\n");
		goto bad;
	}

	if(trace) fprint(2, "unit 0x%lux stmtlist 0x%lux\n", unit, sym.attrs.stmtlist);

	memset(&b, 0, sizeof b);
	b.d = d;
	b.p = d->line.data + off;
	b.ep = b.p + d->line.len;
	b.addrsize = sym.b.addrsize;	/* should i get this from somewhere else? */

	len = dwarfget4(&b);
	if(b.p==nil || b.p+len > b.ep || b.p+len < b.p){
		fprint(2, "bad len\n");
		goto bad;
	}

	b.ep = b.p+len;
	vers = dwarfget2(&b);
	if(vers != 2){
		werrstr("bad dwarf version 0x%lux", vers);
		return -1;
	}

	len = dwarfget4(&b);
	if(b.p==nil || b.p+len > b.ep || b.p+len < b.p){
		fprint(2, "another bad len\n");
		goto bad;
	}
	prog = b.p+len;

	quantum = dwarfget1(&b);
	isstmt = dwarfget1(&b);
	linebase = (schar)dwarfget1(&b);
	linerange = (schar)dwarfget1(&b);
	opcodebase = dwarfget1(&b);

	opcount = b.p-1;
	dwarfgetnref(&b, opcodebase-1);
	if(b.p == nil){
		fprint(2, "bad opcode chart\n");
		goto bad;
	}

	/* just skip the files and dirs for now; we'll come back */
	dirs = (char*)b.p;
	while(b.p!=nil && *b.p!=0)
		dwarfgetstring(&b);
	dwarfget1(&b);

	files = (char*)b.p;
	while(b.p!=nil && *b.p!=0){
		dwarfgetstring(&b);
		dwarfget128(&b);
		dwarfget128(&b);
		dwarfget128(&b);
	}
	dwarfget1(&b);

	/* move on to the program */
	if(b.p == nil || b.p > prog){
		fprint(2, "bad header\n");
		goto bad;
	}
	b.p = prog;

	reset.addr = 0;
	reset.file = 1;
	reset.line = 1;
	reset.column = 0;
	reset.flags = isstmt ? Isstmt : 0;
	reset.isa = 0;

	cur = reset;
	emit = reset;
	nf = 0;
	start = 0;
	if(trace) fprint(2, "program @ %lud ... %.*H opbase = %d\n", b.p - d->line.data, b.ep-b.p, b.p, opcodebase);
	first = 1;
	while(b.p != nil){
		op = dwarfget1(&b);
		if(trace) fprint(2, "\tline %lud, addr 0x%lux, op %d %.10H", cur.line, cur.addr, op, b.p);
		if(op >= opcodebase){
			a = (op - opcodebase) / linerange;
			l = (op - opcodebase) % linerange + linebase;
			cur.line += l;
			cur.addr += a * quantum;
			if(trace) fprint(2, " +%d,%d\n", a, l);
		emit:
			if(first){
				if(cur.addr > pc){
					werrstr("found wrong line mapping 0x%lux for pc 0x%lux", cur.addr, pc);
					goto out;
				}
				first = 0;
				start = cur.addr;
			}
			if(cur.addr > pc)
				break;
			if(b.p == nil){
				werrstr("buffer underflow in line mapping");
				goto out;
			}
			emit = cur;
			if(emit.flags & EndSequence){
				werrstr("found wrong line mapping 0x%lux-0x%lux for pc 0x%lux", start, cur.addr, pc);
				goto out;
			}
			cur.flags &= ~(BasicDwarfBlock|PrologueEnd|EpilogueBegin);
		}else{
			switch(op){
			case 0:	/* extended op code */
				if(trace) fprint(2, " ext");
				len = dwarfget128(&b);
				end = b.p+len;
				if(b.p == nil || end > b.ep || end < b.p || len < 1)
					goto bad;
				switch(dwarfget1(&b)){
				case 1:	/* end sequence */
					if(trace) fprint(2, " end\n");
					cur.flags |= EndSequence;
					goto emit;
				case 2:	/* set address */
					cur.addr = dwarfgetaddr(&b);
					if(trace) fprint(2, " set pc 0x%lux\n", cur.addr);
					break;
				case 3:	/* define file */
					newf = realloc(f, (nf+1)*sizeof(f[0]));
					if(newf == nil)
						goto out;
					f = newf;
					f[nf++] = b.p;
					s = dwarfgetstring(&b);
					dwarfget128(&b);
					dwarfget128(&b);
					dwarfget128(&b);
					if(trace) fprint(2, " def file %s\n", s);
					break;
				}
				if(b.p == nil || b.p > end)
					goto bad;
				b.p = end;
				break;
			case 1:	/* emit */
				if(trace) fprint(2, " emit\n");
				goto emit;
			case 2:	/* advance pc */
				a = dwarfget128(&b);
				if(trace) fprint(2, " advance pc + %lud\n", a*quantum);
				cur.addr += a * quantum;
				break;
			case 3:	/* advance line */
				l = dwarfget128s(&b);
				if(trace) fprint(2, " advance line + %ld\n", l);
				cur.line += l;
				break;
			case 4:	/* set file */
				if(trace) fprint(2, " set file\n");
				cur.file = dwarfget128s(&b);
				break;
			case 5:	/* set column */
				if(trace) fprint(2, " set column\n");
				cur.column = dwarfget128(&b);
				break;
			case 6:	/* negate stmt */
				if(trace) fprint(2, " negate stmt\n");
				cur.flags ^= Isstmt;
				break;
			case 7:	/* set basic block */
				if(trace) fprint(2, " set basic block\n");
				cur.flags |= BasicDwarfBlock;
				break;
			case 8:	/* const add pc */
				a = (255 - opcodebase) / linerange * quantum;
				if(trace) fprint(2, " const add pc + %d\n", a);
				cur.addr += a;
				break;
			case 9:	/* fixed advance pc */
				a = dwarfget2(&b);
				if(trace) fprint(2, " fixed advance pc + %d\n", a);
				cur.addr += a;
				break;
			case 10:	/* set prologue end */
				if(trace) fprint(2, " set prologue end\n");
				cur.flags |= PrologueEnd;
				break;
			case 11:	/* set epilogue begin */
				if(trace) fprint(2, " set epilogue begin\n");
				cur.flags |= EpilogueBegin;
				break;
			case 12:	/* set isa */
				if(trace) fprint(2, " set isa\n");
				cur.isa = dwarfget128(&b);
				break;
			default:	/* something new - skip it */
				if(trace) fprint(2, " unknown %d\n", opcount[op]);
				for(i=0; i<opcount[op]; i++)
					dwarfget128(&b);
				break;
			}
		}
	}
	if(b.p == nil)
		goto bad;

	/* finally!  the data we seek is in "emit" */

	if(emit.file == 0){
		werrstr("invalid file index in mapping data");
		goto out;
	}
	if(line)
		*line = emit.line;

	/* skip over first emit.file-2 guys */
	b.p = (uchar*)files;
	for(i=emit.file-1; i > 0 && b.p!=nil && *b.p!=0; i--){
		dwarfgetstring(&b);
		dwarfget128(&b);
		dwarfget128(&b);
		dwarfget128(&b);
	}
	if(b.p == nil){
		werrstr("problem parsing file data second time (cannot happen)");
		goto bad;
	}
	if(*b.p == 0){
		if(i >= nf){
			werrstr("bad file index in mapping data");
			goto bad;
		}
		b.p = f[i];
	}
	s = dwarfgetstring(&b);
	if(file)
		*file = s;
	i = dwarfget128(&b);		/* directory */
	x = dwarfget128(&b);
	if(mtime)
		*mtime = x;
	x = dwarfget128(&b);
	if(length)
		*length = x;

	/* fetch dir name */
	if(cdir)
		*cdir = sym.attrs.compdir;

	if(dir){
		if(i == 0)
			*dir = nil;
		else{
			b.p = (uchar*)dirs;
			for(i--; i>0 && b.p!=nil && *b.p!=0; i--)
				dwarfgetstring(&b);
			if(b.p==nil || *b.p==0){
				werrstr("bad directory reference in line mapping");
				goto out;		/* can only happen with bad dir index */
			}
			*dir = dwarfgetstring(&b);
		}
	}

	/* free at last, free at last */
	free(f);
	return 0;

bad:
	werrstr("corrupted line mapping for 0x%lux", pc);
out:
	free(f);
	return -1;
}
