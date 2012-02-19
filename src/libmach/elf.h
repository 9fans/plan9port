/*
 * Copyright (c) 2004 Russ Cox.  See LICENSE.
 */

/* /home/rsc/papers/elfXXelf.pdf */

typedef struct Elf Elf;
typedef struct ElfHdr ElfHdr;
typedef struct ElfSect ElfSect;
typedef struct ElfProg ElfProg;
typedef struct ElfNote ElfNote;
typedef struct ElfSym ElfSym;

enum
{
	ElfClassNone = 0,
	ElfClass32,
	ElfClass64,

	ElfDataNone = 0,
	ElfDataLsb,
	ElfDataMsb,

	ElfTypeNone = 0,
	ElfTypeRelocatable,
	ElfTypeExecutable,
	ElfTypeSharedObject,
	ElfTypeCore,
	/* 0xFF00 - 0xFFFF reserved for processor-specific types */

	ElfMachNone = 0,
	ElfMach32100,		/* AT&T WE 32100 */
	ElfMachSparc,		/* SPARC */
	ElfMach386,		/* Intel 80386 */
	ElfMach68000,		/* Motorola 68000 */
	ElfMach88000,		/* Motorola 88000 */
	ElfMach486,		/* Intel 80486, no longer used */
	ElfMach860,		/* Intel 80860 */
	ElfMachMips,		/* MIPS RS3000 */
	ElfMachS370,		/* IBM System/370 */
	ElfMachMipsLe,	/* MIPS RS3000 LE */
	ElfMachParisc = 15,		/* HP PA RISC */
	ElfMachVpp500 = 17,	/* Fujitsu VPP500 */
	ElfMachSparc32Plus,	/* SPARC V8+ */
	ElfMach960,		/* Intel 80960 */
	ElfMachPower,		/* PowerPC */
	ElfMachPower64,	/* PowerPC 64 */
	ElfMachS390,		/* IBM System/390 */
	ElfMachV800 = 36,	/* NEC V800 */
	ElfMachFr20,		/* Fujitsu FR20 */
	ElfMachRh32,		/* TRW RH-32 */
	ElfMachRce,		/* Motorola RCE */
	ElfMachArm,		/* ARM */
	ElfMachAlpha,		/* Digital Alpha */
	ElfMachSH,		/* Hitachi SH */
	ElfMachSparc9,		/* SPARC V9 */
	ElfMachAmd64 = 62,	/* x86-64 */
	/* and the list goes on... */

	ElfAbiNone = 0,
	ElfAbiSystemV = 0,	/* [sic] */
	ElfAbiHPUX,
	ElfAbiNetBSD,
	ElfAbiLinux,
	ElfAbiSolaris = 6,
	ElfAbiAix,
	ElfAbiIrix,
	ElfAbiFreeBSD,
	ElfAbiTru64,
	ElfAbiModesto,
	ElfAbiOpenBSD,
	ElfAbiARM = 97,
	ElfAbiEmbedded = 255,

	/* some of sections 0xFF00 - 0xFFFF reserved for various things */
	ElfSectNone = 0,
	ElfSectProgbits,
	ElfSectSymtab,
	ElfSectStrtab,
	ElfSectRela,
	ElfSectHash,
	ElfSectDynamic,
	ElfSectNote,
	ElfSectNobits,
	ElfSectRel,
	ElfSectShlib,
	ElfSectDynsym,

	ElfSectFlagWrite = 0x1,
	ElfSectFlagAlloc = 0x2,
	ElfSectFlagExec = 0x4,
	/* 0xF0000000 are reserved for processor specific */

	ElfSymBindLocal = 0,
	ElfSymBindGlobal,
	ElfSymBindWeak,
	/* 13-15 reserved */

	ElfSymTypeNone = 0,
	ElfSymTypeObject,
	ElfSymTypeFunc,
	ElfSymTypeSection,
	ElfSymTypeFile,
	/* 13-15 reserved */

	ElfSymShnNone = 0,
	ElfSymShnAbs = 0xFFF1,
	ElfSymShnCommon = 0xFFF2,
	/* 0xFF00-0xFF1F reserved for processors */
	/* 0xFF20-0xFF3F reserved for operating systems */

	ElfProgNone = 0,
	ElfProgLoad,
	ElfProgDynamic,
	ElfProgInterp,
	ElfProgNote,
	ElfProgShlib,
	ElfProgPhdr,

	ElfProgFlagExec = 0x1,
	ElfProgFlagWrite = 0x2,
	ElfProgFlagRead = 0x4,

	ElfNotePrStatus = 1,
	ElfNotePrFpreg = 2,
	ElfNotePrPsinfo = 3,
	ElfNotePrTaskstruct = 4,
	ElfNotePrAuxv = 6,
	ElfNotePrXfpreg = 0x46e62b7f	/* for gdb/386 */
};

struct ElfHdr
{
	uchar	magic[4];
	uchar	class;
	uchar	encoding;
	uchar	version;
	uchar	abi;
	uchar	abiversion;
	u32int	type;
	u32int	machine;
	u64int	entry;
	u64int	phoff;
	u64int	shoff;
	u32int	flags;
	u32int	ehsize;
	u32int	phentsize;
	u32int	phnum;
	u32int	shentsize;
	u32int	shnum;
	u32int	shstrndx;
	u16int	(*e2)(uchar*);
	u32int	(*e4)(uchar*);
	u64int	(*e8)(uchar*);
};

struct ElfSect
{
	char		*name;
	u32int	type;
	u64int	flags;
	u64int	addr;
	u64int	offset;
	u64int	size;
	u32int	link;
	u32int	info;
	u64int	align;
	u64int	entsize;
	uchar	*base;
};

struct ElfProg
{
	u32int	type;
	u64int	offset;
	u64int	vaddr;
	u64int	paddr;
	u64int	filesz;
	u64int	memsz;
	u32int	flags;
	u64int	align;
};

struct ElfNote
{
	u32int	namesz;
	u32int	descsz;
	u32int	type;
	char	*name;
	uchar	*desc;
	u32int	offset;	/* in-memory only */
};

struct ElfSym
{
	char*	name;
	u64int	value;
	u64int	size;
	uchar	bind;
	uchar	type;
	uchar	other;
	u16int	shndx;
};

struct Elf
{
	int		fd;
	ElfHdr	hdr;
	ElfSect	*sect;
	uint		nsect;
	ElfProg	*prog;
	uint		nprog;
	char		*shstrtab;

	int		nsymtab;
	ElfSect	*symtab;
	ElfSect	*symstr;
	int		ndynsym;
	ElfSect	*dynsym;
	ElfSect	*dynstr;
	ElfSect	*bss;
	ulong	dynamic;		/* offset to elf dynamic crap */

	int		(*coreregs)(Elf*, ElfNote*, uchar**);
	int		(*corecmd)(Elf*, ElfNote*, char**);
};

Elf*	elfopen(char*);
Elf*	elfinit(int);
ElfSect *elfsection(Elf*, char*);
void	elfclose(Elf*);
int	elfsym(Elf*, int, ElfSym*);
int	elfsymlookup(Elf*, char*, ulong*);
int	elfmap(Elf*, ElfSect*);

struct Fhdr;
void	elfcorelinux386(struct Fhdr*, Elf*, ElfNote*);
void	elfcorefreebsd386(struct Fhdr*, Elf*, ElfNote*);
void	elfcorefreebsdamd64(struct Fhdr*, Elf*, ElfNote*);
void	elfdl386mapdl(int);
