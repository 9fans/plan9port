/*
 * setjmp and longjmp, but our own because some (stupid) c libraries
 * assume longjmp is only used to move up the stack, and error out
 * if you do otherwise.
 */

typedef struct Label Label;
#define LABELDPC 0

#if defined (__i386__) && (defined(__FreeBSD__) || defined(__linux__))
struct Label
{
	ulong pc;
	ulong bx;
	ulong sp;
	ulong bp;
	ulong si;
	ulong di;
};
#else
#error "Unknown or unsupported architecture"
#endif


