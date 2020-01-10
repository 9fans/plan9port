/*
 * PowerPC definition
 *	forsyth@plan9.cs.york.ac.uk
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include "uregpower.h"
#include <mach.h>

/*
 * PowerPC-specific debugger interface
 *	forsyth@plan9.cs.york.ac.uk
 */

static	char	*powerexcep(Map*, Regs*);
static	int	powerfoll(Map*, Regs*, u64int, u64int*);
static	int	powerdas(Map*, u64int, char, char*, int);
static	int	powerinstlen(Map*, u64int);
static	int	powerhexinst(Map*, u64int, char*, int);

static char *excname[] =
{
	"reserved 0",
	"system reset",
	"machine check",
	"data access",
	"instruction access",
	"external interrupt",
	"alignment",
	"program exception",
	"floating-point unavailable",
	"decrementer",
	"i/o controller interface error",
	"reserved B",
	"system call",
	"trace trap",
	"floating point assist",
	"reserved",
	"ITLB miss",
	"DTLB load miss",
	"DTLB store miss",
	"instruction address breakpoint"
	"SMI interrupt"
	"reserved 15",
	"reserved 16",
	"reserved 17",
	"reserved 18",
	"reserved 19",
	"reserved 1A",
	/* the following are made up on a program exception */
	"floating point exception",		/* FPEXC */
	"illegal instruction",
	"privileged instruction",
	"trap",
	"illegal operation",
};

static char*
powerexcep(Map *map, Regs *regs)
{
	u64int c;
	static char buf[32];

	if(rget(regs, "CAUSE", &c) < 0)
		return "no cause register";
	c >>= 8;
	if(c < nelem(excname))
		return excname[c];
	sprint(buf, "unknown trap #%lux", c);
	return buf;
}

/*
 * disassemble PowerPC opcodes
 */

#define	REGSP	1	/* should come from q.out.h, but there's a clash */
#define	REGSB	2

/*static	char FRAMENAME[] = ".frame"; */

static Map *mymap;

/*
 * ibm conventions for these: bit 0 is top bit
 *	from table 10-1
 */
typedef struct {
	uchar	aa;		/* bit 30 */
	uchar	crba;		/* bits 11-15 */
	uchar	crbb;		/* bits 16-20 */
	long	bd;		/* bits 16-29 */
	uchar	crfd;		/* bits 6-8 */
	uchar	crfs;		/* bits 11-13 */
	uchar	bi;		/* bits 11-15 */
	uchar	bo;		/* bits 6-10 */
	uchar	crbd;		/* bits 6-10 */
	/*union {*/
		short	d;	/* bits 16-31 */
		short	simm;
		ushort	uimm;
	/*};*/
	uchar	fm;		/* bits 7-14 */
	uchar	fra;		/* bits 11-15 */
	uchar	frb;		/* bits 16-20 */
	uchar	frc;		/* bits 21-25 */
	uchar	frs;		/* bits 6-10 */
	uchar	frd;		/* bits 6-10 */
	uchar	crm;		/* bits 12-19 */
	long	li;		/* bits 6-29 || b'00' */
	uchar	lk;		/* bit 31 */
	uchar	mb;		/* bits 21-25 */
	uchar	me;		/* bits 26-30 */
	uchar	nb;		/* bits 16-20 */
	uchar	op;		/* bits 0-5 */
	uchar	oe;		/* bit 21 */
	uchar	ra;		/* bits 11-15 */
	uchar	rb;		/* bits 16-20 */
	uchar	rc;		/* bit 31 */
	/*union {*/
		uchar	rs;	/* bits 6-10 */
		uchar	rd;
	/*};*/
	uchar	sh;		/* bits 16-20 */
	ushort	spr;		/* bits 11-20 */
	uchar	to;		/* bits 6-10 */
	uchar	imm;		/* bits 16-19 */
	ushort	xo;		/* bits 21-30, 22-30, 26-30, or 30 (beware) */
	long	immediate;
	long w0;
	long w1;
	u64int	addr;		/* pc of instruction */
	short	target;
	char	*curr;		/* current fill level in output buffer */
	char	*end;		/* end of buffer */
	int 	size;		/* number of longs in instr */
	char	*err;		/* errmsg */
} Instr;

#define	IBF(v,a,b) (((ulong)(v)>>(32-(b)-1)) & ~(~0UL<<(((b)-(a)+1))))
#define	IB(v,b) IBF((v),(b),(b))

static void
bprint(Instr *i, char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	i->curr = vseprint(i->curr, i->end, fmt, arg);
	va_end(arg);
}

static int
decode(ulong pc, Instr *i)
{
	u32int w;

	if (get4(mymap, pc, &w) < 0) {
		werrstr("can't read instruction: %r");
		return -1;
	}
	i->aa = IB(w, 30);
	i->crba = IBF(w, 11, 15);
	i->crbb = IBF(w, 16, 20);
	i->bd = IBF(w, 16, 29)<<2;
	if(i->bd & 0x8000)
		i->bd |= ~0UL<<16;
	i->crfd = IBF(w, 6, 8);
	i->crfs = IBF(w, 11, 13);
	i->bi = IBF(w, 11, 15);
	i->bo = IBF(w, 6, 10);
	i->crbd = IBF(w, 6, 10);
	i->uimm = IBF(w, 16, 31);	/* also d, simm */
	i->fm = IBF(w, 7, 14);
	i->fra = IBF(w, 11, 15);
	i->frb = IBF(w, 16, 20);
	i->frc = IBF(w, 21, 25);
	i->frs = IBF(w, 6, 10);
	i->frd = IBF(w, 6, 10);
	i->crm = IBF(w, 12, 19);
	i->li = IBF(w, 6, 29)<<2;
	if(IB(w, 6))
		i->li |= ~0UL<<25;
	i->lk = IB(w, 31);
	i->mb = IBF(w, 21, 25);
	i->me = IBF(w, 26, 30);
	i->nb = IBF(w, 16, 20);
	i->op = IBF(w, 0, 5);
	i->oe = IB(w, 21);
	i->ra = IBF(w, 11, 15);
	i->rb = IBF(w, 16, 20);
	i->rc = IB(w, 31);
	i->rs = IBF(w, 6, 10);	/* also rd */
	i->sh = IBF(w, 16, 20);
	i->spr = IBF(w, 11, 20);
	i->to = IBF(w, 6, 10);
	i->imm = IBF(w, 16, 19);
	i->xo = IBF(w, 21, 30);		/* bits 21-30, 22-30, 26-30, or 30 (beware) */
	i->immediate = i->simm;
	if(i->op == 15)
		i->immediate <<= 16;
	i->w0 = w;
	i->target = -1;
	i->addr = pc;
	i->size = 1;
	return 1;
}

