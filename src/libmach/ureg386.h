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
	u32int	ebx;
	u32int	ecx;
	u32int	edx;
	u32int	esi;
	u32int	edi;
	u32int	ebp;
	u32int	eax;
	u32int	xds;
	u32int	xes;
	u32int	xfs;
	u32int	xgs;
	u32int	origeax;
	u32int	eip;
	u32int	xcs;
	u32int	eflags;
	u32int	esp;
	u32int	xss;
};

void linux2ureg386(UregLinux386*, Ureg*);
void ureg2linux386(Ureg*, UregLinux386*);
