/*
 * 386 definition
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include "ureg386.h"

#define	REGOFF(x)	(ulong)(&((struct Ureg *) 0)->x)

#define	REGSIZE		sizeof(struct Ureg)
#define	FP_CTL(x)		(REGSIZE+4*(x))
#define	FP_REG(x)		(FP_CTL(7)+10*(x))
#define	FPREGSIZE	(6*4+8*10)

/*
 * i386-specific debugger interface
 */

static	char	*i386excep(Map*, Regs*);

/*
static	int	i386trace(Map*, ulong, ulong, ulong, Tracer);
static	ulong	i386frame(Map*, ulong, ulong, ulong, ulong);
*/
static	int	i386foll(Map*, Regs*, ulong, ulong*);
static	int	i386hexinst(Map*, ulong, char*, int);
static	int	i386das(Map*, ulong, char, char*, int);
static	int	i386instlen(Map*, ulong);
static	char	*i386windregs[];
static	int	i386unwind(Map*, Regs*, ulong*);

static	Regdesc i386reglist[] = {
	{"DI",		REGOFF(di),	RINT, 'X'},
	{"SI",		REGOFF(si),	RINT, 'X'},
	{"BP",		REGOFF(bp),	RINT, 'X'},
	{"BX",		REGOFF(bx),	RINT, 'X'},
	{"DX",		REGOFF(dx),	RINT, 'X'},
	{"CX",		REGOFF(cx),	RINT, 'X'},
	{"AX",		REGOFF(ax),	RINT, 'X'},
	{"GS",		REGOFF(gs),	RINT, 'X'},
	{"FS",		REGOFF(fs),	RINT, 'X'},
	{"ES",		REGOFF(es),	RINT, 'X'},
	{"DS",		REGOFF(ds),	RINT, 'X'},
	{"TRAP",	REGOFF(trap), 	RINT, 'X'},
	{"ECODE",	REGOFF(ecode),	RINT, 'X'},
	{"PC",		REGOFF(pc),		RINT, 'X'},
	{"CS",		REGOFF(cs),	RINT, 'X'},
	{"EFLAGS",	REGOFF(flags),	RINT, 'X'},
	{"SP",		REGOFF(sp),		RINT, 'X'},
	{"SS",		REGOFF(ss),	RINT, 'X'},

	{"E0",		FP_CTL(0),	RFLT, 'X'},
	{"E1",		FP_CTL(1),	RFLT, 'X'},
	{"E2",		FP_CTL(2),	RFLT, 'X'},
	{"E3",		FP_CTL(3),	RFLT, 'X'},
	{"E4",		FP_CTL(4),	RFLT, 'X'},
	{"E5",		FP_CTL(5),	RFLT, 'X'},
	{"E6",		FP_CTL(6),	RFLT, 'X'},
	{"F0",		FP_REG(7),	RFLT, '3'},
	{"F1",		FP_REG(6),	RFLT, '3'},
	{"F2",		FP_REG(5),	RFLT, '3'},
	{"F3",		FP_REG(4),	RFLT, '3'},
	{"F4",		FP_REG(3),	RFLT, '3'},
	{"F5",		FP_REG(2),	RFLT, '3'},
	{"F6",		FP_REG(1),	RFLT, '3'},
	{"F7",		FP_REG(0),	RFLT, '3'},
	{  0 }
};

Mach mach386 =
{
	"386",
	M386,		/* machine type */
	i386reglist,	/* register list */
	REGSIZE,	/* size of registers in bytes */
	FPREGSIZE,	/* size of fp registers in bytes */
	"PC",		/* name of PC */
	"SP",		/* name of SP */
	"BP",		/* name of FP */
	0,		/* link register */
	"setSB",	/* static base register name (bogus anyways) */
	0,		/* static base register value */
	0x1000,		/* page size */
	0x80100000,	/* kernel base */
	0,		/* kernel text mask */
	1,		/* quantization of pc */
	4,		/* szaddr */
	4,		/* szreg */
	4,		/* szfloat */
	8,		/* szdouble */

	i386windregs,	/* locations unwound in stack trace */
	9,

	{0xCC, 0, 0, 0},	/* break point: INT 3 */
	1,			/* break point size */

	i386foll,		/* following addresses */
	i386excep,		/* print exception */
	i386unwind,		/* stack unwind */

	leswap2,			/* convert short to local byte order */
	leswap4,			/* convert long to local byte order */
	leswap8,			/* convert vlong to local byte order */
	leieeeftoa32,		/* single precision float pointer */
	leieeeftoa64,		/* double precision float pointer */
	leieeeftoa80,		/* long double precision floating point */

	i386das,		/* dissembler */
	i386das,		/* plan9-format disassembler */
	0,			/* commercial disassembler */
	i386hexinst,		/* print instruction */
	i386instlen,		/* instruction size calculation */
};

static char *i386windregs[] = {
	"PC",
	"SP",
	"BP",
	"AX",
	"CX",
	"DX",
	"BX",
	"SI",
	"DI",
	0,
};

static int
i386unwind(Map *map, Regs *regs, ulong *next)
{
	int isp, ipc, ibp;
	ulong bp;
	u32int v;

	/* No symbol information, use frame pointer and do the best we can. */
	isp = windindex("SP");
	ipc = windindex("PC");
	ibp = windindex("BP");
	if(isp < 0 || ipc < 0 || ibp < 0){
		werrstr("i386unwind: cannot happen");
		return -1;
	}

	bp = next[ibp];

	if(get4(map, bp, &v) < 0)
		return -1;
	next[ibp] = v;

	next[isp] = bp+4;

	if(get4(map, bp+4, &v) < 0)
		return -1;
	next[ipc] = v;

	return 0;	
}

//static	char	STARTSYM[] =	"_main";
//static	char	PROFSYM[] =	"_mainp";
static	char	FRAMENAME[] =	".frame";
static char *excname[] =
{
	"divide error",			/* 0 */
	"debug exception",		/* 1 */
	0,0,				/* 2, 3 */
	"overflow",			/* 4 */
	"bounds check",			/* 5 */
	"invalid opcode",		/* 6 */
	"math coprocessor emulation",	/* 7 */
	"double fault",			/* 8 */
	"math coprocessor overrun",	/* 9 */
	"invalid TSS",			/* 10 */
	"segment not present",		/* 11 */
	"stack exception",		/* 12 */
	"general protection violation",	/* 13 */
	"page fault",			/* 14 */
	0,				/* 15 */
	"math coprocessor error",	/* 16 */
	0,0,0,0,0,0,0,			/* 17-23 */
	"clock",			/* 24 */
	"keyboard",			/* 25 */
	0,				/* 26 */
	"modem status",			/* 27 */
	"serial line status",		/* 28 */
	0,				/* 29 */
	"floppy disk",			/* 30 */
	0,0,0,0,0,			/* 31-35 */
	"mouse",			/* 36 */
	"math coprocessor",		/* 37 */
	"hard disk",			/* 38 */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,/* 39-54 */
	0,0,0,0,0,0,0,0,0,		/* 55-63 */
	"system call",			/* 64 */
};

static char*
i386excep(Map *map, Regs *regs)
{
	ulong c;
	ulong pc;
	static char buf[16];

	if(rget(regs, "TRAP", &c) < 0)
		return "no trap register";

	if(c > 64 || excname[c] == 0) {
		if (c == 3) {
			if (rget(regs, "PC", &pc) >= 0)
			if (get1(map, pc, (uchar*)buf, mach->bpsize) > 0)
			if (memcmp(buf, mach->bpinst, mach->bpsize) == 0)
				return "breakpoint";
		}
		sprint(buf, "exception %ld", c);
		return buf;
	} else
		return excname[c];
}

	/* I386/486 - Disassembler and related functions */

/*
 *  an instruction
 */
typedef struct Instr Instr;
struct	Instr
{
	uchar	mem[1+1+1+1+2+1+1+4+4];		/* raw instruction */
	ulong	addr;		/* address of start of instruction */
	int	n;		/* number of bytes in instruction */
	char	*prefix;	/* instr prefix */
	char	*segment;	/* segment override */
	uchar	jumptype;	/* set to the operand type for jump/ret/call */
	char	osize;		/* 'W' or 'L' */
	char	asize;		/* address size 'W' or 'L' */
	uchar	mod;		/* bits 6-7 of mod r/m field */
	uchar	reg;		/* bits 3-5 of mod r/m field */
	char	ss;		/* bits 6-7 of SIB */
	char	index;		/* bits 3-5 of SIB */
	char	base;		/* bits 0-2 of SIB */
	short	seg;		/* segment of far address */
	ulong	disp;		/* displacement */
	ulong 	imm;		/* immediate */
	ulong 	imm2;		/* second immediate operand */
	char	*curr;		/* fill level in output buffer */
	char	*end;		/* end of output buffer */
	char	*err;		/* error message */
};

	/* 386 register (ha!) set */
enum{
	AX=0,
	CX,
	DX,
	BX,
	SP,
	BP,
	SI,
	DI,
};
	/* Operand Format codes */
/*
%A	-	address size register modifier (!asize -> 'E')
%C	-	Control register CR0/CR1/CR2
%D	-	Debug register DR0/DR1/DR2/DR3/DR6/DR7
%I	-	second immediate operand
%O	-	Operand size register modifier (!osize -> 'E')
%T	-	Test register TR6/TR7
%S	-	size code ('W' or 'L')
%X	-	Weird opcode: OSIZE == 'W' => "CBW"; else => "CWDE"
%d	-	displacement 16-32 bits
%e	-	effective address - Mod R/M value
%f	-	floating point register F0-F7 - from Mod R/M register
%g	-	segment register
%i	-	immediate operand 8-32 bits
%p	-	PC-relative - signed displacement in immediate field
%r	-	Reg from Mod R/M
%x	-	Weird opcode: OSIZE == 'W' => "CWD"; else => "CDQ"
*/

typedef struct Optable Optable;
struct Optable
{
	char	operand[2];
	void	*proto;		/* actually either (char*) or (Optable*) */
};
	/* Operand decoding codes */