static int
mkinstr(ulong pc, Instr *i)
{
	Instr x;

	if(decode(pc, i) < 0)
		return -1;
	/*
	 * combine ADDIS/ORI (CAU/ORIL) into MOVW
	 */
	if (i->op == 15 && i->ra==0) {
		if(decode(pc+4, &x) < 0)
			return -1;
		if (x.op == 24 && x.rs == x.ra && x.ra == i->rd) {
			i->immediate |= (x.immediate & 0xFFFF);
			i->w1 = x.w0;
			i->target = x.rd;
			i->size++;
			return 1;
		}
	}
	return 1;
}

static int
plocal(Instr *i)
{
	Symbol s;
	Loc l, li;

	l.type = LOFFSET;
	l.offset = i->immediate;
	l.reg = "SP";

	li.type = LADDR;
	li.addr = i->addr;
	if (findsym(li, CTEXT, &s)<0 || findlsym(&s, l, &s)<0)
		return -1;
	bprint(i, "%s%+ld(SP)", s.name, (long)i->immediate);
	return 0;
}

static int
pglobal(Instr *i, long off, int anyoff, char *reg)
{
	Symbol s, s2;
	u32int off1;
	Loc l;

	l.type = LADDR;
	l.addr = off;
	if(findsym(l, CANY, &s)>=0 && s.loc.type==LADDR &&
	   s.loc.addr-off < 4096 &&
	   (s.class == CDATA || s.class == CTEXT)) {
		if(off==s.loc.addr && s.name[0]=='$'){
			off1 = 0;
			get4(mymap, s.loc.addr, &off1);
			l.addr = off1;
			if(off1 && findsym(l, CANY, &s2)>=0 && s2.loc.type==LADDR && s2.loc.addr == off1){
				bprint(i, "$%s%s", s2.name, reg);
				return 1;
			}
		}
		bprint(i, "%s", s.name);
		if (s.loc.addr != off)
			bprint(i, "+%lux", off-s.loc.addr);
		bprint(i, reg);
		return 1;
	}
	if(!anyoff)
		return 0;
	bprint(i, "%lux%s", off, reg);
	return 1;
}

static void
address(Instr *i)
{
	if (i->ra == REGSP && plocal(i) >= 0)
		return;
	if (i->ra == REGSB && mach->sb && pglobal(i, mach->sb+i->immediate, 0, "(SB)") >= 0)
		return;
	if(i->simm < 0)
		bprint(i, "-%lx(R%d)", -i->simm, i->ra);
	else
		bprint(i, "%lux(R%d)", i->immediate, i->ra);
}

static	char	*tcrbits[] = {"LT", "GT", "EQ", "VS"};
static	char	*fcrbits[] = {"GE", "LE", "NE", "VC"};

typedef struct Opcode Opcode;

struct Opcode {
	uchar	op;
	ushort	xo;
	ushort	xomask;
	char	*mnemonic;
	void	(*f)(Opcode *, Instr *);
	char	*ken;
	int	flags;
};

static void format(char *, Instr *, char *);

static void
branch(Opcode *o, Instr *i)
{
	char buf[8];
	int bo, bi;

	bo = i->bo & ~1;	/* ignore prediction bit */
	if(bo==4 || bo==12 || bo==20) {	/* simple forms */
		if(bo != 20) {
			bi = i->bi&3;
			sprint(buf, "B%s%%L", bo==12? tcrbits[bi]: fcrbits[bi]);
			format(buf, i, 0);
			bprint(i, "\t");
			if(i->bi > 4)
				bprint(i, "CR(%d),", i->bi/4);
		} else
			format("BR%L\t", i, 0);
		if(i->op == 16)
			format(0, i, "%J");
		else if(i->op == 19 && i->xo == 528)
			format(0, i, "(CTR)");
		else if(i->op == 19 && i->xo == 16)
			format(0, i, "(LR)");
	} else
		format(o->mnemonic, i, o->ken);
}

static void
addi(Opcode *o, Instr *i)
{
	if (i->op==14 && i->ra == 0)
		format("MOVW", i, "%i,R%d");
	else if (i->ra == REGSB) {
		bprint(i, "MOVW\t$");
		address(i);
		bprint(i, ",R%d", i->rd);
	} else if(i->op==14 && i->simm < 0) {
		bprint(i, "SUB\t$%d,R%d", -i->simm, i->ra);
		if(i->rd != i->ra)
			bprint(i, ",R%d", i->rd);
	} else if(i->ra == i->rd) {
		format(o->mnemonic, i, "%i");
		bprint(i, ",R%d", i->rd);
	} else
		format(o->mnemonic, i, o->ken);
}

static void
addis(Opcode *o, Instr *i)
{
	long v;

	v = i->immediate;
	if (i->op==15 && i->ra == 0)
		bprint(i, "MOVW\t$%lux,R%d", v, i->rd);
	else if (i->op==15 && i->ra == REGSB) {
		bprint(i, "MOVW\t$");
		address(i);
		bprint(i, ",R%d", i->rd);
	} else if(i->op==15 && v < 0) {
		bprint(i, "SUB\t$%d,R%d", -v, i->ra);
		if(i->rd != i->ra)
			bprint(i, ",R%d", i->rd);
	} else {
		format(o->mnemonic, i, 0);
		bprint(i, "\t$%ld,R%d", v, i->ra);
		if(i->rd != i->ra)
			bprint(i, ",R%d", i->rd);
	}
}

static void
andi(Opcode *o, Instr *i)
{
	if (i->ra == i->rs)
		format(o->mnemonic, i, "%I,R%d");
	else
		format(o->mnemonic, i, o->ken);
}

static void
gencc(Opcode *o, Instr *i)
{
	format(o->mnemonic, i, o->ken);
}

static void
gen(Opcode *o, Instr *i)
{
	format(o->mnemonic, i, o->ken);
	if (i->rc)
		bprint(i, " [illegal Rc]");
}

static void
ldx(Opcode *o, Instr *i)
{
	if(i->ra == 0)
		format(o->mnemonic, i, "(R%b),R%d");
	else
		format(o->mnemonic, i, "(R%b+R%a),R%d");
	if(i->rc)
		bprint(i, " [illegal Rc]");
}

static void
stx(Opcode *o, Instr *i)
{
	if(i->ra == 0)
		format(o->mnemonic, i, "R%d,(R%b)");
	else
		format(o->mnemonic, i, "R%d,(R%b+R%a)");
	if(i->rc && i->xo != 150)
		bprint(i, " [illegal Rc]");
}

