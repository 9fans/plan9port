#include <u.h>
#include <libc.h>

typedef struct Range Range;
struct Range {
	uint	q0;
	uint	q1;
};

void
emphpush(Range **m, int *n, int *a, uint q0, uint q1)
{
	/*
	 * Stub: does nothing.
	 * Tests will fail because n remains 0 and no ranges are added.
	 */
	USED(m);
	USED(n);
	USED(a);
	USED(q0);
	USED(q1);
}

void
rangeshift(Range *m, int *n, uint q, int delta)
{
	/*
	 * Stub: does nothing.
	 * Tests will fail because ranges are not modified.
	 */
	USED(m);
	USED(n);
	USED(q);
	USED(delta);
}

void
rangemerge(Range **dst, int *ndst, int *adst, Range *src, int nsrc)
{
	/*
	 * Stub: does nothing.
	 * Tests will fail because ranges are not merged.
	 */
	USED(dst);
	USED(ndst);
	USED(adst);
	USED(src);
	USED(nsrc);
}

void
emphfreearr(Range **m, int *n, int *a)
{
	/*
	 * Stub: does nothing.
	 * Tests will fail because arrays are not freed.
	 */
	USED(m);
	USED(n);
	USED(a);
}
