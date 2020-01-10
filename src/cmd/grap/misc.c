#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "grap.h"
#include "y.tab.h"

int	nnum	= 0;	/* number of saved numbers */
double	num[MAXNUM];

int	just;		/* current justification mode (RJUST, etc.) */
int	sizeop;		/* current optional operator for size change */
double	sizexpr;	/* current size change expression */

void savenum(int n, double f)	/* save f in num[n] */
{
	num[n] = f;
	nnum = n+1;
	if (nnum >= MAXNUM)
		ERROR "too many numbers" WARNING;
}

void setjust(int j)
{
	just |= j;
}

void setsize(int op, double expr)
{
	sizeop = op;
	sizexpr = expr;
}

char *tostring(char *s)
{
	register char *p;

	p = malloc(strlen(s)+1);
	if (p == NULL)
		ERROR "out of space in tostring on %s", s FATAL;
	strcpy(p, s);
	return(p);
}

void range(Point pt)	/* update the range for point pt */
{
	Obj *p = pt.obj;

	if (!(p->coord & XFLAG)) {
		if (pt.x > p->pt1.x)
			p->pt1.x = pt.x;
		if (pt.x < p->pt.x)
			p->pt.x = pt.x;
	}
	if (!(p->coord & YFLAG)) {
		if (pt.y > p->pt1.y)
			p->pt1.y = pt.y;
		if (pt.y < p->pt.y)
			p->pt.y = pt.y;
	}
}

void halfrange(Obj *p, int side, double val)	/* record max and min for one direction */
{
	if (!(p->coord&XFLAG) && (side == LEFT || side == RIGHT)) {
		if (val < p->pt.y)
			p->pt.y = val;
		if (val > p->pt1.y)
			p->pt1.y = val;
	} else if (!(p->coord&YFLAG) && (side == TOP || side == BOT)) {
		if (val < p->pt.x)
			p->pt.x = val;
		if (val > p->pt1.x)
			p->pt1.x = val;
	}
}


Obj *lookup(char *s, int inst)	/* find s in objlist, install if inst */
{
	Obj *p;
	int found = 0;

	for (p = objlist; p; p = p->next){
		if (strcmp(s, p->name) == 0) {
			found = 1;
			break;
		}
	}
	if (p == NULL && inst != 0) {
		p = (Obj *) calloc(1, sizeof(Obj));
		if (p == NULL)
			ERROR "out of space in lookup" FATAL;
		p->name = tostring(s);
		p->type = NAME;
		p->pt = ptmax;
		p->pt1 = ptmin;
		p->fval = 0.0;
		p->next = objlist;
		objlist = p;
	}
	dprintf("lookup(%s,%d) = %d\n", s, inst, found);
	return p;
}

double getvar(Obj *p)	/* return value of variable */
{
	return p->fval;
}

double setvar(Obj *p, double f)	/* set value of variable to f */
{
	if (strcmp(p->name, "pointsize") == 0) {	/* kludge */
		pointsize = f;
		ps_set = 1;
	}
	p->type = VARNAME;
	return p->fval = f;
}

Point makepoint(Obj *s, double x, double y)	/* make a Point */
{
	Point p;

	dprintf("makepoint: %s, %g,%g\n", s->name, x, y);
	p.obj = s;
	p.x = x;
	p.y = y;
	return p;
}

Attr *makefattr(int type, double fval)	/* set double in attribute */
{
	return makeattr(type, fval, (char *) 0, 0, 0);
}

Attr *makesattr(char *s)		/* make an Attr cell containing s */
{
	Attr *ap = makeattr(STRING, sizexpr, s, just, sizeop);
	just = sizeop = 0;
	sizexpr = 0.0;
	return ap;
}

Attr *makeattr(int type, double fval, char *sval, int just, int op)
{
	Attr *a;

	a = (Attr *) malloc(sizeof(Attr));
	if (a == NULL)
		ERROR "out of space in makeattr" FATAL;
	a->type = type;
	a->fval = fval;
	a->sval = sval;
	a->just = just;
	a->op = op;
	a->next = NULL;
	return a;
}

Attr *addattr(Attr *a1, Attr *ap)	/* add attr ap to end of list a1 */
{
	Attr *p;

	if (a1 == 0)
		return ap;
	if (ap == 0)
		return a1;
	for (p = a1; p->next; p = p->next)
		;
	p->next = ap;
	return a1;
}

void freeattr(Attr *ap)	/* free an attribute list */
{
	Attr *p;

	while (ap) {
		p = ap->next;	/* save next */
		if (ap->sval)
			free(ap->sval);
		free((char *) ap);
		ap = p;
	}
}

char *slprint(Attr *stringlist)	/* print strings from stringlist */
{
	int ntext, n, last_op, last_just;
	double last_fval;
	static char buf[1000];
	Attr *ap;

	buf[0] = '\0';
	last_op = last_just = 0;
	last_fval = 0.0;
	for (ntext = 0, ap = stringlist; ap != NULL; ap = ap->next)
		ntext++;
	sprintf(buf, "box invis wid 0 ht %d*textht", ntext);
	n = strlen(buf);
	for (ap = stringlist; ap != NULL; ap = ap->next) {
		if (ap->op == 0) {	/* propagate last value */
			ap->op = last_op;
			ap->fval = last_fval;
		} else {
			last_op = ap->op;
			last_fval = ap->fval;
		}
		sprintf(buf+n, " \"%s\"", ps_set || ap->op ? sizeit(ap) : ap->sval);
		if (ap->just)
			last_just = ap->just;
		if (last_just)
			strcat(buf+n, juststr(last_just));
		n = strlen(buf);
	}
	return buf;	/* watch it:  static */
}

char *juststr(int j)	/* convert RJUST, etc., into string */
{
	static char buf[50];

	buf[0] = '\0';
	if (j & RJUST)
		strcat(buf, " rjust");
	if (j & LJUST)
		strcat(buf, " ljust");
	if (j & ABOVE)
		strcat(buf, " above");
	if (j & BELOW)
		strcat(buf, " below");
	return buf;	/* watch it:  static */
}

char *sprntf(char *s, Attr *ap)	/* sprintf(s, attrlist ap) */
{
	char buf[500];
	int n;
	Attr *p;

	for (n = 0, p = ap; p; p = p->next)
		n++;
	switch (n) {
	case 0:
		return s;
	case 1:
		sprintf(buf, s, ap->fval);
		break;
	case 2:
		sprintf(buf, s, ap->fval, ap->next->fval);
		break;
	case 3:
		sprintf(buf, s, ap->fval, ap->next->fval, ap->next->next->fval);
		break;
	case 5:
		ERROR "too many expressions in sprintf" WARNING;
	case 4:
		sprintf(buf, s, ap->fval, ap->next->fval, ap->next->next->fval, ap->next->next->next->fval);
		break;
	}
	free(s);
	return tostring(buf);
}