static void
fldx(Opcode *o, Instr *i)
{
	if(i->ra == 0)
		format(o->mnemonic, i, "(R%b),F%d");
	else
		format(o->mnemonic, i, "(R%b+R%a),F%d");
	if(i->rc)
		bprint(i, " [illegal Rc]");
}

static void
fstx(Opcode *o, Instr *i)
{
	if(i->ra == 0)
		format(o->mnemonic, i, "F%d,(R%b)");
	else
		format(o->mnemonic, i, "F%d,(R%b+R%a)");
	if(i->rc)
		bprint(i, " [illegal Rc]");
}

static void
dcb(Opcode *o, Instr *i)
{
	if(i->ra == 0)
		format(o->mnemonic, i, "(R%b)");
	else
		format(o->mnemonic, i, "(R%b+R%a)");
	if(i->rd)
		bprint(i, " [illegal Rd]");
	if(i->rc)
		bprint(i, " [illegal Rc]");
}

static void
lw(Opcode *o, Instr *i, char r)
{
	bprint(i, "%s\t", o->mnemonic);
	address(i);
	bprint(i, ",%c%d", r, i->rd);
}

static void
load(Opcode *o, Instr *i)
{
	lw(o, i, 'R');
}

static void
fload(Opcode *o, Instr *i)
{
	lw(o, i, 'F');
}

static void
sw(Opcode *o, Instr *i, char r)
{
	char *m;
	Symbol s;
	Loc l;

	m = o->mnemonic;
	if (i->rs == REGSP) {
		l.type = LADDR;
		l.addr = i->addr;
		if (findsym(l, CTEXT, &s)>=0) {
			l.type = LOFFSET;
			l.reg = "SP";
			l.offset = i->immediate;
			if (findlsym(&s, l, &s) >= 0) {
				bprint(i, "%s\t%c%d,%s-%d(SP)", m, r, i->rd,
					s.name, i->immediate);
				return;
			}
		}
	}
	if (i->rs == REGSB && mach->sb) {
		bprint(i, "%s\t%c%d,", m, r, i->rd);
		address(i);
		return;
	}
	if (r == 'F')
		format(m, i, "F%d,%l");
	else
		format(m, i, o->ken);
}

static void
store(Opcode *o, Instr *i)
{
	sw(o, i, 'R');
}

static void
fstore(Opcode *o, Instr *i)
{
	sw(o, i, 'F');
}

static void
shifti(Opcode *o, Instr *i)
{
	if (i->ra == i->rs)
		format(o->mnemonic, i, "$%k,R%a");
	else
		format(o->mnemonic, i, o->ken);
}

static void
shift(Opcode *o, Instr *i)
{
	if (i->ra == i->rs)
		format(o->mnemonic, i, "R%b,R%a");
	else
		format(o->mnemonic, i, o->ken);
}

static void
add(Opcode *o, Instr *i)
{
	if (i->rd == i->ra)
		format(o->mnemonic, i, "R%b,R%d");
	else if (i->rd == i->rb)
		format(o->mnemonic, i, "R%a,R%d");
	else
		format(o->mnemonic, i, o->ken);
}

static void
sub(Opcode *o, Instr *i)
{
	format(o->mnemonic, i, 0);
	bprint(i, "\t");
	if(i->op == 31) {
		bprint(i, "\tR%d,R%d", i->ra, i->rb);	/* subtract Ra from Rb */
		if(i->rd != i->rb)
			bprint(i, ",R%d", i->rd);
	} else
		bprint(i, "\tR%d,$%d,R%d", i->ra, i->simm, i->rd);
}

#define div power_div

static void
div(Opcode *o, Instr *i)
{
	format(o->mnemonic, i, 0);
	if(i->op == 31)
		bprint(i, "\tR%d,R%d", i->rb, i->ra);
	else
		bprint(i, "\t$%d,R%d", i->simm, i->ra);
	if(i->ra != i->rd)
		bprint(i, ",R%d", i->rd);
}

static void
and(Opcode *o, Instr *i)
{
	if (i->op == 31) {
		/* Rb,Rs,Ra */
		if (i->ra == i->rs)
			format(o->mnemonic, i, "R%b,R%a");
		else if (i->ra == i->rb)
			format(o->mnemonic, i, "R%s,R%a");
		else
			format(o->mnemonic, i, o->ken);
	} else {
		/* imm,Rs,Ra */
		if (i->ra == i->rs)
			format(o->mnemonic, i, "%I,R%a");
		else
			format(o->mnemonic, i, o->ken);
	}
}

static void
or(Opcode *o, Instr *i)
{
	if (i->op == 31) {
		/* Rb,Rs,Ra */
		if (i->rs == 0 && i->ra == 0 && i->rb == 0)
			format("NOP", i, 0);
		else if (i->rs == i->rb)
			format("MOVW", i, "R%b,R%a");
		else
			and(o, i);
	} else
		and(o, i);
}

static void
shifted(Opcode *o, Instr *i)
{
	format(o->mnemonic, i, 0);
	bprint(i, "\t$%lux,", (ulong)i->uimm<<16);
	if (i->rs == i->ra)
		bprint(i, "R%d", i->ra);
	else
		bprint(i, "R%d,R%d", i->rs, i->ra);
}

static void
neg(Opcode *o, Instr *i)
{
	if (i->rd == i->ra)
		format(o->mnemonic, i, "R%d");
	else
		format(o->mnemonic, i, o->ken);
}

static	char	ir2[] = "R%a,R%d";		/* reverse of IBM order */
static	char	ir3[] = "R%b,R%a,R%d";
static	char	ir3r[] = "R%a,R%b,R%d";
static	char	il3[] = "R%b,R%s,R%a";
static	char	il2u[] = "%I,R%a,R%d";
static	char	il3s[] = "$%k,R%s,R%a";
static	char	il2[] = "R%s,R%a";
static	char	icmp3[] = "R%a,R%b,%D";
static	char	cr3op[] = "%b,%a,%d";
static	char	ir2i[] = "%i,R%a,R%d";
static	char	fp2[] = "F%b,F%d";
static	char	fp3[] = "F%b,F%a,F%d";
static	char	fp3c[] = "F%c,F%a,F%d";
static	char	fp4[] = "F%a,F%c,F%b,F%d";
static	char	fpcmp[] = "F%a,F%b,%D";
static	char	ldop[] = "%l,R%d";
static	char	stop[] = "R%d,%l";
static	char	fldop[] = "%l,F%d";
static	char	fstop[] = "F%d,%l";
static	char	rlim[] = "R%b,R%s,$%z,R%a";
static	char	rlimi[] = "$%k,R%s,$%z,R%a";

