#define	setcontext(u)	_setmcontext(&(u)->mc)
#define	getcontext(u)	_getmcontext(&(u)->mc)
typedef struct mcontext mcontext_t;
typedef struct ucontext ucontext_t;
struct mcontext
{
	int	r[16];
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
int _getmcontext(mcontext_t*);
void _setmcontext(mcontext_t*);

