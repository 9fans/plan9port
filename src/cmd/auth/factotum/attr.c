#include "std.h"
#include "dat.h"

Attr*
addattr(Attr *a, char *fmt, ...)
{
	char buf[8192];
	va_list arg;
	Attr *b;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof buf, fmt, arg);
	va_end(arg);
	b = _parseattr(buf);
	a = addattrs(a, b);
	setmalloctag(a, getcallerpc(&a));
	_freeattr(b);
	return a;
}

/*
 *  add attributes in list b to list a.  If any attributes are in
 *  both lists, replace those in a by those in b.
 */
Attr*
addattrs(Attr *a, Attr *b)
{
	int found;
	Attr **l, *aa;

	for(; b; b=b->next){
		switch(b->type){
		case AttrNameval:
			for(l=&a; *l; ){
				if(strcmp((*l)->name, b->name) != 0){
					l=&(*l)->next;
					continue;
				}
				aa = *l;
				*l = aa->next;
				aa->next = nil;
				freeattr(aa);
			}
			*l = mkattr(AttrNameval, b->name, b->val, nil);
			break;
		case AttrQuery:
			found = 0;
			for(l=&a; *l; l=&(*l)->next)
				if((*l)->type==AttrNameval && strcmp((*l)->name, b->name) == 0)
					found++;
			if(!found)
				*l = mkattr(AttrQuery, b->name, b->val, nil);
			break;
		}
	}
	return a;
}

void
setmalloctaghere(void *v)
{
	setmalloctag(v, getcallerpc(&v));
}

Attr*
sortattr(Attr *a)
{
	int i;
	Attr *anext, *a0, *a1, **l;

	if(a == nil || a->next == nil)
		return a;

	/* cut list in halves */
	a0 = nil;
	a1 = nil;
	i = 0;
	for(; a; a=anext){
		anext = a->next;
		if(i++%2){
			a->next = a0;
			a0 = a;
		}else{
			a->next = a1;
			a1 = a;
		}
	}

	/* sort */
	a0 = sortattr(a0);
	a1 = sortattr(a1);

	/* merge */
	l = &a;
	while(a0 || a1){
		if(a1==nil){
			anext = a0;
			a0 = a0->next;
		}else if(a0==nil){
			anext = a1;
			a1 = a1->next;
		}else if(strcmp(a0->name, a1->name) < 0){
			anext = a0;
			a0 = a0->next;
		}else{
			anext = a1;
			a1 = a1->next;
		}
		*l = anext;
		l = &(*l)->next;
	}
	*l = nil;
	return a;
}

int
attrnamefmt(Fmt *fmt)
{
	char *b, buf[8192], *ebuf;
	Attr *a;

	ebuf = buf+sizeof buf;
	b = buf;
	strcpy(buf, " ");
	for(a=va_arg(fmt->args, Attr*); a; a=a->next){
		if(a->name == nil)
			continue;
		b = seprint(b, ebuf, " %q?", a->name);
	}
	return fmtstrcpy(fmt, buf+1);
}

/*
static int
hasqueries(Attr *a)
{
	for(; a; a=a->next)
		if(a->type == AttrQuery)
			return 1;
	return 0;
}
*/

char *ignored[] = {
	"role",
	"disabled"
};

static int
ignoreattr(char *s)
{
	int i;

	for(i=0; i<nelem(ignored); i++)
		if(strcmp(ignored[i], s)==0)
			return 1;
	return 0;
}

static int
hasname(Attr *a0, Attr *a1, char *name)
{
	return _findattr(a0, name) || _findattr(a1, name);
}

static int
hasnameval(Attr *a0, Attr *a1, char *name, char *val)
{
	Attr *a;

	for(a=_findattr(a0, name); a; a=_findattr(a->next, name))
		if(strcmp(a->val, val) == 0)
			return 1;
	for(a=_findattr(a1, name); a; a=_findattr(a->next, name))
		if(strcmp(a->val, val) == 0)
			return 1;
	return 0;
}

int
matchattr(Attr *pat, Attr *a0, Attr *a1)
{
	int type;

	for(; pat; pat=pat->next){
		type = pat->type;
		if(ignoreattr(pat->name))
			type = AttrDefault;
		switch(type){
		case AttrQuery:		/* name=something be present */
			if(!hasname(a0, a1, pat->name))
				return 0;
			break;
		case AttrNameval:	/* name=val must be present */
			if(!hasnameval(a0, a1, pat->name, pat->val))
				return 0;
			break;
		case AttrDefault:	/* name=val must be present if name=anything is present */
			if(hasname(a0, a1, pat->name) && !hasnameval(a0, a1, pat->name, pat->val))
				return 0;
			break;
		}
	}
	return 1;
}

Attr*
parseattrfmtv(char *fmt, va_list arg)
{
	char *s;
	Attr *a;

	s = vsmprint(fmt, arg);
	if(s == nil)
		sysfatal("vsmprint: out of memory");
	a = parseattr(s);
	free(s);
	return a;
}

Attr*
parseattrfmt(char *fmt, ...)
{
	va_list arg;
	Attr *a;

	va_start(arg, fmt);
	a = parseattrfmtv(fmt, arg);
	va_end(arg);
	return a;
}