enum {
	Ib = 1,			/* 8-bit immediate - (no sign extension)*/
	Ibs,			/* 8-bit immediate (sign extended) */
	Jbs,			/* 8-bit sign-extended immediate in jump or call */
	Iw,			/* 16-bit immediate -> imm */
	Iw2,			/* 16-bit immediate -> imm2 */
	Iwd,			/* Operand-sized immediate (no sign extension)*/
	Awd,			/* Address offset */
	Iwds,			/* Operand-sized immediate (sign extended) */
	RM,			/* Word or long R/M field with register (/r) */
	RMB,			/* Byte R/M field with register (/r) */
	RMOP,			/* Word or long R/M field with op code (/digit) */
	RMOPB,			/* Byte R/M field with op code (/digit) */
	RMR,			/* R/M register only (mod = 11) */
	RMM,			/* R/M memory only (mod = 0/1/2) */
	R0,			/* Base reg of Mod R/M is literal 0x00 */
	R1,			/* Base reg of Mod R/M is literal 0x01 */
	FRMOP,			/* Floating point R/M field with opcode */
	FRMEX,			/* Extended floating point R/M field with opcode */
	JUMP,			/* Jump or Call flag - no operand */
	RET,			/* Return flag - no operand */
	OA,			/* literal 0x0a byte */
	PTR,			/* Seg:Displacement addr (ptr16:16 or ptr16:32) */
	AUX,			/* Multi-byte op code - Auxiliary table */
	PRE,			/* Instr Prefix */
	SEG,			/* Segment Prefix */
	OPOVER,			/* Operand size override */
	ADDOVER,		/* Address size override */
};
	
static Optable optab0F00[8]=
{
	0,0,		"MOVW	LDT,%e",	/* 0x00 */
	0,0,		"MOVW	TR,%e",		/* 0x01 */
	0,0,		"MOVW	%e,LDT",	/* 0x02 */
	0,0,		"MOVW	%e,TR",		/* 0x03 */
	0,0,		"VERR	%e",		/* 0x04 */
	0,0,		"VERW	%e",		/* 0x05 */
};

static Optable optab0F01[8]=
{
	0,0,		"MOVL	GDTR,%e",	/* 0x00 */
	0,0,		"MOVL	IDTR,%e",	/* 0x01 */
	0,0,		"MOVL	%e,GDTR",	/* 0x02 */
	0,0,		"MOVL	%e,IDTR",	/* 0x03 */
	0,0,		"MOVW	MSW,%e",	/* 0x04 */	/* word */
	0,0,		"",			/* 0x05 */
	0,0,		"MOVW	%e,MSW",	/* 0x06 */	/* word */
};

static Optable optab0FBA[8]=
{
	0,0,		"",			/* 0x00 */
	0,0,		"",			/* 0x01 */
	0,0,		"",			/* 0x02 */
	0,0,		"",			/* 0x03 */
	Ib,0,		"BT%S	%i,%e",		/* 0x04 */
	Ib,0,		"BTS%S	%i,%e",		/* 0x05 */
	Ib,0,		"BTR%S	%i,%e",		/* 0x06 */
	Ib,0,		"BTC%S	%i,%e",		/* 0x07 */
};

static Optable optab0F[256]=
{
	RMOP,0,		optab0F00,		/* 0x00 */
	RMOP,0,		optab0F01,		/* 0x01 */
	RM,0,		"LAR	%e,%r",		/* 0x02 */
	RM,0,		"LSL	%e,%r",		/* 0x03 */
	0,0,		"",			/* 0x04 */
	0,0,		"",			/* 0x05 */
	0,0,		"CLTS",			/* 0x06 */
	0,0,		"",			/* 0x07 */
	0,0,		"INVD",			/* 0x08 */
	0,0,		"WBINVD",		/* 0x09 */
	0,0,		"",			/* 0x0a */
	0,0,		"",			/* 0x0b */
	0,0,		"",			/* 0x0c */
	0,0,		"",			/* 0x0d */
	0,0,		"",			/* 0x0e */
	0,0,		"",			/* 0x0f */
	0,0,		"",			/* 0x10 */
	0,0,		"",			/* 0x11 */
	0,0,		"",			/* 0x12 */
	0,0,		"",			/* 0x13 */
	0,0,		"",			/* 0x14 */
	0,0,		"",			/* 0x15 */
	0,0,		"",			/* 0x16 */
	0,0,		"",			/* 0x17 */
	0,0,		"",			/* 0x18 */
	0,0,		"",			/* 0x19 */
	0,0,		"",			/* 0x1a */
	0,0,		"",			/* 0x1b */
	0,0,		"",			/* 0x1c */
	0,0,		"",			/* 0x1d */
	0,0,		"",			/* 0x1e */
	0,0,		"",			/* 0x1f */
	RMR,0,		"MOVL	%C,%e",		/* 0x20 */
	RMR,0,		"MOVL	%D,%e",		/* 0x21 */
	RMR,0,		"MOVL	%e,%C",		/* 0x22 */
	RMR,0,		"MOVL	%e,%D",		/* 0x23 */
	RMR,0,		"MOVL	%T,%e",		/* 0x24 */
	0,0,		"",			/* 0x25 */
	RMR,0,		"MOVL	%e,%T",		/* 0x26 */
	0,0,		"",			/* 0x27 */
	0,0,		"",			/* 0x28 */
	0,0,		"",			/* 0x29 */
	0,0,		"",			/* 0x2a */
	0,0,		"",			/* 0x2b */
	0,0,		"",			/* 0x2c */
	0,0,		"",			/* 0x2d */
	0,0,		"",			/* 0x2e */
	0,0,		"",			/* 0x2f */
	0,0,		"WRMSR",		/* 0x30 */
	0,0,		"RDTSC",		/* 0x31 */
	0,0,		"RDMSR",		/* 0x32 */
	0,0,		"",			/* 0x33 */
	0,0,		"",			/* 0x34 */
	0,0,		"",			/* 0x35 */
	0,0,		"",			/* 0x36 */
	0,0,		"",			/* 0x37 */
	0,0,		"",			/* 0x38 */
	0,0,		"",			/* 0x39 */
	0,0,		"",			/* 0x3a */
	0,0,		"",			/* 0x3b */
	0,0,		"",			/* 0x3c */
	0,0,		"",			/* 0x3d */
	0,0,		"",			/* 0x3e */
	0,0,		"",			/* 0x3f */
	0,0,		"",			/* 0x40 */
	0,0,		"",			/* 0x41 */
	0,0,		"",			/* 0x42 */
	0,0,		"",			/* 0x43 */
	0,0,		"",			/* 0x44 */
	0,0,		"",			/* 0x45 */
	0,0,		"",			/* 0x46 */
	0,0,		"",			/* 0x47 */
	0,0,		"",			/* 0x48 */
	0,0,		"",			/* 0x49 */
	0,0,		"",			/* 0x4a */
	0,0,		"",			/* 0x4b */
	0,0,		"",			/* 0x4c */
	0,0,		"",			/* 0x4d */
	0,0,		"",			/* 0x4e */
	0,0,		"",			/* 0x4f */
	0,0,		"",			/* 0x50 */
	0,0,		"",			/* 0x51 */
	0,0,		"",			/* 0x52 */
	0,0,		"",			/* 0x53 */
	0,0,		"",			/* 0x54 */
	0,0,		"",			/* 0x55 */
	0,0,		"",			/* 0x56 */
	0,0,		"",			/* 0x57 */
	0,0,		"",			/* 0x58 */
	0,0,		"",			/* 0x59 */
	0,0,		"",			/* 0x5a */
	0,0,		"",			/* 0x5b */
	0,0,		"",			/* 0x5c */
	0,0,		"",			/* 0x5d */
	0,0,		"",			/* 0x5e */
	0,0,		"",			/* 0x5f */
	0,0,		"",			/* 0x60 */
	0,0,		"",			/* 0x61 */
	0,0,		"",			/* 0x62 */
	0,0,		"",			/* 0x63 */
	0,0,		"",			/* 0x64 */
	0,0,		"",			/* 0x65 */
	0,0,		"",			/* 0x66 */
	0,0,		"",			/* 0x67 */
	0,0,		"",			/* 0x68 */
	0,0,		"",			/* 0x69 */
	0,0,		"",			/* 0x6a */
	0,0,		"",			/* 0x6b */
	0,0,		"",			/* 0x6c */
	0,0,		"",			/* 0x6d */
	0,0,		"",			/* 0x6e */
	0,0,		"",			/* 0x6f */
	0,0,		"",			/* 0x70 */
	0,0,		"",			/* 0x71 */
	0,0,		"",			/* 0x72 */
	0,0,		"",			/* 0x73 */
	0,0,		"",			/* 0x74 */
	0,0,		"",			/* 0x75 */
	0,0,		"",			/* 0x76 */
	0,0,		"",			/* 0x77 */
	0,0,		"",			/* 0x78 */
	0,0,		"",			/* 0x79 */
	0,0,		"",			/* 0x7a */
	0,0,		"",			/* 0x7b */
	0,0,		"",			/* 0x7c */
	0,0,		"",			/* 0x7d */
	0,0,		"",			/* 0x7e */
	0,0,		"",			/* 0x7f */
	Iwds,0,		"JOS	%p",		/* 0x80 */
	Iwds,0,		"JOC	%p",		/* 0x81 */
	Iwds,0,		"JCS	%p",		/* 0x82 */
	Iwds,0,		"JCC	%p",		/* 0x83 */
	Iwds,0,		"JEQ	%p",		/* 0x84 */
	Iwds,0,		"JNE	%p",		/* 0x85 */
	Iwds,0,		"JLS	%p",		/* 0x86 */
	Iwds,0,		"JHI	%p",		/* 0x87 */
	Iwds,0,		"JMI	%p",		/* 0x88 */
	Iwds,0,		"JPL	%p",		/* 0x89 */
	Iwds,0,		"JPS	%p",		/* 0x8a */
	Iwds,0,		"JPC	%p",		/* 0x8b */
	Iwds,0,		"JLT	%p",		/* 0x8c */
	Iwds,0,		"JGE	%p",		/* 0x8d */
	Iwds,0,		"JLE	%p",		/* 0x8e */
	Iwds,0,		"JGT	%p",		/* 0x8f */
	RMB,0,		"SETOS	%e",		/* 0x90 */
	RMB,0,		"SETOC	%e",		/* 0x91 */
	RMB,0,		"SETCS	%e",		/* 0x92 */
	RMB,0,		"SETCC	%e",		/* 0x93 */
	RMB,0,		"SETEQ	%e",		/* 0x94 */
	RMB,0,		"SETNE	%e",		/* 0x95 */
	RMB,0,		"SETLS	%e",		/* 0x96 */
	RMB,0,		"SETHI	%e",		/* 0x97 */
	RMB,0,		"SETMI	%e",		/* 0x98 */
	RMB,0,		"SETPL	%e",		/* 0x99 */
	RMB,0,		"SETPS	%e",		/* 0x9a */
	RMB,0,		"SETPC	%e",		/* 0x9b */
	RMB,0,		"SETLT	%e",		/* 0x9c */
	RMB,0,		"SETGE	%e",		/* 0x9d */
	RMB,0,		"SETLE	%e",		/* 0x9e */
	RMB,0,		"SETGT	%e",		/* 0x9f */
	0,0,		"PUSHL	FS",		/* 0xa0 */
	0,0,		"POPL	FS",		/* 0xa1 */
	0,0,		"CPUID",		/* 0xa2 */
	RM,0,		"BT%S	%r,%e",		/* 0xa3 */
	RM,Ib,		"SHLD%S	%r,%i,%e",	/* 0xa4 */
	RM,0,		"SHLD%S	%r,CL,%e",	/* 0xa5 */
	0,0,		"",			/* 0xa6 */
	0,0,		"",			/* 0xa7 */
	0,0,		"PUSHL	GS",		/* 0xa8 */
	0,0,		"POPL	GS",		/* 0xa9 */
	0,0,		"",			/* 0xaa */
	RM,0,		"BTS%S	%r,%e",		/* 0xab */
	RM,Ib,		"SHRD%S	%r,%i,%e",	/* 0xac */
	RM,0,		"SHRD%S	%r,CL,%e",	/* 0xad */
	0,0,		"",			/* 0xae */
	RM,0,		"IMUL%S	%e,%r",		/* 0xaf */
	0,0,		"",			/* 0xb0 */
	0,0,		"",			/* 0xb1 */
	RMM,0,		"LSS	%e,%r",		/* 0xb2 */
	RM,0,		"BTR%S	%r,%e",		/* 0xb3 */
	RMM,0,		"LFS	%e,%r",		/* 0xb4 */
	RMM,0,		"LGS	%e,%r",		/* 0xb5 */
	RMB,0,		"MOVBZX	%e,%R",		/* 0xb6 */
	RM,0,		"MOVWZX	%e,%R",		/* 0xb7 */
	0,0,		"",			/* 0xb8 */
	0,0,		"",			/* 0xb9 */
	RMOP,0,		optab0FBA,		/* 0xba */
	RM,0,		"BTC%S	%e,%r",		/* 0xbb */
	RM,0,		"BSF%S	%e,%r",		/* 0xbc */
	RM,0,		"BSR%S	%e,%r",		/* 0xbd */
	RMB,0,		"MOVBSX	%e,%R",		/* 0xbe */
	RM,0,		"MOVWSX	%e,%R",		/* 0xbf */
};

