// See ../src/libmach/LICENSE

#ifndef _MACH_H_
#define _MACH_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif

AUTOLIB(mach)

/*
 * Architecture-dependent application data.
 * 
 * The code assumes that u64int is big enough to hold
 * an address on any system of interest as well as any
 * register.
 *
 * Supported architectures:
 *
 *	MIPS R3000
 *	Motorola 68020
 *	Intel 386
 * 	SPARC
 *	PowerPC (limited)
 *	ARM (limited)
 *	Intel 960 (limited)
 *	AT&T 3210 DSP (limited)
 *	MIPS2 (R4000)
 */

typedef struct Fhdr Fhdr;
typedef struct Loc Loc;
typedef struct Mach Mach;
typedef struct Map Map;
typedef struct Regdesc Regdesc;
typedef struct Regs Regs;
typedef struct Seg Seg;
typedef struct Symbol Symbol;
typedef struct Symtype Symtype;

typedef int (*Tracer)(Map*, Regs*, u64int, u64int, Symbol*, int);

extern	Mach	*mach;
extern	Mach	*machcpu;

/*
 * Byte-order data layout manipulation.
 * 	swap.c ieee.c
 */
u16int	beswap2(u16int u);
u32int	beswap4(u32int u);
u64int	beswap8(u64int u);
int		beieeeftoa32(char*, uint, void*);
int		beieeeftoa64(char*, uint, void*);
int		beieeeftoa80(char*, uint, void*);

u16int	leswap2(u16int u);
u32int	leswap4(u32int u);
u64int	leswap8(u64int u);
int		leieeeftoa32(char *a, uint n, void *v);
int		leieeeftoa64(char *a, uint n, void *v);
int		leieeeftoa80(char *a, uint n, void *v);

u16int	beload2(uchar*);
u32int	beload4(uchar*);
u64int	beload8(uchar*);

u16int	leload2(uchar*);
u32int	leload4(uchar*);
u64int	leload8(uchar*);

int		ieeeftoa32(char *a, uint n, u32int u);
int		ieeeftoa64(char *a, uint n, u32int h, u32int u);

/*
 * Machine-independent access to an executable image.
 *	map.c
 */
struct Seg
{
	char		*name;
	char		*file;
	uchar	*p;
	int		fd;
	int		pid;
	u64int	base;
	u64int	size;
	u64int	offset;
	int		(*rw)(Map*, Seg*, u64int, void*, uint, int);
};

struct Map
{
	int		nseg;
	Seg		*seg;
};

struct Regs
{
	int		(*rw)(Regs*, char*, u64int*, int);
};

typedef struct UregRegs UregRegs;
struct UregRegs
{
	Regs		r;
	uchar	*ureg;
};
int		_uregrw(Regs*, char*, u64int*, int);

typedef struct PidRegs PidRegs;
struct PidRegs
{
	Regs		r;
	int		pid;
};

Map*	allocmap(void);
int		addseg(Map *map, Seg seg);
int		findseg(Map *map, char *name, char *file);
int		addrtoseg(Map *map, u64int addr, Seg *seg);
int		addrtosegafter(Map *map, u64int addr, Seg *seg);
void		removeseg(Map *map, int i);
void		freemap(Map*);

int		get1(Map *map, u64int addr, uchar *a, uint n);
int		get2(Map *map, u64int addr, u16int *u);
int		get4(Map *map, u64int addr, u32int *u);
int		get8(Map *map, u64int addr, u64int *u);
int		geta(Map *map, u64int addr, u64int *u);

int		put1(Map *map, u64int addr, uchar *a, uint n);
int		put2(Map *map, u64int addr, u16int u);
int		put4(Map *map, u64int addr, u32int u);
int		put8(Map *map, u64int addr, u64int u);

int		rget(Regs*, char*, u64int*);
int		rput(Regs*, char*, u64int);

/* 
 * A location is either a memory address or a register.
 * It is useful to be able to specify constant values that
 * originate from outside the register set and memory,
 * hence LCONST.  If the register values are known, then
 * we can dispense with LOFFSET, but it's useful to be able
 * to look up local symbols (via findlsym) with locations
 * like 8(BP).
 *
 *	loc.c
 */

enum
{
	/* location type */
	LNONE,
	LREG,		/* register */
	LADDR,		/* absolute address */
	LCONST,		/* constant (an anonymous readonly location) */
	LOFFSET		/* dereference offset + register ptr */
};

