#ifndef _MACH_H_
#define _MACH_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif

AUTOLIB(mach)

/*
 * Architecture-dependent application data.
 * 
 * The code assumes that ulong is big enough to hold
 * an address on any system of interest as well as any
 * register.  Debugging 64-bit code on 32-bit machines
 * will be interesting.
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

typedef int (*Tracer)(Map*, Regs*, ulong, ulong, Symbol*, int);

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
	ulong	base;
	ulong	size;
	ulong	offset;
	int		(*rw)(Map*, Seg*, ulong, void*, uint, int);
};

struct Map
{
	int		nseg;
	Seg		*seg;
};

struct Regs
{
	int		(*rw)(Regs*, char*, ulong*, int);
};

typedef struct UregRegs UregRegs;
struct UregRegs
{
	Regs		r;
	uchar	*ureg;
};
int		_uregrw(Regs*, char*, ulong*, int);

typedef struct PidRegs PidRegs;
struct PidRegs
{
	Regs		r;
	int		pid;
};

Map*	allocmap(void);
int		addseg(Map *map, Seg seg);
int		findseg(Map *map, char *name, char *file);
int		addrtoseg(Map *map, ulong addr, Seg *seg);
int		addrtosegafter(Map *map, ulong addr, Seg *seg);
void		removeseg(Map *map, int i);
void		freemap(Map*);

int		get1(Map *map, ulong addr, uchar *a, uint n);
int		get2(Map *map, ulong addr, u16int *u);
int		get4(Map *map, ulong addr, u32int *u);
int		get8(Map *map, ulong addr, u64int *u);

int		put1(Map *map, ulong addr, uchar *a, uint n);
int		put2(Map *map, ulong addr, u16int u);
int		put4(Map *map, ulong addr, u32int u);
int		put8(Map *map, ulong addr, u64int u);

int		rget(Regs*, char*, ulong*);
int		rput(Regs*, char*, ulong);

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
	LOFFSET,		/* dereference offset + register ptr */
};

struct Loc
{
	uint type;		/* LNONE, ... */
	char *reg;		/* LREG */
	ulong addr;	/* LADDR, CONST */
	long offset;	/* LOFFSET */
};

int		lget1(Map *map, Regs *regs, Loc loc, uchar *a, uint n);
int		lget2(Map *map, Regs *regs, Loc loc, u16int *v);
int		lget4(Map *map, Regs *regs, Loc loc, u32int *v);
int		lget8(Map *map, Regs *regs, Loc loc, u64int *v);

int		lput1(Map *map, Regs *regs, Loc loc, uchar *a, uint n);
int		lput2(Map *map, Regs *regs, Loc loc, u16int v);
int		lput4(Map *map, Regs *regs, Loc loc, u32int v);
int		lput8(Map *map, Regs *regs, Loc loc, u64int v);

Loc		locnone(void);
Loc		locaddr(ulong addr);
Loc		locconst(ulong con);
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
	ulong	txtaddr;		/* text address */
	ulong	entry;		/* entry point */
	ulong	txtsz;		/* text size */
	ulong	txtoff;		/* text offset in file */
	ulong	dataddr;		/* data address */
	ulong	datsz;		/* data size */
	ulong	datoff;		/* data offset in file */
	ulong	bsssz;		/* bss size */
	ulong	symsz;		/* symbol table size */
	ulong	symoff;		/* symbol table offset in file */
	ulong	sppcsz;		/* size of sp-pc table */
	ulong	sppcoff;		/* offset of sp-pc table in file */
	ulong	lnpcsz;		/* size of line number-pc table */
	ulong	lnpcoff;		/* size of line number-pc table */
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
	uint		nsym;
	Symbol	*esym;		/* elf symbols */
	Symbol	**ebyname;
	uint		nesym;
	ulong	base;		/* base address for relocatables */
	Fhdr		*next;		/* link to next fhdr (internal) */

	/* file mapping */
	int		(*map)(Fhdr*, ulong, Map*, Regs**);

	/* debugging symbol access; see below */
	int		(*syminit)(Fhdr*);
	void		(*symclose)(Fhdr*);

	int		(*pc2file)(Fhdr*, ulong, char*, uint, ulong*);
	int		(*file2pc)(Fhdr*, char*, ulong, ulong*);
	int		(*line2pc)(Fhdr*, ulong, ulong, ulong*);

	int		(*lookuplsym)(Fhdr*, Symbol*, char*, Symbol*);
	int		(*indexlsym)(Fhdr*, Symbol*, uint, Symbol*);
	int		(*findlsym)(Fhdr*, Symbol*, Loc, Symbol*);

	int		(*unwind)(Fhdr*, Map*, Regs*, ulong*, Symbol*);
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