static Optable optab80[8]=
{
	Ib,0,		"ADDB	%i,%e",		/* 0x00 */
	Ib,0,		"ORB	%i,%e",		/* 0x01 */
	Ib,0,		"ADCB	%i,%e",		/* 0x02 */
	Ib,0,		"SBBB	%i,%e",		/* 0x03 */
	Ib,0,		"ANDB	%i,%e",		/* 0x04 */
	Ib,0,		"SUBB	%i,%e",		/* 0x05 */
	Ib,0,		"XORB	%i,%e",		/* 0x06 */
	Ib,0,		"CMPB	%e,%i",		/* 0x07 */
};

static Optable optab81[8]=
{
	Iwd,0,		"ADD%S	%i,%e",		/* 0x00 */
	Iwd,0,		"OR%S	%i,%e",		/* 0x01 */
	Iwd,0,		"ADC%S	%i,%e",		/* 0x02 */
	Iwd,0,		"SBB%S	%i,%e",		/* 0x03 */
	Iwd,0,		"AND%S	%i,%e",		/* 0x04 */
	Iwd,0,		"SUB%S	%i,%e",		/* 0x05 */
	Iwd,0,		"XOR%S	%i,%e",		/* 0x06 */
	Iwd,0,		"CMP%S	%e,%i",		/* 0x07 */
};

static Optable optab83[8]=
{
	Ibs,0,		"ADD%S	%i,%e",		/* 0x00 */
	Ibs,0,		"OR%S	%i,%e",		/* 0x01 */
	Ibs,0,		"ADC%S	%i,%e",		/* 0x02 */
	Ibs,0,		"SBB%S	%i,%e",		/* 0x03 */
	Ibs,0,		"AND%S	%i,%e",		/* 0x04 */
	Ibs,0,		"SUB%S	%i,%e",		/* 0x05 */
	Ibs,0,		"XOR%S	%i,%e",		/* 0x06 */
	Ibs,0,		"CMP%S	%e,%i",		/* 0x07 */
};

static Optable optabC0[8] =
{
	Ib,0,		"ROLB	%i,%e",		/* 0x00 */
	Ib,0,		"RORB	%i,%e",		/* 0x01 */
	Ib,0,		"RCLB	%i,%e",		/* 0x02 */
	Ib,0,		"RCRB	%i,%e",		/* 0x03 */
	Ib,0,		"SHLB	%i,%e",		/* 0x04 */
	Ib,0,		"SHRB	%i,%e",		/* 0x05 */
	0,0,		"",			/* 0x06 */
	Ib,0,		"SARB	%i,%e",		/* 0x07 */
};

static Optable optabC1[8] =
{
	Ib,0,		"ROL%S	%i,%e",		/* 0x00 */
	Ib,0,		"ROR%S	%i,%e",		/* 0x01 */
	Ib,0,		"RCL%S	%i,%e",		/* 0x02 */
	Ib,0,		"RCR%S	%i,%e",		/* 0x03 */
	Ib,0,		"SHL%S	%i,%e",		/* 0x04 */
	Ib,0,		"SHR%S	%i,%e",		/* 0x05 */
	0,0,		"",			/* 0x06 */
	Ib,0,		"SAR%S	%i,%e",		/* 0x07 */
};

static Optable optabD0[8] =
{
	0,0,		"ROLB	%e",		/* 0x00 */
	0,0,		"RORB	%e",		/* 0x01 */
	0,0,		"RCLB	%e",		/* 0x02 */
	0,0,		"RCRB	%e",		/* 0x03 */
	0,0,		"SHLB	%e",		/* 0x04 */
	0,0,		"SHRB	%e",		/* 0x05 */
	0,0,		"",			/* 0x06 */
	0,0,		"SARB	%e",		/* 0x07 */
};

static Optable optabD1[8] =
{
	0,0,		"ROL%S	%e",		/* 0x00 */
	0,0,		"ROR%S	%e",		/* 0x01 */
	0,0,		"RCL%S	%e",		/* 0x02 */
	0,0,		"RCR%S	%e",		/* 0x03 */
	0,0,		"SHL%S	%e",		/* 0x04 */
	0,0,		"SHR%S	%e",		/* 0x05 */
	0,0,		"",			/* 0x06 */
	0,0,		"SAR%S	%e",		/* 0x07 */
};

static Optable optabD2[8] =
{
	0,0,		"ROLB	CL,%e",		/* 0x00 */
	0,0,		"RORB	CL,%e",		/* 0x01 */
	0,0,		"RCLB	CL,%e",		/* 0x02 */
	0,0,		"RCRB	CL,%e",		/* 0x03 */
	0,0,		"SHLB	CL,%e",		/* 0x04 */
	0,0,		"SHRB	CL,%e",		/* 0x05 */
	0,0,		"",			/* 0x06 */
	0,0,		"SARB	CL,%e",		/* 0x07 */
};

static Optable optabD3[8] =
{
	0,0,		"ROL%S	CL,%e",		/* 0x00 */
	0,0,		"ROR%S	CL,%e",		/* 0x01 */
	0,0,		"RCL%S	CL,%e",		/* 0x02 */
	0,0,		"RCR%S	CL,%e",		/* 0x03 */
	0,0,		"SHL%S	CL,%e",		/* 0x04 */
	0,0,		"SHR%S	CL,%e",		/* 0x05 */
	0,0,		"",			/* 0x06 */
	0,0,		"SAR%S	CL,%e",		/* 0x07 */
};

static Optable optabD8[8+8] =
{
	0,0,		"FADDF	%e,F0",		/* 0x00 */
	0,0,		"FMULF	%e,F0",		/* 0x01 */
	0,0,		"FCOMF	%e,F0",		/* 0x02 */
	0,0,		"FCOMFP	%e,F0",		/* 0x03 */
	0,0,		"FSUBF	%e,F0",		/* 0x04 */
	0,0,		"FSUBRF	%e,F0",		/* 0x05 */
	0,0,		"FDIVF	%e,F0",		/* 0x06 */
	0,0,		"FDIVRF	%e,F0",		/* 0x07 */
	0,0,		"FADDD	%f,F0",		/* 0x08 */
	0,0,		"FMULD	%f,F0",		/* 0x09 */
	0,0,		"FCOMD	%f,F0",		/* 0x0a */
	0,0,		"FCOMPD	%f,F0",		/* 0x0b */
	0,0,		"FSUBD	%f,F0",		/* 0x0c */
	0,0,		"FSUBRD	%f,F0",		/* 0x0d */
	0,0,		"FDIVD	%f,F0",		/* 0x0e */
	0,0,		"FDIVRD	%f,F0",		/* 0x0f */
};
/*
 *	optabD9 and optabDB use the following encoding: 
 *	if (0 <= modrm <= 2) instruction = optabDx[modrm&0x07];
 *	else instruction = optabDx[(modrm&0x3f)+8];
 *
 *	the instructions for MOD == 3, follow the 8 instructions
 *	for the other MOD values stored at the front of the table.
 */