struct Loc
{
	uint type;		/* LNONE, ... */
	char *reg;		/* LREG */
	u64int addr;	/* LADDR, CONST */
	long offset;	/* LOFFSET */
};

int		lget1(Map *map, Regs *regs, Loc loc, uchar *a, uint n);
int		lget2(Map *map, Regs *regs, Loc loc, u16int *v);
int		lget4(Map *map, Regs *regs, Loc loc, u32int *v);
int		lget8(Map *map, Regs *regs, Loc loc, u64int *v);
int		lgeta(Map *map, Regs *regs, Loc loc, u64int *v);

int		lput1(Map *map, Regs *regs, Loc loc, uchar *a, uint n);
int		lput2(Map *map, Regs *regs, Loc loc, u16int v);
int		lput4(Map *map, Regs *regs, Loc loc, u32int v);
int		lput8(Map *map, Regs *regs, Loc loc, u64int v);

Loc		locnone(void);
Loc		locaddr(u64int addr);
Loc		locconst(u64int con);
Loc		locreg(char*);
Loc		locindir(char*, long);

/* 
 * Executable file parsing.
 *
 * An Fhdr represents an open file image.
 * The contents are a grab bag of constants used for the
 * various file types.  Not all elements are used by all
 * file types.
 *
 *	crackadotplan9.c crackadotunix.c
 *	crackelf.c crackdwarf.c
 */
enum
{
	/* file types */
	FNONE,
	FEXEC,		/* executable image */
	FLIB,			/* library */
	FOBJ,		/* object file */
	FRELOC,		/* relocatable executable */
	FSHLIB,		/* shared library */
	FSHOBJ,		/* shared object */
	FCORE,		/* core dump */
	FBOOT,		/* bootable image */
	FKERNEL,		/* kernel image */
	NFTYPE,

	/* abi types */
	ANONE = 0,
	APLAN9,
	ALINUX,
	AFREEBSD,
	AMACH,
	NATYPE
};

/* I wish this could be kept in stabs.h */
struct Stab
{
	uchar *stabbase;
	uint stabsize;
	char *strbase;
	uint strsize;
	u16int (*e2)(uchar*);
	u32int (*e4)(uchar*);
};

struct Fhdr
{
	int		fd;			/* file descriptor */
	char		*filename;		/* file name */
	Mach	*mach;		/* machine */
	char		*mname;		/* 386, power, ... */	
	uint		mtype;		/* machine type M386, ... */
	char		*fname;		/* core, executable, boot image, ... */
	uint		ftype;		/* file type FCORE, ... */
	char		*aname;		/* abi name */
	uint		atype;		/* abi type ALINUX, ... */

	ulong	magic;		/* magic number */
	u64int	txtaddr;		/* text address */
	u64int	entry;		/* entry point */
	u64int	txtsz;		/* text size */
	u64int	txtoff;		/* text offset in file */
	u64int	dataddr;		/* data address */
	u64int	datsz;		/* data size */
	u64int	datoff;		/* data offset in file */
	u64int	bsssz;		/* bss size */
	u64int	symsz;		/* symbol table size */
	u64int	symoff;		/* symbol table offset in file */
	u64int	sppcsz;		/* size of sp-pc table */
	u64int	sppcoff;		/* offset of sp-pc table in file */
	u64int	lnpcsz;		/* size of line number-pc table */
	u64int	lnpcoff;		/* size of line number-pc table */
	void		*elf;			/* handle to elf image */
	void		*dwarf;		/* handle to dwarf image */
	void		*macho;		/* handle to mach-o image */
	struct Stab	stabs;
	uint		pid;			/* for core files */
	char		*prog;		/* program name, for core files */
	char		*cmdline;		/* command-line that produced core */
	struct	{			/* thread state for core files */
		uint	id;
		void	*ureg;
	} *thread;
	uint		nthread;

	/* private */
	Symbol	*sym;		/* cached list of symbols */
	Symbol	**byname;
	Symbol	**byxname;
	uint		nsym;
	Symbol	*esym;		/* elf symbols */
	uint		nesym;
	ulong	base;		/* base address for relocatables */
	Fhdr		*next;		/* link to next fhdr (internal) */

	/* file mapping */
	int		(*map)(Fhdr*, u64int, Map*, Regs**);

	/* debugging symbol access; see below */
	int		(*syminit)(Fhdr*);
	void		(*symclose)(Fhdr*);

