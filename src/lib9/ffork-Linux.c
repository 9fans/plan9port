#define ffork ffork_clone
#define getfforkid getfforkid_clone
#include "ffork-Linux-clone.c"
#undef ffork
#undef getfforkid

#define ffork ffork_pthread
#define getfforkid getfforkid_pthread
#include "ffork-pthread.c"
#undef ffork
#undef getfforkid

extern int _islinuxnptl(void);

int
ffork(int flags, void (*fn)(void*), void *arg)
{
	if(_islinuxnptl())
		return ffork_pthread(flags, fn, arg);
	else
		return ffork_clone(flags, fn, arg);
}

int
getfforkid(void)
{
	if(_islinuxnptl())
		return getfforkid_pthread();
	else
		return getfforkid_clone();
}

