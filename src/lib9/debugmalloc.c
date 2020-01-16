#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

/*
 * The Unix libc routines cannot be trusted to do their own locking.
 * Sad but apparently true.
 */
static int mallocpid;

/*
 * The Unix mallocs don't do nearly enough error checking
 * for my tastes.  We'll waste another 24 bytes per guy so that
 * we can.  This is severely antisocial, since now free and p9free
 * are not interchangeable.
 */
int debugmalloc;

#define Overhead (debugmalloc ? (6*sizeof(ulong)) : 0)
#define MallocMagic 0xA110C09
#define ReallocMagic 0xB110C09
#define CallocMagic 0xC110C09
#define FreeMagic 0xF533F533
#define CheckMagic 0
#define END "\x7F\x2E\x55\x23"

static void
whoops(void *v)
{
	fprint(2, "bad malloc block %p\n", v);
	abort();
}

static void*
mark(void *v, ulong pc, ulong n, ulong magic)
{
	ulong *u;
	char *p;

	if(!debugmalloc)
		return v;

	if(v == nil)
		return nil;

	if(magic == FreeMagic || magic == CheckMagic){
		u = (ulong*)((char*)v-4*sizeof(ulong));
		if(u[0] != MallocMagic && u[0] != ReallocMagic && u[0] != CallocMagic)
			whoops(v);
		n = u[1];
		p = (char*)v+n;
		if(memcmp(p, END, 4) != 0)
			whoops(v);
		if(magic != CheckMagic){
			u[0] = FreeMagic;
			u[1] = u[2] = u[3] = pc;
			if(n > 16){
				u[4] = u[5] = u[6] = u[7] = pc;
				memset((char*)v+16, 0xFB, n-16);
			}
		}
		return u;
	}else{
		u = v;
		u[0] = magic;
		u[1] = n;
		u[2] = 0;
		u[3] = 0;
		if(magic == ReallocMagic)
			u[3] = pc;
		else
			u[2] = pc;
		p = (char*)(u+4)+n;
		memmove(p, END, 4);
		return u+4;
	}
}

void
setmalloctag(void *v, ulong t)
{
	ulong *u;

	if(!debugmalloc)
		return;

	if(v == nil)
		return;
	u = mark(v, 0, 0, 0);
	u[2] = t;
}

void
setrealloctag(void *v, ulong t)
{
	ulong *u;

	if(!debugmalloc)
		return;

	if(v == nil)
		return;
	u = mark(v, 0, 0, 0);
	u[3] = t;
}

void*
p9malloc(ulong n)
{
	void *v;
	if(n == 0)
		n++;
/*fprint(2, "%s %d malloc\n", argv0, getpid()); */
	mallocpid = getpid();
	v = malloc(n+Overhead);
	v = mark(v, getcallerpc(&n), n, MallocMagic);
/*fprint(2, "%s %d donemalloc\n", argv0, getpid()); */
	return v;
}

void
p9free(void *v)
{
	if(v == nil)
		return;

/*fprint(2, "%s %d free\n", argv0, getpid()); */
	mallocpid = getpid();
	v = mark(v, getcallerpc(&v), 0, FreeMagic);
	free(v);
/*fprint(2, "%s %d donefree\n", argv0, getpid()); */
}

void*
p9calloc(ulong a, ulong b)
{
	void *v;

/*fprint(2, "%s %d calloc\n", argv0, getpid()); */
	mallocpid = getpid();
	v = calloc(a*b+Overhead, 1);
	v = mark(v, getcallerpc(&a), a*b, CallocMagic);
/*fprint(2, "%s %d donecalloc\n", argv0, getpid()); */
	return v;
}

void*
p9realloc(void *v, ulong n)
{
/*fprint(2, "%s %d realloc\n", argv0, getpid()); */
	mallocpid = getpid();
	v = mark(v, getcallerpc(&v), 0, CheckMagic);
	v = realloc(v, n+Overhead);
	v = mark(v, getcallerpc(&v), n, ReallocMagic);
/*fprint(2, "%s %d donerealloc\n", argv0, getpid()); */
	return v;
}