	int		(*pc2file)(Fhdr*, u64int, char*, uint, ulong*);
	int		(*file2pc)(Fhdr*, char*, u64int, u64int*);
	int		(*line2pc)(Fhdr*, u64int, ulong, u64int*);

	int		(*lookuplsym)(Fhdr*, Symbol*, char*, Symbol*);
	int		(*indexlsym)(Fhdr*, Symbol*, uint, Symbol*);
	int		(*findlsym)(Fhdr*, Symbol*, Loc, Symbol*);

	int		(*unwind)(Fhdr*, Map*, Regs*, u64int*, Symbol*);
};

Fhdr*	crackhdr(char *file, int mode);
void		uncrackhdr(Fhdr *hdr);
int		crackelf(int fd, Fhdr *hdr);
int		crackmacho(int fd, Fhdr *hdr);
Regs*	coreregs(Fhdr*, uint);

int		symopen(Fhdr*);
int		symdwarf(Fhdr*);
int		symelf(Fhdr*);
int		symstabs(Fhdr*);
int		symmacho(Fhdr*);
void		symclose(Fhdr*);

int		mapfile(Fhdr *fp, u64int base, Map *map, Regs **regs);
void		unmapfile(Fhdr *fp, Map *map);

/*
 * Process manipulation.
 */
int		mapproc(int pid, Map *map, Regs **regs);
void		unmapproc(Map *map);
int		detachproc(int pid);
int		ctlproc(int pid, char *msg);
int		procnotes(int pid, char ***notes);
char*	proctextfile(int pid);

/*
 * Command-line debugger help
 */
extern Fhdr *symhdr;
extern Fhdr *corhdr;
extern char *symfil;
extern char *corfil;
extern int corpid;
extern Regs *correg;
extern Map *symmap;
extern Map *cormap;

int		attachproc(int pid);
int		attachcore(Fhdr *hdr);
int		attachargs(int argc, char **argv, int omode, int);
int		attachdynamic(int);
/*
 * Machine descriptions.
 *
 *	mach.c
 *	mach386.c dis386.c
 *	machsparc.c dissparc.c
 *	...
 */

/*
 * Register sets.  The Regs are opaque, accessed by using
 * the reglist (and really the accessor functions).
 */
enum
{
	/* must be big enough for all machine register sets */
	REGSIZE = 256,

	RINT = 0<<0,
	RFLT = 1<<0,
	RRDONLY = 1<<1
};

struct Regdesc
{
	char		*name;		/* register name */
	uint		offset;		/* offset in b */
	uint		flags;		/* RINT/RFLT/RRDONLY */
	uint		format;		/* print format: 'x', 'X', 'f', 'z', 'Z' */
};

enum
{
	/* machine types */
	MNONE,
	MMIPS,		/* MIPS R3000 */
	MSPARC,		/* SUN SPARC */
	M68000,		/* Motorola 68000 */
	M386,		/* Intel 32-bit x86*/
	M960,		/* Intel 960 */
	M3210,		/* AT&T 3210 DSP */
	MMIPS2,		/* MIPS R4000 */
	M29000,		/* AMD 29000 */
	MARM,		/* ARM */
	MPOWER,		/* PowerPC */
	MALPHA,		/* DEC/Compaq Alpha */
	MAMD64,		/* AMD64 */
	NMTYPE
};

struct Mach
{
	char		*name;		/* "386", ... */
	uint		type;			/* M386, ... */
	Regdesc	*reglist;		/* register set */
	uint		regsize;		/* size of register set in bytes */
	uint		fpregsize;		/* size of fp register set in bytes */
	char		*pc;			/* name of program counter */
	char		*sp;			/* name of stack pointer */
	char		*fp;			/* name of frame pointer */
	char		*link;		/* name of link register */
	char		*sbreg;		/* name of static base */
	ulong	sb;			/* value of static base */
	uint		pgsize;		/* page size */
	u64int	kbase;		/* kernel base address for Plan 9 */
	u64int	ktmask;		/* ktzero = kbase & ~ktmask */
	uint		pcquant;		/* pc quantum */
	uint		szaddr;		/* size of pointer in bytes */
	uint		szreg;		/* size of integer register */
	uint		szfloat;		/* size of float */
	uint		szdouble;		/* size of double */
	char**	windreg;		/* unwinding registers */
	uint		nwindreg;

	uchar	bpinst[4];		/* break point instruction */
	uint		bpsize;		/* size of bp instruction */