static Optable optabD9[64+8] =
{
	0,0,		"FMOVF	%e,F0",		/* 0x00 */
	0,0,		"",			/* 0x01 */
	0,0,		"FMOVF	F0,%e",		/* 0x02 */
	0,0,		"FMOVFP	F0,%e",		/* 0x03 */
	0,0,		"FLDENV%S %e",		/* 0x04 */
	0,0,		"FLDCW	%e",		/* 0x05 */
	0,0,		"FSTENV%S %e",		/* 0x06 */
	0,0,		"FSTCW	%e",		/* 0x07 */
	0,0,		"FMOVD	F0,F0",		/* 0x08 */	/* Mod R/M = 11xx xxxx*/
	0,0,		"FMOVD	F1,F0",		/* 0x09 */
	0,0,		"FMOVD	F2,F0",		/* 0x0a */
	0,0,		"FMOVD	F3,F0",		/* 0x0b */
	0,0,		"FMOVD	F4,F0",		/* 0x0c */
	0,0,		"FMOVD	F5,F0",		/* 0x0d */
	0,0,		"FMOVD	F6,F0",		/* 0x0e */
	0,0,		"FMOVD	F7,F0",		/* 0x0f */
	0,0,		"FXCHD	F0,F0",		/* 0x10 */
	0,0,		"FXCHD	F1,F0",		/* 0x11 */
	0,0,		"FXCHD	F2,F0",		/* 0x12 */
	0,0,		"FXCHD	F3,F0",		/* 0x13 */
	0,0,		"FXCHD	F4,F0",		/* 0x14 */
	0,0,		"FXCHD	F5,F0",		/* 0x15 */
	0,0,		"FXCHD	F6,F0",		/* 0x16 */
	0,0,		"FXCHD	F7,F0",		/* 0x17 */
	0,0,		"FNOP",			/* 0x18 */
	0,0,		"",			/* 0x19 */
	0,0,		"",			/* 0x1a */
	0,0,		"",			/* 0x1b */
	0,0,		"",			/* 0x1c */
	0,0,		"",			/* 0x1d */
	0,0,		"",			/* 0x1e */
	0,0,		"",			/* 0x1f */
	0,0,		"",			/* 0x20 */
	0,0,		"",			/* 0x21 */
	0,0,		"",			/* 0x22 */
	0,0,		"",			/* 0x23 */
	0,0,		"",			/* 0x24 */
	0,0,		"",			/* 0x25 */
	0,0,		"",			/* 0x26 */
	0,0,		"",			/* 0x27 */
	0,0,		"FCHS",			/* 0x28 */
	0,0,		"FABS",			/* 0x29 */
	0,0,		"",			/* 0x2a */
	0,0,		"",			/* 0x2b */
	0,0,		"FTST",			/* 0x2c */
	0,0,		"FXAM",			/* 0x2d */
	0,0,		"",			/* 0x2e */
	0,0,		"",			/* 0x2f */
	0,0,		"FLD1",			/* 0x30 */
	0,0,		"FLDL2T",		/* 0x31 */
	0,0,		"FLDL2E",		/* 0x32 */
	0,0,		"FLDPI",		/* 0x33 */
	0,0,		"FLDLG2",		/* 0x34 */
	0,0,		"FLDLN2",		/* 0x35 */
	0,0,		"FLDZ",			/* 0x36 */
	0,0,		"",			/* 0x37 */
	0,0,		"F2XM1",		/* 0x38 */
	0,0,		"FYL2X",		/* 0x39 */
	0,0,		"FPTAN",		/* 0x3a */
	0,0,		"FPATAN",		/* 0x3b */
	0,0,		"FXTRACT",		/* 0x3c */
	0,0,		"FPREM1",		/* 0x3d */
	0,0,		"FDECSTP",		/* 0x3e */
	0,0,		"FNCSTP",		/* 0x3f */
	0,0,		"FPREM",		/* 0x40 */
	0,0,		"FYL2XP1",		/* 0x41 */
	0,0,		"FSQRT",		/* 0x42 */
	0,0,		"FSINCOS",		/* 0x43 */
	0,0,		"FRNDINT",		/* 0x44 */
	0,0,		"FSCALE",		/* 0x45 */
	0,0,		"FSIN",			/* 0x46 */
	0,0,		"FCOS",			/* 0x47 */
};

static Optable optabDA[8+8] =
{
	0,0,		"FADDL	%e,F0",		/* 0x00 */
	0,0,		"FMULL	%e,F0",		/* 0x01 */
	0,0,		"FCOML	%e,F0",		/* 0x02 */
	0,0,		"FCOMLP	%e,F0",		/* 0x03 */
	0,0,		"FSUBL	%e,F0",		/* 0x04 */
	0,0,		"FSUBRL	%e,F0",		/* 0x05 */
	0,0,		"FDIVL	%e,F0",		/* 0x06 */
	0,0,		"FDIVRL	%e,F0",		/* 0x07 */
	0,0,		"",			/* 0x08 */
	0,0,		"",			/* 0x09 */
	0,0,		"",			/* 0x0a */
	0,0,		"",			/* 0x0b */
	0,0,		"",			/* 0x0c */
	R1,0,		"FUCOMPP",		/* 0x0d */
};

static Optable optabDB[8+64] =
{
	0,0,		"FMOVL	%e,F0",		/* 0x00 */
	0,0,		"",			/* 0x01 */
	0,0,		"FMOVL	F0,%e",		/* 0x02 */
	0,0,		"FMOVLP	F0,%e",		/* 0x03 */
	0,0,		"",			/* 0x04 */
	0,0,		"FMOVX	%e,F0",		/* 0x05 */
	0,0,		"",			/* 0x06 */
	0,0,		"FMOVXP	F0,%e",		/* 0x07 */
	0,0,		"",			/* 0x08 */
	0,0,		"",			/* 0x09 */
	0,0,		"",			/* 0x0a */
	0,0,		"",			/* 0x0b */
	0,0,		"",			/* 0x0c */
	0,0,		"",			/* 0x0d */
	0,0,		"",			/* 0x0e */
	0,0,		"",			/* 0x0f */
	0,0,		"",			/* 0x10 */
	0,0,		"",			/* 0x11 */
	0,0,		"",			/* 0x12 */
	0,0,		"",			/* 0x13 */
	0,0,		"",			/* 0x14 */
	0,0,		"",			/* 0x15 */
	0,0,		"",			/* 0x16 */
	0,0,		"",			/* 0x17 */
	0,0,		"",			/* 0x18 */
	0,0,		"",			/* 0x19 */
	0,0,		"",			/* 0x1a */
	0,0,		"",			/* 0x1b */
	0,0,		"",			/* 0x1c */
	0,0,		"",			/* 0x1d */
	0,0,		"",			/* 0x1e */
	0,0,		"",			/* 0x1f */
	0,0,		"",			/* 0x20 */
	0,0,		"",			/* 0x21 */
	0,0,		"",			/* 0x22 */
	0,0,		"",			/* 0x23 */
	0,0,		"",			/* 0x24 */
	0,0,		"",			/* 0x25 */
	0,0,		"",			/* 0x26 */
	0,0,		"",			/* 0x27 */
	0,0,		"",			/* 0x28 */
	0,0,		"",			/* 0x29 */
	0,0,		"FCLEX",		/* 0x2a */
	0,0,		"FINIT",		/* 0x2b */
};

static Optable optabDC[8+8] =
{
	0,0,		"FADDD	%e,F0",		/* 0x00 */
	0,0,		"FMULD	%e,F0",		/* 0x01 */
	0,0,		"FCOMD	%e,F0",		/* 0x02 */
	0,0,		"FCOMDP	%e,F0",		/* 0x03 */
	0,0,		"FSUBD	%e,F0",		/* 0x04 */
	0,0,		"FSUBRD	%e,F0",		/* 0x05 */
	0,0,		"FDIVD	%e,F0",		/* 0x06 */
	0,0,		"FDIVRD	%e,F0",		/* 0x07 */
	0,0,		"FADDD	F0,%f",		/* 0x08 */
	0,0,		"FMULD	F0,%f",		/* 0x09 */
	0,0,		"",			/* 0x0a */
	0,0,		"",			/* 0x0b */
	0,0,		"FSUBRD	F0,%f",		/* 0x0c */
	0,0,		"FSUBD	F0,%f",		/* 0x0d */
	0,0,		"FDIVRD	F0,%f",		/* 0x0e */
	0,0,		"FDIVD	F0,%f",		/* 0x0f */
};

static Optable optabDD[8+8] =
{
	0,0,		"FMOVD	%e,F0",		/* 0x00 */
	0,0,		"",			/* 0x01 */
	0,0,		"FMOVD	F0,%e",		/* 0x02 */
	0,0,		"FMOVDP	F0,%e",		/* 0x03 */
	0,0,		"FRSTOR%S %e",		/* 0x04 */
	0,0,		"",			/* 0x05 */
	0,0,		"FSAVE%S %e",		/* 0x06 */
	0,0,		"FSTSW	%e",		/* 0x07 */
	0,0,		"FFREED	%f",		/* 0x08 */
	0,0,		"",			/* 0x09 */
	0,0,		"FMOVD	%f,F0",		/* 0x0a */
	0,0,		"FMOVDP	%f,F0",		/* 0x0b */
	0,0,		"FUCOMD	%f,F0",		/* 0x0c */
	0,0,		"FUCOMDP %f,F0",	/* 0x0d */
};

static Optable optabDE[8+8] =
{
	0,0,		"FADDW	%e,F0",		/* 0x00 */
	0,0,		"FMULW	%e,F0",		/* 0x01 */
	0,0,		"FCOMW	%e,F0",		/* 0x02 */
	0,0,		"FCOMWP	%e,F0",		/* 0x03 */
	0,0,		"FSUBW	%e,F0",		/* 0x04 */
	0,0,		"FSUBRW	%e,F0",		/* 0x05 */
	0,0,		"FDIVW	%e,F0",		/* 0x06 */
	0,0,		"FDIVRW	%e,F0",		/* 0x07 */
	0,0,		"FADDDP	F0,%f",		/* 0x08 */
	0,0,		"FMULDP	F0,%f",		/* 0x09 */
	0,0,		"",			/* 0x0a */
	R1,0,		"FCOMPDP",		/* 0x0b */
	0,0,		"FSUBRDP F0,%f",	/* 0x0c */
	0,0,		"FSUBDP	F0,%f",		/* 0x0d */
	0,0,		"FDIVRDP F0,%f",	/* 0x0e */
	0,0,		"FDIVDP	F0,%f",		/* 0x0f */
};

static Optable optabDF[8+8] =
{
	0,0,		"FMOVW	%e,F0",		/* 0x00 */
	0,0,		"",			/* 0x01 */
	0,0,		"FMOVW	F0,%e",		/* 0x02 */
	0,0,		"FMOVWP	F0,%e",		/* 0x03 */
	0,0,		"FBLD	%e",		/* 0x04 */
	0,0,		"FMOVL	%e,F0",		/* 0x05 */
	0,0,		"FBSTP	%e",		/* 0x06 */
	0,0,		"FMOVLP	F0,%e",		/* 0x07 */
	0,0,		"",			/* 0x08 */
	0,0,		"",			/* 0x09 */
	0,0,		"",			/* 0x0a */
	0,0,		"",			/* 0x0b */
	R0,0,		"FSTSW	%OAX",		/* 0x0c */
};

