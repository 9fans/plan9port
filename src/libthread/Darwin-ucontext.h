typedef struct ucontext ucontext_t;
struct ucontext
{
	ulong	pc;		/* lr */
	ulong	cr;		/* mfcr */
	ulong	ctr;		/* mfcr */
	ulong	xer;		/* mfcr */
	ulong	sp;		/* callee saved: r1 */
	ulong	toc;		/* callee saved: r2 */
	ulong	r3;		/* first arg to function, return register: r3 */
	ulong	gpr[19];	/* callee saved: r13-r31 */
// XXX: currently do not save vector registers or floating-point state
//	ulong	pad;
//	uvlong	fpr[18];	/* callee saved: f14-f31 */
//	ulong	vr[4*12];	/* callee saved: v20-v31, 256-bits each */
};

void makecontext(ucontext_t*, void(*)(void), int, ...);
void getcontext(ucontext_t*);
int setcontext(ucontext_t*);
int swapcontext(ucontext_t*, ucontext_t*);
int __setlabel(ucontext_t*);
void __gotolabel(ucontext_t*);