#define	OEM	IBF(~0,22,30)
#define	FP4	IBF(~0,26,30)
#define ALL	((ushort)~0)
/*
notes:
	10-26: crfD = rD>>2; rD&3 mbz
		also, L bit (bit 10) mbz or selects 64-bit operands
*/

static Opcode opcodes[] = {
	{31,	360,	OEM,	"ABS%V%C",	0,	ir2},	/* POWER */

	{31,	266,	OEM,	"ADD%V%C",	add,	ir3},
	{31,	 10,	OEM,	"ADDC%V%C",	add,	ir3},
	{31,	138,	OEM,	"ADDE%V%C",	add,	ir3},
	{14,	0,	0,	"ADD",		addi,	ir2i},
	{12,	0,	0,	"ADDC",		addi,	ir2i},
	{13,	0,	0,	"ADDCCC",	addi,	ir2i},
	{15,	0,	0,	"ADD",		addis,	0},
	{31,	234,	OEM,	"ADDME%V%C",	gencc,	ir2},
	{31,	202,	OEM,	"ADDZE%V%C",	gencc,	ir2},

	{31,	28,	ALL,	"AND%C",	and,	il3},
	{31,	60,	ALL,	"ANDN%C",	and,	il3},
	{28,	0,	0,	"ANDCC",		andi,	il2u},
	{29,	0,	0,	"ANDCC",		shifted, 0},

	{18,	0,	0,	"B%L",		gencc,	"%j"},
	{16,	0,	0,	"BC%L",		branch,	"%d,%a,%J"},
	{19,	528,	ALL,	"BC%L",		branch,	"%d,%a,(CTR)"},
	{19,	16,	ALL,	"BC%L",		branch,	"%d,%a,(LR)"},

	{31,	531,	ALL,	"CLCS",		gen,	ir2},	/* POWER */

	{31,	0,	ALL,	"CMP",		0,	icmp3},
	{11,	0,	0,	"CMP",		0,	"R%a,%i,%D"},
	{31,	32,	ALL,	"CMPU",		0,	icmp3},
	{10,	0,	0,	"CMPU",		0,	"R%a,%I,%D"},

	{31,	26,	ALL,	"CNTLZ%C",	gencc,	ir2},

	{19,	257,	ALL,	"CRAND",	gen,	cr3op},
	{19,	129,	ALL,	"CRANDN",	gen,	cr3op},
	{19,	289,	ALL,	"CREQV",	gen,	cr3op},
	{19,	225,	ALL,	"CRNAND",	gen,	cr3op},
	{19,	33,	ALL,	"CRNOR",	gen,	cr3op},
	{19,	449,	ALL,	"CROR",		gen,	cr3op},
	{19,	417,	ALL,	"CRORN",	gen,	cr3op},
	{19,	193,	ALL,	"CRXOR",	gen,	cr3op},

	{31,	86,	ALL,	"DCBF",		dcb,	0},
	{31,	470,	ALL,	"DCBI",		dcb,	0},
	{31,	54,	ALL,	"DCBST",	dcb,	0},
	{31,	278,	ALL,	"DCBT",		dcb,	0},
	{31,	246,	ALL,	"DCBTST",	dcb,	0},
	{31,	1014,	ALL,	"DCBZ",		dcb,	0},

	{31,	331,	OEM,	"DIV%V%C",	div,	ir3},	/* POWER */
	{31,	363,	OEM,	"DIVS%V%C",	div,	ir3},	/* POWER */
	{31,	491,	OEM,	"DIVW%V%C",	div,	ir3},
	{31,	459,	OEM,	"DIVWU%V%C",	div,	ir3},

	{31,	264,	OEM,	"DOZ%V%C",	gencc,	ir3r},	/* POWER */
	{9,	0,	0,	"DOZ",		gen,	ir2i},	/* POWER */

	{31,	310,	ALL,	"ECIWX",	ldx,	0},
	{31,	438,	ALL,	"ECOWX",	stx,	0},
	{31,	854,	ALL,	"EIEIO",	gen,	0},

	{31,	284,	ALL,	"EQV%C",	gencc,	il3},

	{31,	954,	ALL,	"EXTSB%C",	gencc,	il2},
	{31,	922,	ALL,	"EXTSH%C",	gencc,	il2},

	{63,	264,	ALL,	"FABS%C",	gencc,	fp2},
	{63,	21,	ALL,	"FADD%C",	gencc,	fp3},
	{59,	21,	ALL,	"FADDS%C",	gencc,	fp3},
	{63,	32,	ALL,	"FCMPO",	gen,	fpcmp},
	{63,	0,	ALL,	"FCMPU",	gen,	fpcmp},
	{63,	14,	ALL,	"FCTIW%C",	gencc,	fp2},
	{63,	15,	ALL,	"FCTIWZ%C",	gencc,	fp2},
	{63,	18,	ALL,	"FDIV%C",	gencc,	fp3},
	{59,	18,	ALL,	"FDIVS%C",	gencc,	fp3},
	{63,	29,	FP4,	"FMADD%C",	gencc,	fp4},
	{59,	29,	FP4,	"FMADDS%C",	gencc,	fp4},
	{63,	72,	ALL,	"FMOVD%C",	gencc,	fp2},
	{63,	28,	FP4,	"FMSUB%C",	gencc,	fp4},
	{59,	28,	FP4,	"FMSUBS%C",	gencc,	fp4},
	{63,	25,	FP4,	"FMUL%C",	gencc,	fp3c},
	{59,	25,	FP4,	"FMULS%C",	gencc,	fp3c},
	{63,	136,	ALL,	"FNABS%C",	gencc,	fp2},
	{63,	40,	ALL,	"FNEG%C",	gencc,	fp2},
	{63,	31,	FP4,	"FNMADD%C",	gencc,	fp4},
	{59,	31,	FP4,	"FNMADDS%C",	gencc,	fp4},
	{63,	30,	FP4,	"FNMSUB%C",	gencc,	fp4},
	{59,	30,	FP4,	"FNMSUBS%C",	gencc,	fp4},
	{63,	12,	ALL,	"FRSP%C",	gencc,	fp2},
	{63,	20,	FP4,	"FSUB%C",	gencc,	fp3},
	{59,	20,	FP4,	"FSUBS%C",	gencc,	fp3},

	{31,	982,	ALL,	"ICBI",		dcb,	0},
	{19,	150,	ALL,	"ISYNC",	gen,	0},

	{34,	0,	0,	"MOVBZ",	load,	ldop},
	{35,	0,	0,	"MOVBZU",	load,	ldop},
	{31,	119,	ALL,	"MOVBZU",	ldx,	0},
	{31,	87,	ALL,	"MOVBZ",	ldx,	0},
	{50,	0,	0,	"FMOVD",	fload,	fldop},
	{51,	0,	0,	"FMOVDU",	fload,	fldop},
	{31,	631,	ALL,	"FMOVDU",	fldx,	0},
	{31,	599,	ALL,	"FMOVD",	fldx,	0},
	{48,	0,	0,	"FMOVS",	load,	fldop},
	{49,	0,	0,	"FMOVSU",	load,	fldop},
	{31,	567,	ALL,	"FMOVSU",	fldx,	0},
	{31,	535,	ALL,	"FMOVS",	fldx,	0},
	{42,	0,	0,	"MOVH",		load,	ldop},
	{43,	0,	0,	"MOVHU",	load,	ldop},
	{31,	375,	ALL,	"MOVHU",	ldx,	0},
	{31,	343,	ALL,	"MOVH",		ldx,	0},
	{31,	790,	ALL,	"MOVHBR",	ldx,	0},
	{40,	0,	0,	"MOVHZ",	load,	ldop},
	{41,	0,	0,	"MOVHZU",	load,	ldop},
	{31,	311,	ALL,	"MOVHZU",	ldx,	0},
	{31,	279,	ALL,	"MOVHZ",	ldx,	0},
	{46,	0,	0,	"MOVMW",		load,	ldop},
	{31,	277,	ALL,	"LSCBX%C",	ldx,	0},	/* POWER */
	{31,	597,	ALL,	"LSW",		gen,	"(R%a),$%n,R%d"},
	{31,	533,	ALL,	"LSW",		ldx,	0},
	{31,	20,	ALL,	"LWAR",		ldx,	0},
	{31,	534,	ALL,	"MOVWBR",		ldx,	0},
	{32,	0,	0,	"MOVW",		load,	ldop},
	{33,	0,	0,	"MOVWU",	load,	ldop},
	{31,	55,	ALL,	"MOVWU",	ldx,	0},
	{31,	23,	ALL,	"MOVW",		ldx,	0},

	{31,	29,	ALL,	"MASKG%C",	gencc,	"R%s:R%b,R%d"},	/* POWER */
	{31,	541,	ALL,	"MASKIR%C",	gencc,	"R%s,R%b,R%a"},	/* POWER */

	{19,	0,	ALL,	"MOVFL",	gen,	"%S,%D"},
	{63,	64,	ALL,	"MOVCRFS",	gen,	"%S,%D"},
	{31,	512,	ALL,	"MOVW",	gen,	"XER,%D"},
	{31,	19,	ALL,	"MOVW",	gen,	"CR,R%d"},

	{63,	583,	ALL,	"MOVW%C",	gen,	"FPSCR, F%d"},	/* mffs */
	{31,	83,	ALL,	"MOVW",		gen,	"MSR,R%d"},
	{31,	339,	ALL,	"MOVW",		gen,	"%P,R%d"},
	{31,	595,	ALL,	"MOVW",		gen,	"SEG(%a),R%d"},
	{31,	659,	ALL,	"MOVW",		gen,	"SEG(R%b),R%d"},
	{31,	144,	ALL,	"MOVFL",	gen,	"R%s,%m,CR"},
	{63,	70,	ALL,	"MTFSB0%C",	gencc,	"%D"},
	{63,	38,	ALL,	"MTFSB1%C",	gencc,	"%D"},
	{63,	711,	ALL,	"MOVFL%C",	gencc,	"F%b,%M,FPSCR"},	/* mtfsf */
	{63,	134,	ALL,	"MOVFL%C",	gencc,	"%K,%D"},
	{31,	146,	ALL,	"MOVW",		gen,	"R%s,MSR"},
	{31,	467,	ALL,	"MOVW",		gen,	"R%s,%P"},
	{31,	210,	ALL,	"MOVW",		gen,	"R%s,SEG(%a)"},
	{31,	242,	ALL,	"MOVW",		gen,	"R%s,SEG(R%b)"},

	{31,	107,	OEM,	"MUL%V%C",	gencc,	ir3},	/* POWER */
	{31,	75,	ALL,	"MULHW%C",	gencc,	ir3},	/* POWER */
	{31,	11,	ALL,	"MULHWU%C",	gencc,	ir3},	/* POWER */

	{31,	235,	OEM,	"MULLW%V%C",	gencc,	ir3},
	{7,	0,	0,	"MULLW",	div,	"%i,R%a,R%d"},

	{31,	488,	OEM,	"NABS%V%C",	neg,	ir2},	/* POWER */

	{31,	476,	ALL,	"NAND%C",	gencc,	il3},
	{31,	104,	OEM,	"NEG%V%C",	neg,	ir2},
	{31,	124,	ALL,	"NOR%C",	gencc,	il3},
	{31,	444,	ALL,	"OR%C",	or,	il3},
	{31,	412,	ALL,	"ORN%C",	or,	il3},
	{24,	0,	0,	"OR",		and,	"%I,R%d,R%a"},
	{25,	0,	0,	"OR",		shifted, 0},

	{19,	50,	ALL,	"RFI",		gen,	0},

	{22,	0,	0,	"RLMI%C",	gencc,	rlim},	/* POWER */
	{20,	0,	0,	"RLWMI%C",	gencc,	rlimi},
	{21,	0,	0,	"RLWNM%C",	gencc,	rlimi},
	{23,	0,	0,	"RLWNM%C",	gencc,	rlim},

	{31,	537,	ALL,	"RRIB%C",	gencc,	il3},	/* POWER */

	{17,	1,	ALL,	"SYSCALL",	gen,	0},

	{31,	153,	ALL,	"SLE%C",	shift,	il3},	/* POWER */
	{31,	217,	ALL,	"SLEQ%C",	shift,	il3},	/* POWER */
	{31,	184,	ALL,	"SLQ%C",	shifti,	il3s},	/* POWER */
	{31,	248,	ALL,	"SLLQ%C",	shifti,	il3s},	/* POWER */
	{31,	216,	ALL,	"SLLQ%C",	shift,	il3},	/* POWER */
	{31,	152,	ALL,	"SLQ%C",	shift,	il3},	/* POWER */

	{31,	24,	ALL,	"SLW%C",	shift,	il3},

	{31,	920,	ALL,	"SRAQ%C",	shift,	il3},	/* POWER */
	{31,	952,	ALL,	"SRAQ%C",	shifti,	il3s},	/* POWER */

	{31,	792,	ALL,	"SRAW%C",	shift,	il3},
	{31,	824,	ALL,	"SRAW%C",	shifti,	il3s},

	{31,	665,	ALL,	"SRE%C",	shift,	il3},	/* POWER */
	{31,	921,	ALL,	"SREA%C",	shift,	il3},	/* POWER */
	{31,	729,	ALL,	"SREQ%C",	shift,	il3},	/* POWER */
	{31,	696,	ALL,	"SRQ%C",	shifti,	il3s},	/* POWER */
	{31,	760,	ALL,	"SRLQ%C",	shifti,	il3s},	/* POWER */
	{31,	728,	ALL,	"SRLQ%C",	shift,	il3},	/* POWER */
	{31,	664,	ALL,	"SRQ%C",	shift,	il3},	/* POWER */

	{31,	536,	ALL,	"SRW%C",	shift,	il3},

	{38,	0,	0,	"MOVB",		store,	stop},
	{39,	0,	0,	"MOVBU",	store,	stop},
	{31,	247,	ALL,	"MOVBU",	stx,	0},
	{31,	215,	ALL,	"MOVB",		stx,	0},
	{54,	0,	0,	"FMOVD",	fstore,	fstop},
	{55,	0,	0,	"FMOVDU",	fstore,	fstop},
	{31,	759,	ALL,	"FMOVDU",	fstx,	0},
	{31,	727,	ALL,	"FMOVD",	fstx,	0},
	{52,	0,	0,	"FMOVS",	fstore,	fstop},
	{53,	0,	0,	"FMOVSU",	fstore,	fstop},
	{31,	695,	ALL,	"FMOVSU",	fstx,	0},
	{31,	663,	ALL,	"FMOVS",	fstx,	0},
	{44,	0,	0,	"MOVH",		store,	stop},
	{31,	918,	ALL,	"MOVHBR",	stx,	0},
	{45,	0,	0,	"MOVHU",	store,	stop},
	{31,	439,	ALL,	"MOVHU",	stx,	0},
	{31,	407,	ALL,	"MOVH",		stx,	0},
	{47,	0,	0,	"MOVMW",		store,	stop},
	{31,	725,	ALL,	"STSW",		gen,	"R%d,$%n,(R%a)"},
	{31,	661,	ALL,	"STSW",		stx,	0},
	{36,	0,	0,	"MOVW",		store,	stop},
	{31,	662,	ALL,	"MOVWBR",	stx,	0},
	{31,	150,	ALL,	"STWCCC",		stx,	0},
	{37,	0,	0,	"MOVWU",	store,	stop},
	{31,	183,	ALL,	"MOVWU",	stx,	0},
	{31,	151,	ALL,	"MOVW",		stx,	0},

	{31,	40,	OEM,	"SUB%V%C",	sub,	ir3},
	{31,	8,	OEM,	"SUBC%V%C",	sub,	ir3},
	{31,	136,	OEM,	"SUBE%V%C",	sub,	ir3},
	{8,	0,	0,	"SUBC",		gen,	"R%a,%i,R%d"},
	{31,	232,	OEM,	"SUBME%V%C",	sub,	ir2},
	{31,	200,	OEM,	"SUBZE%V%C",	sub,	ir2},

	{31,	598,	ALL,	"SYNC",		gen,	0},
	{31,	370,	ALL,	"TLBIA",	gen,	0},
	{31,	306,	ALL,	"TLBIE",	gen,	"R%b"},
	{31,	1010,	ALL,	"TLBLI",	gen,	"R%b"},
	{31,	978,	ALL,	"TLBLD",	gen,	"R%b"},
	{31,	4,	ALL,	"TW",		gen,	"%d,R%a,R%b"},
	{3,	0,	0,	"TW",		gen,	"%d,R%a,%i"},

	{31,	316,	ALL,	"XOR",		and,	il3},
	{26,	0,	0,	"XOR",		and,	il2u},
	{27,	0,	0,	"XOR",		shifted, 0},

	{0},
};