static Optable optabF6[8] =
{
	Ib,0,		"TESTB	%i,%e",		/* 0x00 */
	0,0,		"",			/* 0x01 */
	0,0,		"NOTB	%e",		/* 0x02 */
	0,0,		"NEGB	%e",		/* 0x03 */
	0,0,		"MULB	AL,%e",		/* 0x04 */
	0,0,		"IMULB	AL,%e",		/* 0x05 */
	0,0,		"DIVB	AL,%e",		/* 0x06 */
	0,0,		"IDIVB	AL,%e",		/* 0x07 */
};

static Optable optabF7[8] =
{
	Iwd,0,		"TEST%S	%i,%e",		/* 0x00 */
	0,0,		"",			/* 0x01 */
	0,0,		"NOT%S	%e",		/* 0x02 */
	0,0,		"NEG%S	%e",		/* 0x03 */
	0,0,		"MUL%S	%OAX,%e",	/* 0x04 */
	0,0,		"IMUL%S	%OAX,%e",	/* 0x05 */
	0,0,		"DIV%S	%OAX,%e",	/* 0x06 */
	0,0,		"IDIV%S	%OAX,%e",	/* 0x07 */
};

static Optable optabFE[8] =
{
	0,0,		"INCB	%e",		/* 0x00 */
	0,0,		"DECB	%e",		/* 0x01 */
};

static Optable optabFF[8] =
{
	0,0,		"INC%S	%e",		/* 0x00 */
	0,0,		"DEC%S	%e",		/* 0x01 */
	JUMP,0,		"CALL*%S %e",		/* 0x02 */
	JUMP,0,		"CALLF*%S %e",		/* 0x03 */
	JUMP,0,		"JMP*%S	%e",		/* 0x04 */
	JUMP,0,		"JMPF*%S %e",		/* 0x05 */
	0,0,		"PUSHL	%e",		/* 0x06 */
};

