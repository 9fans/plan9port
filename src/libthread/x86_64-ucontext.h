#define	setcontext(u)	libthread_setmcontext(&(u)->mc)
#define	getcontext(u)	libthread_getmcontext(&(u)->mc)
typedef struct mcontext mcontext_t;
typedef struct ucontext ucontext_t;

struct mcontext
{
	uintptr	ax;
	uintptr	bx;
	uintptr cx;
	uintptr dx;
	uintptr si;
	uintptr di;
	uintptr	bp;
	uintptr sp;
	uintptr r8;
	uintptr r9;
	uintptr r10;
	uintptr r11;
	uintptr r12;
	uintptr r13;
	uintptr r14;
	uintptr r15;
/*
// XXX: currently do not save vector registers or floating-point state
*/
};

struct ucontext
{
	struct {
		void *ss_sp;
		uint ss_size;
	} uc_stack;
	sigset_t uc_sigmask;
	mcontext_t mc;
};

void makecontext(ucontext_t*, void(*)(void), int, ...);
int swapcontext(ucontext_t*, ucontext_t*);
int libthread_getmcontext(mcontext_t*);
void libthread_setmcontext(mcontext_t*);