typedef struct Spr Spr;
struct Spr {
	int	n;
	char	*name;
};

static	Spr	sprname[] = {
	{0, "MQ"},
	{1, "XER"},
	{268, "TBL"},
	{269, "TBU"},
	{8, "LR"},
	{9, "CTR"},
	{528, "IBAT0U"},
	{529, "IBAT0L"},
	{530, "IBAT1U"},
	{531, "IBAT1L"},
	{532, "IBAT2U"},
	{533, "IBAT2L"},
	{534, "IBAT3U"},
	{535, "IBAT3L"},
	{536, "DBAT0U"},
	{537, "DBAT0L"},
	{538, "DBAT1U"},
	{539, "DBAT1L"},
	{540, "DBAT2U"},
	{541, "DBAT2L"},
	{542, "DBAT3U"},
	{543, "DBAT3L"},
	{25, "SDR1"},
	{19, "DAR"},
	{272, "SPRG0"},
	{273, "SPRG1"},
	{274, "SPRG2"},
	{275, "SPRG3"},
	{18, "DSISR"},
	{26, "SRR0"},
	{27, "SRR1"},
	{284, "TBLW"},
	{285, "TBUW"},
	{22, "DEC"},
	{282, "EAR"},
	{1008, "HID0"},
	{1009, "HID1"},
	{976, "DMISS"},
	{977, "DCMP"},
	{978, "HASH1"},
	{979, "HASH2"},
	{980, "IMISS"},
	{981, "ICMP"},
	{982, "RPA"},
	{1010, "IABR"},
	{0,0},
};