static Optable optable[256] =
{
	RMB,0,		"ADDB	%r,%e",		/* 0x00 */
	RM,0,		"ADD%S	%r,%e",		/* 0x01 */
	RMB,0,		"ADDB	%e,%r",		/* 0x02 */
	RM,0,		"ADD%S	%e,%r",		/* 0x03 */
	Ib,0,		"ADDB	%i,AL",		/* 0x04 */
	Iwd,0,		"ADD%S	%i,%OAX",	/* 0x05 */
	0,0,		"PUSHL	ES",		/* 0x06 */
	0,0,		"POPL	ES",		/* 0x07 */
	RMB,0,		"ORB	%r,%e",		/* 0x08 */
	RM,0,		"OR%S	%r,%e",		/* 0x09 */
	RMB,0,		"ORB	%e,%r",		/* 0x0a */
	RM,0,		"OR%S	%e,%r",		/* 0x0b */
	Ib,0,		"ORB	%i,AL",		/* 0x0c */
	Iwd,0,		"OR%S	%i,%OAX",	/* 0x0d */
	0,0,		"PUSHL	CS",		/* 0x0e */
	AUX,0,		optab0F,		/* 0x0f */
	RMB,0,		"ADCB	%r,%e",		/* 0x10 */
	RM,0,		"ADC%S	%r,%e",		/* 0x11 */
	RMB,0,		"ADCB	%e,%r",		/* 0x12 */
	RM,0,		"ADC%S	%e,%r",		/* 0x13 */
	Ib,0,		"ADCB	%i,AL",		/* 0x14 */
	Iwd,0,		"ADC%S	%i,%OAX",	/* 0x15 */
	0,0,		"PUSHL	SS",		/* 0x16 */
	0,0,		"POPL	SS",		/* 0x17 */
	RMB,0,		"SBBB	%r,%e",		/* 0x18 */
	RM,0,		"SBB%S	%r,%e",		/* 0x19 */
	RMB,0,		"SBBB	%e,%r",		/* 0x1a */
	RM,0,		"SBB%S	%e,%r",		/* 0x1b */
	Ib,0,		"SBBB	%i,AL",		/* 0x1c */
	Iwd,0,		"SBB%S	%i,%OAX",	/* 0x1d */
	0,0,		"PUSHL	DS",		/* 0x1e */
	0,0,		"POPL	DS",		/* 0x1f */
	RMB,0,		"ANDB	%r,%e",		/* 0x20 */
	RM,0,		"AND%S	%r,%e",		/* 0x21 */
	RMB,0,		"ANDB	%e,%r",		/* 0x22 */
	RM,0,		"AND%S	%e,%r",		/* 0x23 */
	Ib,0,		"ANDB	%i,AL",		/* 0x24 */
	Iwd,0,		"AND%S	%i,%OAX",	/* 0x25 */
	SEG,0,		"ES:",			/* 0x26 */
	0,0,		"DAA",			/* 0x27 */
	RMB,0,		"SUBB	%r,%e",		/* 0x28 */
	RM,0,		"SUB%S	%r,%e",		/* 0x29 */
	RMB,0,		"SUBB	%e,%r",		/* 0x2a */
	RM,0,		"SUB%S	%e,%r",		/* 0x2b */
	Ib,0,		"SUBB	%i,AL",		/* 0x2c */
	Iwd,0,		"SUB%S	%i,%OAX",	/* 0x2d */
	SEG,0,		"CS:",			/* 0x2e */
	0,0,		"DAS",			/* 0x2f */
	RMB,0,		"XORB	%r,%e",		/* 0x30 */
	RM,0,		"XOR%S	%r,%e",		/* 0x31 */
	RMB,0,		"XORB	%e,%r",		/* 0x32 */
	RM,0,		"XOR%S	%e,%r",		/* 0x33 */
	Ib,0,		"XORB	%i,AL",		/* 0x34 */
	Iwd,0,		"XOR%S	%i,%OAX",	/* 0x35 */
	SEG,0,		"SS:",			/* 0x36 */
	0,0,		"AAA",			/* 0x37 */
	RMB,0,		"CMPB	%r,%e",		/* 0x38 */
	RM,0,		"CMP%S	%r,%e",		/* 0x39 */
	RMB,0,		"CMPB	%e,%r",		/* 0x3a */
	RM,0,		"CMP%S	%e,%r",		/* 0x3b */
	Ib,0,		"CMPB	%i,AL",		/* 0x3c */
	Iwd,0,		"CMP%S	%i,%OAX",	/* 0x3d */
	SEG,0,		"DS:",			/* 0x3e */
	0,0,		"AAS",			/* 0x3f */
	0,0,		"INC%S	%OAX",		/* 0x40 */
	0,0,		"INC%S	%OCX",		/* 0x41 */
	0,0,		"INC%S	%ODX",		/* 0x42 */
	0,0,		"INC%S	%OBX",		/* 0x43 */
	0,0,		"INC%S	%OSP",		/* 0x44 */
	0,0,		"INC%S	%OBP",		/* 0x45 */
	0,0,		"INC%S	%OSI",		/* 0x46 */
	0,0,		"INC%S	%ODI",		/* 0x47 */
	0,0,		"DEC%S	%OAX",		/* 0x48 */
	0,0,		"DEC%S	%OCX",		/* 0x49 */
	0,0,		"DEC%S	%ODX",		/* 0x4a */
	0,0,		"DEC%S	%OBX",		/* 0x4b */
	0,0,		"DEC%S	%OSP",		/* 0x4c */
	0,0,		"DEC%S	%OBP",		/* 0x4d */
	0,0,		"DEC%S	%OSI",		/* 0x4e */
	0,0,		"DEC%S	%ODI",		/* 0x4f */
	0,0,		"PUSH%S	%OAX",		/* 0x50 */
	0,0,		"PUSH%S	%OCX",		/* 0x51 */
	0,0,		"PUSH%S	%ODX",		/* 0x52 */
	0,0,		"PUSH%S	%OBX",		/* 0x53 */
	0,0,		"PUSH%S	%OSP",		/* 0x54 */
	0,0,		"PUSH%S	%OBP",		/* 0x55 */
	0,0,		"PUSH%S	%OSI",		/* 0x56 */
	0,0,		"PUSH%S	%ODI",		/* 0x57 */
	0,0,		"POP%S	%OAX",		/* 0x58 */
	0,0,		"POP%S	%OCX",		/* 0x59 */
	0,0,		"POP%S	%ODX",		/* 0x5a */
	0,0,		"POP%S	%OBX",		/* 0x5b */
	0,0,		"POP%S	%OSP",		/* 0x5c */
	0,0,		"POP%S	%OBP",		/* 0x5d */
	0,0,		"POP%S	%OSI",		/* 0x5e */
	0,0,		"POP%S	%ODI",		/* 0x5f */
	0,0,		"PUSHA%S",		/* 0x60 */
	0,0,		"POPA%S",		/* 0x61 */
	RMM,0,		"BOUND	%e,%r",		/* 0x62 */
	RM,0,		"ARPL	%r,%e",		/* 0x63 */
	SEG,0,		"FS:",			/* 0x64 */
	SEG,0,		"GS:",			/* 0x65 */
	OPOVER,0,	"",			/* 0x66 */
	ADDOVER,0,	"",			/* 0x67 */
	Iwd,0,		"PUSH%S	%i",		/* 0x68 */
	RM,Iwd,		"IMUL%S	%e,%i,%r",	/* 0x69 */
	Ib,0,		"PUSH%S	%i",		/* 0x6a */
	RM,Ibs,		"IMUL%S	%e,%i,%r",	/* 0x6b */
	0,0,		"INSB	DX,(%ODI)",	/* 0x6c */
	0,0,		"INS%S	DX,(%ODI)",	/* 0x6d */
	0,0,		"OUTSB	(%ASI),DX",	/* 0x6e */
	0,0,		"OUTS%S	(%ASI),DX",	/* 0x6f */
	Jbs,0,		"JOS	%p",		/* 0x70 */
	Jbs,0,		"JOC	%p",		/* 0x71 */
	Jbs,0,		"JCS	%p",		/* 0x72 */
	Jbs,0,		"JCC	%p",		/* 0x73 */
	Jbs,0,		"JEQ	%p",		/* 0x74 */
	Jbs,0,		"JNE	%p",		/* 0x75 */
	Jbs,0,		"JLS	%p",		/* 0x76 */
	Jbs,0,		"JHI	%p",		/* 0x77 */
	Jbs,0,		"JMI	%p",		/* 0x78 */
	Jbs,0,		"JPL	%p",		/* 0x79 */
	Jbs,0,		"JPS	%p",		/* 0x7a */
	Jbs,0,		"JPC	%p",		/* 0x7b */
	Jbs,0,		"JLT	%p",		/* 0x7c */
	Jbs,0,		"JGE	%p",		/* 0x7d */
	Jbs,0,		"JLE	%p",		/* 0x7e */
	Jbs,0,		"JGT	%p",		/* 0x7f */
	RMOPB,0,	optab80,		/* 0x80 */
	RMOP,0,		optab81,		/* 0x81 */
	0,0,		"",			/* 0x82 */
	RMOP,0,		optab83,		/* 0x83 */
	RMB,0,		"TESTB	%r,%e",		/* 0x84 */
	RM,0,		"TEST%S	%r,%e",		/* 0x85 */
	RMB,0,		"XCHGB	%r,%e",		/* 0x86 */
	RM,0,		"XCHG%S	%r,%e",		/* 0x87 */
	RMB,0,		"MOVB	%r,%e",		/* 0x88 */
	RM,0,		"MOV%S	%r,%e",		/* 0x89 */
	RMB,0,		"MOVB	%e,%r",		/* 0x8a */
	RM,0,		"MOV%S	%e,%r",		/* 0x8b */
	RM,0,		"MOVW	%g,%e",		/* 0x8c */
	RM,0,		"LEA	%e,%r",		/* 0x8d */
	RM,0,		"MOVW	%e,%g",		/* 0x8e */
	RM,0,		"POP%S	%e",		/* 0x8f */
	0,0,		"NOP",			/* 0x90 */
	0,0,		"XCHG	%OCX,%OAX",	/* 0x91 */
	0,0,		"XCHG	%OCX,%OAX",	/* 0x92 */
	0,0,		"XCHG	%OCX,%OAX",	/* 0x93 */
	0,0,		"XCHG	%OSP,%OAX",	/* 0x94 */
	0,0,		"XCHG	%OBP,%OAX",	/* 0x95 */
	0,0,		"XCHG	%OSI,%OAX",	/* 0x96 */
	0,0,		"XCHG	%ODI,%OAX",	/* 0x97 */
	0,0,		"%X",			/* 0x98 */	/* miserable CBW or CWDE */
	0,0,		"%x",			/* 0x99 */	/* idiotic CWD or CDQ */
	PTR,0,		"CALL%S	%d",		/* 0x9a */
	0,0,		"WAIT",			/* 0x9b */
	0,0,		"PUSHF",		/* 0x9c */
	0,0,		"POPF",			/* 0x9d */
	0,0,		"SAHF",			/* 0x9e */
	0,0,		"LAHF",			/* 0x9f */
	Awd,0,		"MOVB	%i,AL",		/* 0xa0 */
	Awd,0,		"MOV%S	%i,%OAX",	/* 0xa1 */
	Awd,0,		"MOVB	AL,%i",		/* 0xa2 */
	Awd,0,		"MOV%S	%OAX,%i",	/* 0xa3 */
	0,0,		"MOVSB	(%ASI),(%ADI)",	/* 0xa4 */
	0,0,		"MOVS%S	(%ASI),(%ADI)",	/* 0xa5 */
	0,0,		"CMPSB	(%ASI),(%ADI)",	/* 0xa6 */
	0,0,		"CMPS%S	(%ASI),(%ADI)",	/* 0xa7 */
	Ib,0,		"TESTB	%i,AL",		/* 0xa8 */
	Iwd,0,		"TEST%S	%i,%OAX",	/* 0xa9 */
	0,0,		"STOSB	AL,(%ADI)",	/* 0xaa */
	0,0,		"STOS%S	%OAX,(%ADI)",	/* 0xab */
	0,0,		"LODSB	(%ASI),AL",	/* 0xac */
	0,0,		"LODS%S	(%ASI),%OAX",	/* 0xad */
	0,0,		"SCASB	(%ADI),AL",	/* 0xae */
	0,0,		"SCAS%S	(%ADI),%OAX",	/* 0xaf */
	Ib,0,		"MOVB	%i,AL",		/* 0xb0 */
	Ib,0,		"MOVB	%i,CL",		/* 0xb1 */
	Ib,0,		"MOVB	%i,DL",		/* 0xb2 */
	Ib,0,		"MOVB	%i,BL",		/* 0xb3 */
	Ib,0,		"MOVB	%i,AH",		/* 0xb4 */
	Ib,0,		"MOVB	%i,CH",		/* 0xb5 */
	Ib,0,		"MOVB	%i,DH",		/* 0xb6 */
	Ib,0,		"MOVB	%i,BH",		/* 0xb7 */
	Iwd,0,		"MOV%S	%i,%OAX",	/* 0xb8 */
	Iwd,0,		"MOV%S	%i,%OCX",	/* 0xb9 */
	Iwd,0,		"MOV%S	%i,%ODX",	/* 0xba */
	Iwd,0,		"MOV%S	%i,%OBX",	/* 0xbb */
	Iwd,0,		"MOV%S	%i,%OSP",	/* 0xbc */
	Iwd,0,		"MOV%S	%i,%OBP",	/* 0xbd */
	Iwd,0,		"MOV%S	%i,%OSI",	/* 0xbe */
	Iwd,0,		"MOV%S	%i,%ODI",	/* 0xbf */
	RMOPB,0,	optabC0,		/* 0xc0 */
	RMOP,0,		optabC1,		/* 0xc1 */
	Iw,0,		"RET	%i",		/* 0xc2 */
	RET,0,		"RET",			/* 0xc3 */
	RM,0,		"LES	%e,%r",		/* 0xc4 */
	RM,0,		"LDS	%e,%r",		/* 0xc5 */
	RMB,Ib,		"MOVB	%i,%e",		/* 0xc6 */
	RM,Iwd,		"MOV%S	%i,%e",		/* 0xc7 */
	Iw2,Ib,		"ENTER	%i,%I",		/* 0xc8 */	/* loony ENTER */
	RET,0,		"LEAVE",		/* 0xc9 */	/* bizarre LEAVE */
	Iw,0,		"RETF	%i",		/* 0xca */
	RET,0,		"RETF",			/* 0xcb */
	0,0,		"INT	3",		/* 0xcc */
	Ib,0,		"INTB	%i",		/* 0xcd */
	0,0,		"INTO",			/* 0xce */
	0,0,		"IRET",			/* 0xcf */
	RMOPB,0,	optabD0,		/* 0xd0 */
	RMOP,0,		optabD1,		/* 0xd1 */
	RMOPB,0,	optabD2,		/* 0xd2 */
	RMOP,0,		optabD3,		/* 0xd3 */
	OA,0,		"AAM",			/* 0xd4 */
	OA,0,		"AAD",			/* 0xd5 */
	0,0,		"",			/* 0xd6 */
	0,0,		"XLAT",			/* 0xd7 */
	FRMOP,0,	optabD8,		/* 0xd8 */
	FRMEX,0,	optabD9,		/* 0xd9 */
	FRMOP,0,	optabDA,		/* 0xda */
	FRMEX,0,	optabDB,		/* 0xdb */
	FRMOP,0,	optabDC,		/* 0xdc */
	FRMOP,0,	optabDD,		/* 0xdd */
	FRMOP,0,	optabDE,		/* 0xde */
	FRMOP,0,	optabDF,		/* 0xdf */
	Jbs,0,		"LOOPNE	%p",		/* 0xe0 */
	Jbs,0,		"LOOPE	%p",		/* 0xe1 */
	Jbs,0,		"LOOP	%p",		/* 0xe2 */
	Jbs,0,		"JCXZ	%p",		/* 0xe3 */
	Ib,0,		"INB	%i,AL",		/* 0xe4 */
	Ib,0,		"IN%S	%i,%OAX",	/* 0xe5 */
	Ib,0,		"OUTB	AL,%i",		/* 0xe6 */
	Ib,0,		"OUT%S	%OAX,%i",	/* 0xe7 */
	Iwds,0,		"CALL	%p",		/* 0xe8 */
	Iwds,0,		"JMP	%p",		/* 0xe9 */
	PTR,0,		"JMP	%d",		/* 0xea */
	Jbs,0,		"JMP	%p",		/* 0xeb */
	0,0,		"INB	DX,AL",		/* 0xec */
	0,0,		"IN%S	DX,%OAX",	/* 0xed */
	0,0,		"OUTB	AL,DX",		/* 0xee */
	0,0,		"OUT%S	%OAX,DX",	/* 0xef */
	PRE,0,		"LOCK",			/* 0xf0 */
	0,0,		"",			/* 0xf1 */	
	PRE,0,		"REPNE",		/* 0xf2 */
	PRE,0,		"REP",			/* 0xf3 */
	0,0,		"HALT",			/* 0xf4 */
	0,0,		"CMC",			/* 0xf5 */
	RMOPB,0,	optabF6,		/* 0xf6 */
	RMOP,0,		optabF7,		/* 0xf7 */
	0,0,		"CLC",			/* 0xf8 */
	0,0,		"STC",			/* 0xf9 */
	0,0,		"CLI",			/* 0xfa */
	0,0,		"STI",			/* 0xfb */
	0,0,		"CLD",			/* 0xfc */
	0,0,		"STD",			/* 0xfd */
	RMOPB,0,	optabFE,		/* 0xfe */
	RMOP,0,		optabFF,		/* 0xff */
};

/*
 *  get a byte of the instruction
 */
static int
igetc(Map * map, Instr *ip, uchar *c)
{
	if(ip->n+1 > sizeof(ip->mem)){
		werrstr("instruction too long");
		return -1;
	}
	if (get1(map, ip->addr+ip->n, c, 1) < 0) {
		werrstr("can't read instruction: %r");
		return -1;
	}
	ip->mem[ip->n++] = *c;
	return 1;
}

/*
 *  get two bytes of the instruction
 */
static int
igets(Map *map, Instr *ip, ushort *sp)
{
	uchar	c;
	ushort s;

	if (igetc(map, ip, &c) < 0)
		return -1;
	s = c;
	if (igetc(map, ip, &c) < 0)
		return -1;
	s |= (c<<8);
	*sp = s;
	return 1;
}

/*
 *  get 4 bytes of the instruction
 */