int		mapfile(Fhdr *fp, ulong base, Map *map, Regs **regs);
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
	RRDONLY = 1<<1,
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
	ulong	kbase;		/* kernel base address for Plan 9 */
	ulong	ktmask;		/* ktzero = kbase & ~ktmask */
	uint		pcquant;		/* pc quantum */
	uint		szaddr;		/* size of pointer in bytes */
	uint		szreg;		/* size of integer register */
	uint		szfloat;		/* size of float */
	uint		szdouble;		/* size of double */
	char**	windreg;		/* unwinding registers */
	uint		nwindreg;

	uchar	bpinst[4];		/* break point instruction */
	uint		bpsize;		/* size of bp instruction */

	int		(*foll)(Map*, Regs*, ulong, ulong*);	/* follow set */
	char*	(*exc)(Map*, Regs*);		/* last exception */
	int		(*unwind)(Map*, Regs*, ulong*, Symbol*);

	/* cvt to local byte order */
	u16int	(*swap2)(u16int);
	u32int	(*swap4)(u32int);
	u64int	(*swap8)(u64int);
	int		(*ftoa32)(char*, uint, void*);
	int		(*ftoa64)(char*, uint, void*);
	int		(*ftoa80)(char*, uint, void*);

	/* disassembly */
	int		(*das)(Map*, ulong, char, char*, int);	/* symbolic */
	int		(*kendas)(Map*, ulong, char, char*, int);	/* symbolic */
	int		(*codas)(Map*, ulong, char, char*, int);
	int		(*hexinst)(Map*, ulong, char*, int);	/* hex */
	int		(*instsize)(Map*, ulong);	/* instruction size */
};

Mach	*machbyname(char*);
Mach	*machbytype(uint);

extern	Mach	mach386;
extern	Mach	machsparc;
extern	Mach	machmips;
extern	Mach	machpower;

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
	CANY,
};

struct Symbol
{
	char		*name;		/* name of symbol */
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
			char frameptr;
			uint framesize;
		} stabs;
	} u;
};

/* look through all currently cracked Fhdrs calling their fns */
int		pc2file(ulong pc, char *file, uint nfile, ulong *line);
int		file2pc(char *file, ulong line, ulong *addr);
int		line2pc(ulong basepc, ulong line, ulong *pc);
int		fnbound(ulong pc, ulong *bounds);
int		fileline(ulong pc, char *a, uint n);
int		pc2line(ulong pc, ulong *line);

int		lookupsym(char *fn, char *var, Symbol *s);
int		indexsym(uint ndx, Symbol *s);
int		findsym(Loc loc, uint class, Symbol *s);
int		findexsym(Fhdr*, uint, Symbol*);

int		lookuplsym(Symbol *s1, char *name, Symbol *s2);
int		indexlsym(Symbol *s1, uint ndx, Symbol *s2);
int		findlsym(Symbol *s1, Loc loc, Symbol *s);
int		symoff(char *a, uint n, ulong addr, uint class);
int		unwindframe(Map *map, Regs *regs, ulong *next, Symbol*);

void		_addhdr(Fhdr*);
void		_delhdr(Fhdr*);
extern Fhdr*	fhdrlist;
Fhdr*	findhdr(char*);

Symbol*	flookupsym(Fhdr*, char*);
Symbol*	ffindsym(Fhdr*, Loc, uint);
Symbol*	_addsym(Fhdr*, Symbol*);

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
int		localaddr(Map *map, Regs *regs, char *fn, char *var, ulong *val);
int		fpformat(Map *map, Regdesc *reg, char *a, uint n, uint code);
char*	_hexify(char*, ulong, int);
int		locfmt(Fmt*);
int		loccmp(Loc*, Loc*);
int		locsimplify(Map *map, Regs *regs, Loc loc, Loc *newloc);
Regdesc*	regdesc(char*);

struct ps_prochandle
{
	int pid;
};

int		sys_ps_lgetregs(struct ps_prochandle*, uint, void*);
int		sys_ps_lgetfpregs(struct ps_prochandle*, uint, void*);
int		sys_ps_lsetregs(struct ps_prochandle*, uint, void*);
int		sys_ps_lsetfpregs(struct ps_prochandle*, uint, void*);
Regs*	threadregs(uint);
int		pthreaddbinit(void);

extern int machdebug;
#if defined(__cplusplus)
}
#endif
#endif
