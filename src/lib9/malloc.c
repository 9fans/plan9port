#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

/*
 * The Unix libc routines cannot be trusted to do their own locking.
 * Sad but apparently true.
 */

static Lock malloclock;

void*
p9malloc(ulong n)
{
	void *v;
	if(n == 0)
		n++;
//fprint(2, "%s %d malloc\n", argv0, getpid());
	lock(&malloclock);
	v = malloc(n);
	unlock(&malloclock);
//fprint(2, "%s %d donemalloc\n", argv0, getpid());
	return v;
}

void
p9free(void *v)
{
//fprint(2, "%s %d free\n", argv0, getpid());
	lock(&malloclock);
	free(v);
	unlock(&malloclock);
//fprint(2, "%s %d donefree\n", argv0, getpid());
}

void*
p9calloc(ulong a, ulong b)
{
	void *v;

//fprint(2, "%s %d calloc\n", argv0, getpid());
	lock(&malloclock);
	v = calloc(a, b);
	unlock(&malloclock);
//fprint(2, "%s %d donecalloc\n", argv0, getpid());
	return v;
}

void*
p9realloc(void *v, ulong n)
{
//fprint(2, "%s %d realloc\n", argv0, getpid());
	lock(&malloclock);
	v = realloc(v, n);
	unlock(&malloclock);
//fprint(2, "%s %d donerealloc\n", argv0, getpid());
	return v;
}