static int
igetl(Map *map, Instr *ip, ulong *lp)
{
	ushort s;
	long	l;

	if (igets(map, ip, &s) < 0)
		return -1;
	l = s;
	if (igets(map, ip, &s) < 0)
		return -1;
	l |= (s<<16);
	*lp = l;
	return 1;
}

static int
getdisp(Map *map, Instr *ip, int mod, int rm, int code)
{
	uchar c;
	ushort s;

	if (mod > 2)
		return 1;
	if (mod == 1) {
		if (igetc(map, ip, &c) < 0)
			return -1;
		if (c&0x80)
			ip->disp = c|0xffffff00;
		else
			ip->disp = c&0xff;
	} else if (mod == 2 || rm == code) {
		if (ip->asize == 'E') {
			if (igetl(map, ip, &ip->disp) < 0)
				return -1;
		} else {
			if (igets(map, ip, &s) < 0)
				return -1;
			if (s&0x8000)
				ip->disp = s|0xffff0000;
			else
				ip->disp = s;
		}
		if (mod == 0)
			ip->base = -1;
	}
	return 1;
}

static int
modrm(Map *map, Instr *ip, uchar c)
{
	uchar rm, mod;

	mod = (c>>6)&3;
	rm = c&7;
	ip->mod = mod;
	ip->base = rm;
	ip->reg = (c>>3)&7;
	if (mod == 3)			/* register */
		return 1;
	if (ip->asize == 0) {		/* 16-bit mode */
		switch(rm)
		{
		case 0:
			ip->base = BX; ip->index = SI;
			break;
		case 1:
			ip->base = BX; ip->index = DI;
			break;
		case 2:
			ip->base = BP; ip->index = SI;
			break;
		case 3:
			ip->base = BP; ip->index = DI;
			break;
		case 4:
			ip->base = SI;
			break;
		case 5:
			ip->base = DI;
			break;
		case 6:
			ip->base = BP;
			break;
		case 7:
			ip->base = BX;
			break;
		default:
			break;
		}
		return getdisp(map, ip, mod, rm, 6);
	}
	if (rm == 4) {	/* scummy sib byte */
		if (igetc(map, ip, &c) < 0)
			return -1;
		ip->ss = (c>>6)&0x03;
		ip->index = (c>>3)&0x07;
		if (ip->index == 4)
			ip->index = -1;
		ip->base = c&0x07;
		return getdisp(map, ip, mod, ip->base, 5);
	}
	return getdisp(map, ip, mod, rm, 5);
}

static Optable *
mkinstr(Map *map, Instr *ip, ulong pc)
{
	int i, n;
	uchar c;
	ushort s;
	Optable *op, *obase;
	char buf[128];

	memset(ip, 0, sizeof(*ip));
	ip->base = -1;
	ip->index = -1;
	if(0) /* asstype == AI8086) */
		ip->osize = 'W';
	else {
		ip->osize = 'L';
		ip->asize = 'E';
	}
	ip->addr = pc;
	if (igetc(map, ip, &c) < 0)
		return 0;
	obase = optable;
newop:
	op = &obase[c];
	if (op->proto == 0) {
badop:
		n = snprint(buf, sizeof(buf), "opcode: ??");
		for (i = 0; i < ip->n && n < sizeof(buf)-3; i++, n+=2)
			_hexify(buf+n, ip->mem[i], 1);
		strcpy(buf+n, "??");
		werrstr(buf);
		return 0;
	}
	for(i = 0; i < 2 && op->operand[i]; i++) {
		switch(op->operand[i])
		{
		case Ib:	/* 8-bit immediate - (no sign extension)*/
			if (igetc(map, ip, &c) < 0)
				return 0;
			ip->imm = c&0xff;
			break;
		case Jbs:	/* 8-bit jump immediate (sign extended) */
			if (igetc(map, ip, &c) < 0)
				return 0;
			if (c&0x80)
				ip->imm = c|0xffffff00;
			else
				ip->imm = c&0xff;
			ip->jumptype = Jbs;
			break;
		case Ibs:	/* 8-bit immediate (sign extended) */
			if (igetc(map, ip, &c) < 0)
				return 0;
			if (c&0x80)
				if (ip->osize == 'L')
					ip->imm = c|0xffffff00;
				else
					ip->imm = c|0xff00;
			else
				ip->imm = c&0xff;
			break;
		case Iw:	/* 16-bit immediate -> imm */
			if (igets(map, ip, &s) < 0)
				return 0;
			ip->imm = s&0xffff;
			ip->jumptype = Iw;
			break;
		case Iw2:	/* 16-bit immediate -> in imm2*/
			if (igets(map, ip, &s) < 0)
				return 0;
			ip->imm2 = s&0xffff;
			break;
		case Iwd:	/* Operand-sized immediate (no sign extension)*/
			if (ip->osize == 'L') {
				if (igetl(map, ip, &ip->imm) < 0)
					return 0;
			} else {
				if (igets(map, ip, &s)< 0)
					return 0;
				ip->imm = s&0xffff;
			}
			break;
		case Awd:	/* Address-sized immediate (no sign extension)*/
			if (ip->asize == 'E') {
				if (igetl(map, ip, &ip->imm) < 0)
					return 0;
			} else {
				if (igets(map, ip, &s)< 0)
					return 0;
				ip->imm = s&0xffff;
			}
			break;
		case Iwds:	/* Operand-sized immediate (sign extended) */
			if (ip->osize == 'L') {
				if (igetl(map, ip, &ip->imm) < 0)
					return 0;
			} else {
				if (igets(map, ip, &s)< 0)
					return 0;
				if (s&0x8000)
					ip->imm = s|0xffff0000;
				else
					ip->imm = s&0xffff;
			}
			ip->jumptype = Iwds;
			break;
		case OA:	/* literal 0x0a byte */
			if (igetc(map, ip, &c) < 0)
				return 0;
			if (c != 0x0a)
				goto badop;
			break;
		case R0:	/* base register must be R0 */
			if (ip->base != 0)
				goto badop;
			break;
		case R1:	/* base register must be R1 */
			if (ip->base != 1)
				goto badop;
			break;
		case RMB:	/* R/M field with byte register (/r)*/
			if (igetc(map, ip, &c) < 0)
				return 0;
			if (modrm(map, ip, c) < 0)
				return 0;
			ip->osize = 'B';
			break;
		case RM:	/* R/M field with register (/r) */
			if (igetc(map, ip, &c) < 0)
				return 0;
			if (modrm(map, ip, c) < 0)
				return 0;
			break;
		case RMOPB:	/* R/M field with op code (/digit) */
			if (igetc(map, ip, &c) < 0)
				return 0;
			if (modrm(map, ip, c) < 0)
				return 0;
			c = ip->reg;		/* secondary op code */
			obase = (Optable*)op->proto;
			ip->osize = 'B';
			goto newop;
		case RMOP:	/* R/M field with op code (/digit) */
			if (igetc(map, ip, &c) < 0)
				return 0;
			if (modrm(map, ip, c) < 0)
				return 0;
			c = ip->reg;
			obase = (Optable*)op->proto;
			goto newop;
		case FRMOP:	/* FP R/M field with op code (/digit) */
			if (igetc(map, ip, &c) < 0)
				return 0;
			if (modrm(map, ip, c) < 0)
				return 0;
			if ((c&0xc0) == 0xc0)
				c = ip->reg+8;		/* 16 entry table */
			else
				c = ip->reg;
			obase = (Optable*)op->proto;
			goto newop;
		case FRMEX:	/* Extended FP R/M field with op code (/digit) */
			if (igetc(map, ip, &c) < 0)
				return 0;
			if (modrm(map, ip, c) < 0)
				return 0;
			if ((c&0xc0) == 0xc0)
				c = (c&0x3f)+8;		/* 64-entry table */
			else
				c = ip->reg;
			obase = (Optable*)op->proto;
			goto newop;
		case RMR:	/* R/M register only (mod = 11) */
			if (igetc(map, ip, &c) < 0)
				return 0;
			if ((c&0xc0) != 0xc0) {
				werrstr("invalid R/M register: %x", c);
				return 0;
			}
			if (modrm(map, ip, c) < 0)
				return 0;
			break;
		case RMM:	/* R/M register only (mod = 11) */
			if (igetc(map, ip, &c) < 0)
				return 0;
			if ((c&0xc0) == 0xc0) {
				werrstr("invalid R/M memory mode: %x", c);
				return 0;
			}
			if (modrm(map, ip, c) < 0)
				return 0;
			break;
		case PTR:	/* Seg:Displacement addr (ptr16:16 or ptr16:32) */
			if (ip->osize == 'L') {
				if (igetl(map, ip, &ip->disp) < 0)
					return 0;
			} else {
				if (igets(map, ip, &s)< 0)
					return 0;
				ip->disp = s&0xffff;
			}
			if (igets(map, ip, (ushort*)&ip->seg) < 0)
				return 0;
			ip->jumptype = PTR;
			break;
		case AUX:	/* Multi-byte op code - Auxiliary table */
			obase = (Optable*)op->proto;
			if (igetc(map, ip, &c) < 0)
				return 0;
			goto newop;
		case PRE:	/* Instr Prefix */
			ip->prefix = (char*)op->proto;
			if (igetc(map, ip, &c) < 0)
				return 0;
			goto newop;
		case SEG:	/* Segment Prefix */
			ip->segment = (char*)op->proto;
			if (igetc(map, ip, &c) < 0)
				return 0;
			goto newop;
		case OPOVER:	/* Operand size override */
			ip->osize = 'W';
			if (igetc(map, ip, &c) < 0)
				return 0;
			goto newop;
		case ADDOVER:	/* Address size override */
			ip->asize = 0;
			if (igetc(map, ip, &c) < 0)
				return 0;
			goto newop;
		case JUMP:	/* mark instruction as JUMP or RET */
		case RET:
			ip->jumptype = op->operand[i];
			break;
		default:
			werrstr("bad operand type %d", op->operand[i]);
			return 0;
		}
	}
	return op;
}

static void
bprint(Instr *ip, char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	ip->curr = vseprint(ip->curr, ip->end, fmt, arg);
	va_end(arg);
}

/*
 *  if we want to call 16 bit regs AX,BX,CX,...
 *  and 32 bit regs EAX,EBX,ECX,... then
 *  change the defs of ANAME and ONAME to:
 *  #define	ANAME(ip)	((ip->asize == 'E' ? "E" : "")
 *  #define	ONAME(ip)	((ip)->osize == 'L' ? "E" : "")
 */