static void
format(char *mnemonic, Instr *i, char *f)
{
	int n, s;
	ulong mask;

	if (mnemonic)
		format(0, i, mnemonic);
	if (f == 0)
		return;
	if (mnemonic)
		bprint(i, "\t");
	for ( ; *f; f++) {
		if (*f != '%') {
			bprint(i, "%c", *f);
			continue;
		}
		switch (*++f) {
		case 'V':
			if(i->oe)
				bprint(i, "V");
			break;

		case 'C':
			if(i->rc)
				bprint(i, "CC");
			break;

		case 'a':
			bprint(i, "%d", i->ra);
			break;

		case 'b':
			bprint(i, "%d", i->rb);
			break;

		case 'c':
			bprint(i, "%d", i->frc);
			break;

		case 'd':
		case 's':
			bprint(i, "%d", i->rd);
			break;

		case 'S':
			if(i->ra & 3)
				bprint(i, "CR(INVAL:%d)", i->ra);
			else if(i->op == 63)
				bprint(i, "FPSCR(%d)", i->crfs);
			else
				bprint(i, "CR(%d)", i->crfs);
			break;

		case 'D':
			if(i->rd & 3)
				bprint(i, "CR(INVAL:%d)", i->rd);
			else if(i->op == 63)
				bprint(i, "FPSCR(%d)", i->crfd);
			else
				bprint(i, "CR(%d)", i->crfd);
			break;

		case 'l':
			if(i->simm < 0)
				bprint(i, "-%lx(R%d)", -i->simm, i->ra);
			else
				bprint(i, "%lx(R%d)", i->simm, i->ra);
			break;

		case 'i':
			bprint(i, "$%ld", i->simm);
			break;

		case 'I':
			bprint(i, "$%lx", i->uimm);
			break;

		case 'w':
			bprint(i, "[%lux]", i->w0);
			break;

		case 'P':
			n = ((i->spr&0x1f)<<5)|((i->spr>>5)&0x1f);
			for(s=0; sprname[s].name; s++)
				if(sprname[s].n == n)
					break;
			if(sprname[s].name) {
				if(s < 10)
					bprint(i, sprname[s].name);
				else
					bprint(i, "SPR(%s)", sprname[s].name);
			} else
				bprint(i, "SPR(%d)", n);
			break;

		case 'n':
			bprint(i, "%d", i->nb==0? 32: i->nb);	/* eg, pg 10-103 */
			break;

		case 'm':
			bprint(i, "%lx", i->crm);
			break;

		case 'M':
			bprint(i, "%lx", i->fm);
			break;

		case 'z':
			if(i->mb <= i->me)
				mask = ((ulong)~0L>>i->mb) & (~0L<<(31-i->me));
			else
				mask = ~(((ulong)~0L>>(i->me+1)) & (~0L<<(31-(i->mb-1))));
			bprint(i, "%lux", mask);
			break;

		case 'k':
			bprint(i, "%d", i->sh);
			break;

		case 'K':
			bprint(i, "$%x", i->imm);
			break;

		case 'L':
			if(i->lk)
				bprint(i, "L");
			break;

		case 'j':
			if(i->aa)
				pglobal(i, i->li, 1, "(SB)");
			else
				pglobal(i, i->addr+i->li, 1, "");
			break;

		case 'J':
			if(i->aa)
				pglobal(i, i->bd, 1, "(SB)");
			else
				pglobal(i, i->addr+i->bd, 1, "");
			break;

		case '\0':
			bprint(i, "%%");
			return;

		default:
			bprint(i, "%%%c", *f);
			break;
		}
	}
}

