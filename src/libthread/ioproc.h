#define ioproc_arg(io, type)	(va_arg((io)->arg, type))

struct Ioproc
{
	int tid;
	Channel *c, *creply;
	int inuse;
	long (*op)(va_list*);
	va_list arg;
	long ret;
	char err[ERRMAX];
	Ioproc *next;
};
