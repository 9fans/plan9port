/* Copyright (c) 2002, 2003 William Josephson */

enum {
	CoremapMagic	= 0xba5eba11,
	CoremapMax	= 128,
};
#undef MAXCOMLEN
#define MAXCOMLEN 16
#define PRSTATUS_VERSION	1	/* Current version of prstatus_t */
#define PRPSINFO_VERSION	1	/* Current version of prpsinfo_t */
#define PRARGSZ			80	/* Maximum argument bytes saved */


typedef struct Coremap Coremap;
typedef struct CoremapItem CoremapItem;
typedef struct CoremapHeader CoremapHeader;
typedef struct ElfNote ElfNote;
typedef struct Reg386 Reg386;
typedef struct PrStatus386 PrStatus386;
typedef struct PrPsinfo PrPsinfo;

struct CoremapHeader {
	u32int	magic;
	u32int	counter;
	u32int	maxelem;
};

struct CoremapItem {
	u32int	address;
	u32int	size;
};

struct Coremap {
	CoremapHeader	header;
	CoremapItem	map[CoremapMax];
};

struct ElfNote {
	u32int	namesz;
	u32int	descsz;
	u32int	type;
	char	*name;
	uchar	*desc;
	u32int	offset;	/* in-memory only */
};

enum
{
	NotePrStatus = 1,
	NotePrFpreg = 2,
	NotePrPsinfo = 3,
	NotePrTaskstruct = 4,
	NotePrAuxv = 6,
	NotePrXfpreg = 0x46e62b7f,	/* according to gdb */
};
#if 0
struct Reg386
{
	u32int	fs;
	u32int	es;
	u32int	ds;
	u32int	edi;
	u32int	esi;
	u32int	ebp;
	u32int	isp;
	u32int	ebx;
	u32int	edx;
	u32int	ecx;
	u32int	eax;
	u32int	trapno;
	u32int	err;
	u32int	eip;
	u32int	cs;
	u32int	eflags;
	u32int	esp;
	u32int	ss;
	u32int	gs;
};
#endif

struct Reg386
{
	u32int	ebx;
	u32int	ecx;
	u32int	edx;
	u32int	esi;
	u32int	edi;
	u32int	ebp;
	u32int	eax;
	u32int	ds;
	u32int	es;
	u32int	fs;
	u32int	gs;
	u32int	origeax;
	u32int	eip;
	u32int	cs;
	u32int	eflags;
	u32int	esp;
	u32int	ss;
};

#if 0
struct PrStatus386
{
    u32int		version;	/* Version number of struct (1) */
    u32int		statussz;	/* sizeof(prstatus_t) (1) */
    u32int		gregsetsz;	/* sizeof(gregset_t) (1) */
    u32int		fpregsetsz;	/* sizeof(fpregset_t) (1) */
    int			osreldate;	/* Kernel version (1) */
    int			cursig;	/* Current signal (1) */
    pid_t		pid;		/* Process ID (1) */
    Reg386		reg;		/* General purpose registers (1) */
};
#endif

struct PrPsinfo
{
    int		version;		/* Version number of struct (1) */
    u32int	psinfosz;		/* sizeof(prpsinfo_t) (1) */
    char	fname[MAXCOMLEN+1];	/* Command name, null terminated (1) */
    char	psargs[PRARGSZ+1];	/* Arguments, null terminated (1) */
};

struct PrStatus386
{
	u32int	signo;
	u32int	code;
	u32int	errno;
	u32int	cursig;
	u32int	sigpend;
	u32int	sighold;
	u32int	pid;
	u32int	ppid;
	u32int	pgrp;
	u32int	sid;
	u32int	utime[2];
	u32int	stime[2];
	u32int	cutime[2];
	u32int	cstime[2];
	Reg386	reg;
	u32int	fpvalid;
};