static int
printins(Map *map, ulong pc, char *buf, int n)
{
	Instr i;
	Opcode *o;

	mymap = map;
	memset(&i, 0, sizeof(i));
	i.curr = buf;
	i.end = buf+n-1;
	if(mkinstr(pc, &i) < 0)
		return -1;
	for(o = opcodes; o->mnemonic != 0; o++)
		if(i.op == o->op && (i.xo & o->xomask) == o->xo) {
			if (o->f)
				(*o->f)(o, &i);
			else
				format(o->mnemonic, &i, o->ken);
			return i.size*4;
		}
	bprint(&i, "unknown %lux", i.w0);
	return i.size*4;
}

static int
powerdas(Map *map, u64int pc, char modifier, char *buf, int n)
{
	USED(modifier);
	return printins(map, pc, buf, n);
}

static int
powerhexinst(Map *map, u64int pc, char *buf, int n)
{
	Instr instr;

	mymap = map;
	memset(&instr, 0, sizeof(instr));
	instr.curr = buf;
	instr.end = buf+n-1;
	if (mkinstr(pc, &instr) < 0)
		return -1;
	if (instr.end-instr.curr > 8)
		instr.curr = _hexify(instr.curr, instr.w0, 7);
	if (instr.end-instr.curr > 9 && instr.size == 2) {
		*instr.curr++ = ' ';
		instr.curr = _hexify(instr.curr, instr.w1, 7);
	}
	*instr.curr = 0;
	return instr.size*4;
}

static int
powerinstlen(Map *map, u64int pc)
{
	Instr i;

	mymap = map;
	if (mkinstr(pc, &i) < 0)
		return -1;
	return i.size*4;
}

static int
powerfoll(Map *map, Regs *regs, u64int pc, u64int *foll)
{
	char *reg;
	Instr i;

	mymap = map;
	if (mkinstr(pc, &i) < 0)
		return -1;
	foll[0] = pc+4;
	foll[1] = pc+4;
	switch(i.op) {
	default:
		return 1;

	case 18:	/* branch */
		foll[0] = i.li;
		if(!i.aa)
			foll[0] += pc;
		break;

	case 16:	/* conditional branch */
		foll[0] = i.bd;
		if(!i.aa)
			foll[0] += pc;
		break;

	case 19:	/* conditional branch to register */
		if(i.xo == 528)
			reg = "CTR";
		else if(i.xo == 16)
			reg = "LR";
		else
			return 1;	/* not a branch */
		if(rget(regs, reg, &foll[0]) < 0)
			return -1;
		break;
	}
	if(i.lk)
		return 2;
	return 1;
}

#define	REGOFF(x)	(ulong) (&((struct Ureg *) 0)->x)

#define SP		REGOFF(r1)
#define PC		REGOFF(pc)
#define	R3		REGOFF(r3)	/* return reg */
#define	LR		REGOFF(lr)
#define R31		REGOFF(r31)
#define FP_REG(x)	(R31+4+8*(x))

#define	REGSIZE		sizeof(struct Ureg)
#define	FPREGSIZE	(8*33)