#define	ANAME(ip)	""
#define	ONAME(ip)	""

static char *reg[] =  {
	"AX",		/* 0 */
	"CX",		/* 1 */
	"DX",		/* 2 */
	"BX",		/* 3 */
	"SP",		/* 4 */
	"BP",		/* 5 */
	"SI",		/* 6 */
	"DI",		/* 7 */
};

static char *breg[] = { "AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH" };
static char *sreg[] = { "ES", "CS", "SS", "DS", "FS", "GS" };

static void
plocal(Instr *ip)
{
	Symbol s;
	char *name;
	Loc l, li;

	l.type = LOFFSET;
	l.offset = ip->disp;
	if(ip->base == SP)
		l.reg = "SP";
	else
		l.reg = "BP";

	li.type = LADDR;
	li.addr = ip->addr;
	if(findsym(li, CTEXT, &s) < 0)
		goto raw;

	name = nil;
	if(ip->base==SP && lookuplsym(&s, FRAMENAME, &s) >= 0){
		/* translate stack offset to offset from plan 9 frame pointer */
		/* XXX not sure how to do this */
	}

	if(name==nil && findlsym(&s, l, &s) >= 0)
		name = s.name;

	if(name)
		bprint(ip, "%s+", name);

raw:
	bprint(ip, "%lx(%s)", l.offset, l.reg);
}

static int
isjmp(Instr *ip)
{
	switch(ip->jumptype){
	case Iwds:
	case Jbs:
	case JUMP:
		return 1;
	default:
		return 0;
	}
}

/*
 * This is too smart for its own good, but it really is nice
 * to have accurate translations when debugging, and it
 * helps us identify which code is different in binaries that
 * are changed on sources.
 */
static int
issymref(Instr *ip, Symbol *s, long w, long val)
{
	Symbol next, tmp;
	long isstring, size;

	if (isjmp(ip))
		return 1;
	if (s->class==CTEXT && w==0)
		return 1;
	if (s->class==CDATA) {
		/* use first bss symbol (or "end") rather than edata */
		if (s->name[0]=='e' && strcmp(s->name, "edata") == 0){
			if((indexsym(s->index+1, &tmp) && loccmp(&tmp.loc, &s->loc)==0)
			|| (indexsym(s->index-1, &tmp) && loccmp(&tmp.loc, &s->loc)==0))
				*s = tmp;
		}
		if (w == 0)
			return 1;
		for (next=*s; next.loc.addr==s->loc.addr; next=tmp)
			if (!indexsym(next.index+1, &tmp))
				break;
		size = next.loc.addr - s->loc.addr;
		if (w >= size)
			return 0;
		if (w > size-w)
			w = size-w;
		/* huge distances are usually wrong except in .string */
		isstring = (s->name[0]=='.' && strcmp(s->name, ".string") == 0);
		if (w > 8192 && !isstring)
			return 0;
		/* medium distances are tricky - look for constants */
		/* near powers of two */
		if ((val&(val-1)) == 0 || (val&(val+1)) == 0)
			return 0;
		return 1;
	}
	return 0;
}

static void
immediate(Instr *ip, long val)
{
	Symbol s;
	long w;
	Loc l;

	l.type = LADDR;
	l.addr = val;
	if (findsym(l, CANY, &s) >= 0) {
		w = val - s.loc.addr;
		if (w < 0)
			w = -w;
		if (issymref(ip, &s, w, val)) {
			if (w)
				bprint(ip, "%s+%lux(SB)", s.name, w);
			else
				bprint(ip, "%s(SB)", s.name);
			return;
		}
		if (s.class==CDATA && indexsym(s.index+1, &s) >= 0) {
			w = s.loc.addr - val;
			if (w < 0)
				w = -w;
			if (w < 4096) {
				bprint(ip, "%s-%lux(SB)", s.name, w);
				return;
			}
		}
	}
	bprint(ip, "%lux", val);
}

static void
pea(Instr *ip)
{
	if (ip->mod == 3) {
		if (ip->osize == 'B')
			bprint(ip, breg[(uchar)ip->base]);
		else
			bprint(ip, "%s%s", ANAME(ip), reg[(uchar)ip->base]);
		return;
	}
	if (ip->segment)
		bprint(ip, ip->segment);
	if (ip->asize == 'E' && (ip->base == SP || ip->base == BP))
		plocal(ip);
	else {
		if (ip->base < 0)
			immediate(ip, ip->disp);
		else
			bprint(ip,"(%s%s)", ANAME(ip), reg[(uchar)ip->base]);
	}
	if (ip->index >= 0)
		bprint(ip,"(%s%s*%d)", ANAME(ip), reg[(uchar)ip->index], 1<<ip->ss);
}

static void
prinstr(Instr *ip, char *fmt)
{
	if (ip->prefix)
		bprint(ip, "%s ", ip->prefix);
	for (; *fmt && ip->curr < ip->end; fmt++) {
		if (*fmt != '%')
			*ip->curr++ = *fmt;
		else switch(*++fmt)
		{
		case '%':
			*ip->curr++ = '%';
			break;
		case 'A':
			bprint(ip, "%s", ANAME(ip));
			break;
		case 'C':
			bprint(ip, "CR%d", ip->reg);
			break;
		case 'D':
			if (ip->reg < 4 || ip->reg == 6 || ip->reg == 7)
				bprint(ip, "DR%d",ip->reg);
			else
				bprint(ip, "???");
			break;
		case 'I':
			bprint(ip, "$");
			immediate(ip, ip->imm2);
			break;
		case 'O':
			bprint(ip,"%s", ONAME(ip));
			break;
		case 'i':
			bprint(ip, "$");
			immediate(ip,ip->imm);
			break;
		case 'R':
			bprint(ip, "%s%s", ONAME(ip), reg[ip->reg]);
			break;
		case 'S':
			bprint(ip, "%c", ip->osize);
			break;
		case 'T':
			if (ip->reg == 6 || ip->reg == 7)
				bprint(ip, "TR%d",ip->reg);
			else
				bprint(ip, "???");
			break;
		case 'X':
			if (ip->osize == 'L')
				bprint(ip,"CWDE");
			else
				bprint(ip, "CBW");
			break;
		case 'd':
			bprint(ip,"%lux:%lux",ip->seg,ip->disp);
			break;
		case 'e':
			pea(ip);
			break;
		case 'f':
			bprint(ip, "F%d", ip->base);
			break;
		case 'g':
			if (ip->reg < 6)
				bprint(ip,"%s",sreg[ip->reg]);
			else
				bprint(ip,"???");
			break;
		case 'p':
			immediate(ip, ip->imm+ip->addr+ip->n);
			break;
		case 'r':
			if (ip->osize == 'B')
				bprint(ip,"%s",breg[ip->reg]);
			else
				bprint(ip, reg[ip->reg]);
			break;
		case 'x':
			if (ip->osize == 'L')
				bprint(ip,"CDQ");
			else
				bprint(ip, "CWD");
			break;
		default:
			bprint(ip, "%%%c", *fmt);
			break;
		}
	}
	*ip->curr = 0;		/* there's always room for 1 byte */
}

static int
i386das(Map *map, ulong pc, char modifier, char *buf, int n)
{
	Instr	instr;
	Optable *op;

	USED(modifier);
	op = mkinstr(map, &instr, pc);
	if (op == 0) {
		errstr(buf, n);
		return -1;
	}
	instr.curr = buf;
	instr.end = buf+n-1;
	prinstr(&instr, op->proto);
	return instr.n;
}

static int
i386hexinst(Map *map, ulong pc, char *buf, int n)
{
	Instr	instr;
	int i;

	if (mkinstr(map, &instr, pc) == 0) {
		errstr(buf, n);
		return -1;
	}
	for(i = 0; i < instr.n && n > 2; i++) {
		_hexify(buf, instr.mem[i], 1);
		buf += 2;
		n -= 2;
	}
	*buf = 0;
	return instr.n;
}

static int
i386instlen(Map *map, ulong pc)
{
	Instr i;

	if (mkinstr(map, &i, pc))
		return i.n;
	return -1;
}

static int
i386foll(Map *map, Regs *regs, ulong pc, ulong *foll)
{
	Instr i;
	Optable *op;
	ushort s;
	ulong addr;
	u32int l;
	int n;

	op = mkinstr(map, &i, pc);
	if (!op)
		return -1;

	n = 0;

	switch(i.jumptype) {
	case RET:		/* RETURN or LEAVE */
	case Iw:		/* RETURN */
		if (strcmp(op->proto, "LEAVE") == 0) {
			if (lget4(map, regs, locindir("BP", 0), &l) < 0)
				return -1;
		} else if (lget4(map, regs, locindir(mach->sp, 0), &l) < 0)
			return -1;
		foll[0] = l;
		return 1;
	case Iwds:		/* pc relative JUMP or CALL*/
	case Jbs:		/* pc relative JUMP or CALL */
		foll[0] = pc+i.imm+i.n;
		n = 1;
		break;
	case PTR:		/* seg:displacement JUMP or CALL */
		foll[0] = (i.seg<<4)+i.disp;
		return 1;
	case JUMP:		/* JUMP or CALL EA */

		if(i.mod == 3) {
			if (rget(regs, reg[(uchar)i.base], &foll[0]) < 0)
				return -1;
			return 1;
		}
			/* calculate the effective address */
		addr = i.disp;
		if (i.base >= 0) {
			if (lget4(map, regs, locindir(reg[(uchar)i.base], 0), &l) < 0)
				return -1;
			addr += l;
		}
		if (i.index >= 0) {
			if (lget4(map, regs, locindir(reg[(uchar)i.index], 0), &l) < 0)
				return -1;
			addr += l*(1<<i.ss);
		}
			/* now retrieve a seg:disp value at that address */
		if (get2(map, addr, &s) < 0)		/* seg */
			return -1;
		foll[0] = s<<4;
		addr += 2;
		if (i.asize == 'L') {
			if (get4(map, addr, &l) < 0)	/* disp32 */
				return -1;
			foll[0] += l;
		} else {					/* disp16 */
			if (get2(map, addr, &s) < 0)
				return -1;
			foll[0] += s;
		}
		return 1;
	default:
		break;
	}		
	if (strncmp(op->proto,"JMP", 3) == 0 || strncmp(op->proto,"CALL", 4) == 0)
		return 1;
	foll[n++] = pc+i.n;
	return n;
}
