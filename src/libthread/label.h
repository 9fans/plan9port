/*
 * setjmp and longjmp, but our own because some (stupid) c libraries
 * assume longjmp is only used to move up the stack, and error out
 * if you do otherwise.
 */

typedef struct Label Label;
#define LABELDPC 0

#if defined (__i386__) && (defined(__FreeBSD__) || defined(__linux__) || defined(__OpenBSD__))
struct Label
{
	ulong pc;
	ulong bx;
	ulong sp;
	ulong bp;
	ulong si;
	ulong di;
};
#elif defined(__APPLE__)
struct Label
{
	ulong	pc;		/* lr */
	ulong	cr;		/* mfcr */
	ulong	ctr;		/* mfcr */
	ulong	xer;		/* mfcr */
	ulong	sp;		/* callee saved: r1 */
	ulong	toc;		/* callee saved: r2 */
	ulong	gpr[19];	/* callee saved: r13-r31 */
// XXX: currently do not save vector registers or floating-point state
//	ulong	pad;
//	uvlong	fpr[18];	/* callee saved: f14-f31 */
//	ulong	vr[4*12];	/* callee saved: v20-v31, 256-bits each */
};
#elif defined(__sun__)
struct Label
{
	ulong	input[8];	/* %i registers */
	ulong	local[8];	/* %l registers */
	ulong	sp;		/* %o6 */
	ulong	link;		/* %o7 */
};
#elif defined(__powerpc__)
struct Label
{
	ulong	pc;		/* lr */
	ulong	cr;		/* mfcr */
	ulong	ctr;		/* mfcr */
	ulong	xer;		/* mfcr */
	ulong	sp;		/* callee saved: r1 */
	ulong	toc;		/* callee saved: r2 */
	ulong	gpr[19];	/* callee saved: r13-r31 */
// XXX: currently do not save vector registers or floating-point state
//	ulong	pad;
//	uvlong	fpr[18];	/* callee saved: f14-f31 */
//	ulong	vr[4*12];	/* callee saved: v20-v31, 256-bits each */
};
#else
#error "Unknown or unsupported architecture"
#endif