Regdesc powerreglist[] =
{
	{"CAUSE",	REGOFF(cause),	RINT|RRDONLY,	'X'},
	{"SRR1",	REGOFF(srr1),	RINT|RRDONLY,	'X'},
	{"PC",		REGOFF(pc),	RINT,		'X'},
	{"LR",		REGOFF(lr),	RINT,		'X'},
	{"CR",		REGOFF(cr),	RINT,		'X'},
	{"XER",		REGOFF(xer),	RINT,		'X'},
	{"CTR",		REGOFF(ctr),	RINT,		'X'},
	{"PC",		PC,		RINT,		'X'},
	{"SP",		SP,		RINT,		'X'},
	{"R0",		REGOFF(r0),	RINT,		'X'},
	/* R1 is SP */
	{"R2",		REGOFF(r2),	RINT,		'X'},
	{"R3",		REGOFF(r3),	RINT,		'X'},
	{"R4",		REGOFF(r4),	RINT,		'X'},
	{"R5",		REGOFF(r5),	RINT,		'X'},
	{"R6",		REGOFF(r6),	RINT,		'X'},
	{"R7",		REGOFF(r7),	RINT,		'X'},
	{"R8",		REGOFF(r8),	RINT,		'X'},
	{"R9",		REGOFF(r9),	RINT,		'X'},
	{"R10",		REGOFF(r10),	RINT,		'X'},
	{"R11",		REGOFF(r11),	RINT,		'X'},
	{"R12",		REGOFF(r12),	RINT,		'X'},
	{"R13",		REGOFF(r13),	RINT,		'X'},
	{"R14",		REGOFF(r14),	RINT,		'X'},
	{"R15",		REGOFF(r15),	RINT,		'X'},
	{"R16",		REGOFF(r16),	RINT,		'X'},
	{"R17",		REGOFF(r17),	RINT,		'X'},
	{"R18",		REGOFF(r18),	RINT,		'X'},
	{"R19",		REGOFF(r19),	RINT,		'X'},
	{"R20",		REGOFF(r20),	RINT,		'X'},
	{"R21",		REGOFF(r21),	RINT,		'X'},
	{"R22",		REGOFF(r22),	RINT,		'X'},
	{"R23",		REGOFF(r23),	RINT,		'X'},
	{"R24",		REGOFF(r24),	RINT,		'X'},
	{"R25",		REGOFF(r25),	RINT,		'X'},
	{"R26",		REGOFF(r26),	RINT,		'X'},
	{"R27",		REGOFF(r27),	RINT,		'X'},
	{"R28",		REGOFF(r28),	RINT,		'X'},
	{"R29",		REGOFF(r29),	RINT,		'X'},
	{"R30",		REGOFF(r30),	RINT,		'X'},
	{"R31",		REGOFF(r31),	RINT,		'X'},
	{"VRSAVE",	REGOFF(vrsave),	RINT,	'X'},
	{"F0",		FP_REG(0),	RFLT,		'F'},
	{"F1",		FP_REG(1),	RFLT,		'F'},
	{"F2",		FP_REG(2),	RFLT,		'F'},
	{"F3",		FP_REG(3),	RFLT,		'F'},
	{"F4",		FP_REG(4),	RFLT,		'F'},
	{"F5",		FP_REG(5),	RFLT,		'F'},
	{"F6",		FP_REG(6),	RFLT,		'F'},
	{"F7",		FP_REG(7),	RFLT,		'F'},
	{"F8",		FP_REG(8),	RFLT,		'F'},
	{"F9",		FP_REG(9),	RFLT,		'F'},
	{"F10",		FP_REG(10),	RFLT,		'F'},
	{"F11",		FP_REG(11),	RFLT,		'F'},
	{"F12",		FP_REG(12),	RFLT,		'F'},
	{"F13",		FP_REG(13),	RFLT,		'F'},
	{"F14",		FP_REG(14),	RFLT,		'F'},
	{"F15",		FP_REG(15),	RFLT,		'F'},
	{"F16",		FP_REG(16),	RFLT,		'F'},
	{"F17",		FP_REG(17),	RFLT,		'F'},
	{"F18",		FP_REG(18),	RFLT,		'F'},
	{"F19",		FP_REG(19),	RFLT,		'F'},
	{"F20",		FP_REG(20),	RFLT,		'F'},
	{"F21",		FP_REG(21),	RFLT,		'F'},
	{"F22",		FP_REG(22),	RFLT,		'F'},
	{"F23",		FP_REG(23),	RFLT,		'F'},
	{"F24",		FP_REG(24),	RFLT,		'F'},
	{"F25",		FP_REG(25),	RFLT,		'F'},
	{"F26",		FP_REG(26),	RFLT,		'F'},
	{"F27",		FP_REG(27),	RFLT,		'F'},
	{"F28",		FP_REG(28),	RFLT,		'F'},
	{"F29",		FP_REG(29),	RFLT,		'F'},
	{"F30",		FP_REG(30),	RFLT,		'F'},
	{"F31",		FP_REG(31),	RFLT,		'F'},
	{"FPSCR",	FP_REG(32)+4,	RFLT,		'X'},
	{  0 }
};

static char *powerwindregs[] =
{
	"PC",
	"SP",
	"LR",
	0,
};

static int
powerunwind(Map *map, Regs *regs, u64int *next, Symbol *sym)
{
	/*
	 * This is tremendously hard.  The best we're going to
	 * do without better debugger support is trace through
	 * the stack frame links and pull the link registers out of 8(R1).
	 * Anything more requires knowing which registers got saved,
	 * and the compiler appears not to record that.  Gdb appears
	 * to disassemble the function prologues in order to figure
	 * this out.
	 */
	/* evaluate lr */
	/* if in this function, no good - go to saved one. */
	/* set next[sp] to *cur[sp] */
	/* set next[pc] to lr */
	/* set next[lr] to lr */
	/*  */
	werrstr("powerunwind not implemented");
	return -1;
}

	/* the machine description */
Mach machpower =
{
	"power",
	MPOWER,		/* machine type */
	powerreglist,	/* register set */
	REGSIZE,	/* number of bytes in register set */
	FPREGSIZE,	/* number of bytes in FP register set */
	"PC",		/* name of PC */
	"SP",		/* name of SP */
	0,		/* name of FP */
	"LR",		/* name of link register */
	"setSB",	/* static base register name */
	0,		/* value */
	0x1000,		/* page size */
	0x80000000,	/* kernel base */
	0,		/* kernel text mask */
	4,		/* quantization of pc */
	4,		/* szaddr */
	4,		/* szreg */
	4,		/* szfloat */
	8,		/* szdouble */

	powerwindregs,	/* locations unwound in stack trace */
	3,

	{0x02, 0x8F, 0xFF, 0xFF},	/* break point */ /* BUG */
	4,

	powerfoll,		/* following addresses */
	powerexcep,		/* print exception */
	powerunwind,		/* stack unwind */

	beswap2,			/* convert short to local byte order */
	beswap4,			/* convert long to local byte order */
	beswap8,			/* convert vlong to local byte order */
	beieeeftoa32,		/* single precision float pointer */
	beieeeftoa64,		/* double precision float pointer */
	beieeeftoa80,		/* long double precision floating point */

	powerdas,		/* dissembler */
	powerdas,		/* plan9-format disassembler */
	0,			/* commercial disassembler */
	powerhexinst,		/* print instruction */
	powerinstlen,		/* instruction size calculation */
};
