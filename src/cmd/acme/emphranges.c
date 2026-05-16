#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"

void
emphpush(Range **m, int *n, int *a, uint q0, uint q1)
{
	/* Grow by doubling when needed */
	if(*n >= *a) {
		*a = (*a == 0) ? 1 : *a * 2;
		*m = erealloc(*m, *a * sizeof(Range));
	}
	(*m)[*n].q0 = q0;
	(*m)[*n].q1 = q1;
	(*n)++;
}

void
rangeshift(Range *m, int *n, uint q, int delta)
{
	int i, j;
	uint dq0, dq1;

	j = 0;
	for(i = 0; i < *n; i++) {
		Range *r = &m[i];
		if(delta < 0) {
			/* deletion: dq0 to dq1 removed */
			dq0 = q;
			dq1 = q - delta;
			if(r->q1 <= dq0) {
				/* fully before deletion */
			} else if(r->q0 >= dq1) {
				/* fully after deletion - shift left */
				r->q0 += delta;
				r->q1 += delta;
			} else {
				/* overlaps deletion - drop */
				continue;
			}
		} else {
			/* insertion at q */
			if(r->q1 <= q) {
				/* fully before insertion */
			} else if(r->q0 >= q) {
				/* at or after insertion - shift right */
				r->q0 += delta;
				r->q1 += delta;
			} else {
				/* spans insertion - drop */
				continue;
			}
		}
		if(j != i)
			m[j] = *r;
		j++;
	}
	*n = j;
}

void
rangemerge(Range **dst, int *ndst, int *adst, Range *src, int nsrc)
{
	Range *tmp;
	int i, j, k, total;

	if(nsrc == 0)
		return;

	total = *ndst + nsrc;
	tmp = emalloc(total * sizeof(Range));

	/* Merge dst and src into tmp, maintaining sorted order */
	i = 0;  /* pointer into *dst */
	j = 0;  /* pointer into src */
	k = 0;  /* pointer into tmp */

	while(i < *ndst && j < nsrc) {
		if((*dst)[i].q0 <= src[j].q0) {
			tmp[k++] = (*dst)[i++];
		} else {
			tmp[k++] = src[j++];
		}
	}

	/* Copy remaining elements */
	while(i < *ndst)
		tmp[k++] = (*dst)[i++];
	while(j < nsrc)
		tmp[k++] = src[j++];

	/* Update capacity if needed and replace dst */
	while(*adst < total)
		*adst = (*adst == 0) ? 1 : *adst * 2;

	free(*dst);
	*dst = tmp;
	*ndst = total;
}

void
emphfreearr(Range **m, int *n, int *a)
{
	free(*m);
	*m = nil;
	*n = 0;
	*a = 0;
}