	int		(*foll)(Map*, Regs*, u64int, u64int*);	/* follow set */
	char*	(*exc)(Map*, Regs*);		/* last exception */
	int		(*unwind)(Map*, Regs*, u64int*, Symbol*);

	/* cvt to local byte order */
	u16int	(*swap2)(u16int);
	u32int	(*swap4)(u32int);
	u64int	(*swap8)(u64int);
	int		(*ftoa32)(char*, uint, void*);
	int		(*ftoa64)(char*, uint, void*);
	int		(*ftoa80)(char*, uint, void*);

	/* disassembly */
	int		(*das)(Map*, u64int, char, char*, int);	/* symbolic */
	int		(*kendas)(Map*, u64int, char, char*, int);	/* symbolic */
	int		(*codas)(Map*, u64int, char, char*, int);
	int		(*hexinst)(Map*, u64int, char*, int);	/* hex */
	int		(*instsize)(Map*, u64int);	/* instruction size */
};

Mach	*machbyname(char*);
Mach	*machbytype(uint);

extern	Mach	mach386;
extern	Mach	machsparc;
extern	Mach	machmips;
extern	Mach	machpower;
extern	Mach	machamd64;

/*
 * Debugging symbols and type information.
 * (Not all objects include type information.)
 *
 *	sym.c
 */

enum
{
	/* symbol table classes */
	CNONE,
	CAUTO,		/* stack variable */
	CPARAM,		/* function parameter */
	CTEXT,		/* text segment */
	CDATA,		/* data segment */
	CANY
};

struct Symbol
{
	char		*name;		/* name of symbol */
	char		*xname;		/* demangled name */

	/* Symtype	*typedesc;	/* type info, if any */
	Loc		loc;			/* location of symbol */
	Loc		hiloc;		/* location of end of symbol */
	char		class;		/* CAUTO, ... */
	char		type;			/* type letter from a.out.h */
	Fhdr		*fhdr;		/* where did this come from? */
	uint		index;		/* in by-address list */

	/* private use by various symbol implementations */
	union {
		struct {
			uint unit;
			uint uoff;
		} dwarf;
		struct {
			uint i;
			uint locals;
			char *dir;
			char *file;
			schar frameptr;
			uint framesize;
		} stabs;
	} u;
	
	void *aux;	/* for use by client */
};

/* look through all currently cracked Fhdrs calling their fns */
int		pc2file(u64int pc, char *file, uint nfile, ulong *line);
int		file2pc(char *file, ulong line, u64int *addr);
int		line2pc(u64int basepc, ulong line, u64int *pc);
int		fnbound(u64int pc, u64int *bounds);
int		fileline(u64int pc, char *a, uint n);
int		pc2line(u64int pc, ulong *line);

int		lookupsym(char *fn, char *var, Symbol *s);
int		indexsym(uint ndx, Symbol *s);
int		findsym(Loc loc, uint class, Symbol *s);
int		findexsym(Fhdr*, uint, Symbol*);

int		lookuplsym(Symbol *s1, char *name, Symbol *s2);
int		indexlsym(Symbol *s1, uint ndx, Symbol *s2);
int		findlsym(Symbol *s1, Loc loc, Symbol *s);
int		symoff(char *a, uint n, u64int addr, uint class);
int		unwindframe(Map *map, Regs *regs, u64int *next, Symbol*);

void		_addhdr(Fhdr*);
void		_delhdr(Fhdr*);
extern Fhdr*	fhdrlist;
Fhdr*	findhdr(char*);

Symbol*	flookupsym(Fhdr*, char*);
Symbol*	ffindsym(Fhdr*, Loc, uint);
Symbol*	_addsym(Fhdr*, Symbol*);

char*	demangle(char*, char*, int);
char*	demanglegcc3(char*, char*);
char*	demanglegcc2(char*, char*);
/*
 * Stack frame walking.
 *
 *	frame.c
 */
int		stacktrace(Map*, Regs*, Tracer);
int		windindex(char*);
Loc*		windreglocs(void);

/*
 * Debugger help.
 */
int		localaddr(Map *map, Regs *regs, char *fn, char *var, u64int *val);
int		fpformat(Map *map, Regdesc *reg, char *a, uint n, uint code);
char*	_hexify(char*, u64int, int);
int		locfmt(Fmt*);
int		loccmp(Loc*, Loc*);
int		locsimplify(Map *map, Regs *regs, Loc loc, Loc *newloc);
Regdesc*	regdesc(char*);

extern int machdebug;
#if defined(__cplusplus)
}
#endif
#endif
