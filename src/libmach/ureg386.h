typedef struct Ureg Ureg;
struct Ureg
{
	ulong	di;		/* general registers */
	ulong	si;		/* ... */
	ulong	bp;		/* ... */
	ulong	nsp;
	ulong	bx;		/* ... */
	ulong	dx;		/* ... */
	ulong	cx;		/* ... */
	ulong	ax;		/* ... */
	ulong	gs;		/* data segments */
	ulong	fs;		/* ... */
	ulong	es;		/* ... */
	ulong	ds;		/* ... */
	ulong	trap;		/* trap type */
	ulong	ecode;		/* error code (or zero) */
	ulong	pc;		/* pc */
	ulong	cs;		/* old context */
	ulong	flags;		/* old flags */
	ulong	sp;
	ulong	ss;		/* old stack segment */
};

typedef struct UregLinux386 UregLinux386;
struct UregLinux386
{
	ulong	ebx;
	ulong	ecx;
	ulong	edx;
	ulong	esi;
	ulong	ebp;
	ulong	eax;
	ulong	xds;
	ulong	xes;
	ulong	xfs;
	ulong	xgs;
	ulong	origeax;
	ulong	eip;
	ulong	xcs;
	ulong	eflags;
	ulong	esp;
	ulong	xss;
};

