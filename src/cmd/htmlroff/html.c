/*
 * Emit html.  Keep track of tags so that user doesn't have to.
 */

#include "a.h"

typedef struct Tag Tag;
struct Tag
{
	Tag *next;
	Rune *id;
	Rune *open;
	Rune *close;
};

Tag *tagstack;
Tag *tagset;
int hidingset;

static Rune*
closingtag(Rune *s)
{
	Rune *t;
	Rune *p0, *p;

	t = runemalloc(sizeof(Rune));
	if(s == nil)
		return t;
	for(p=s; *p; p++){
		if(*p == Ult){
			p++;
			if(*p == '/'){
				while(*p && *p != Ugt)
					p++;
				goto close;
			}
			p0 = p;
			while(*p && !isspacerune(*p) && *p != Uspace && *p != Ugt)
				p++;
			t = runerealloc(t, 1+(p-p0)+2+runestrlen(t)+1);
			runemove(t+(p-p0)+3, t, runestrlen(t)+1);
			t[0] = Ult;
			t[1] = '/';
			runemove(t+2, p0, p-p0);
			t[2+(p-p0)] = Ugt;
		}

		if(*p == Ugt && p>s && *(p-1) == '/'){
		close:
			for(p0=t+1; *p0 && *p0 != Ult; p0++)
				;
			runemove(t, p0, runestrlen(p0)+1);
		}
	}
	return t;
}

void
html(Rune *id, Rune *s)
{
	Rune *es;
	Tag *t, *tt, *next;

	br();
	hideihtml();	/* br already did, but be paranoid */
	for(t=tagstack; t; t=t->next){
		if(runestrcmp(t->id, id) == 0){
			for(tt=tagstack;; tt=next){
				next = tt->next;
				free(tt->id);
				free(tt->open);
				out(tt->close);
				outrune('\n');
				free(tt->close);
				free(tt);
				if(tt == t){
					tagstack = next;
					goto cleared;
				}
			}
		}
	}

cleared:
	if(s == nil || s[0] == 0)
		return;
	out(s);
	outrune('\n');
	es = closingtag(s);
	if(es[0] == 0){
		free(es);
		return;
	}
	if(runestrcmp(id, L("-")) == 0){
		out(es);
		outrune('\n');
		free(es);
		return;
	}
	t = emalloc(sizeof *t);
	t->id = erunestrdup(id);
	t->close = es;
	t->next = tagstack;
	tagstack = t;
}

void
closehtml(void)
{
	Tag *t, *next;

	br();
	hideihtml();
	for(t=tagstack; t; t=next){
		next = t->next;
		out(t->close);
		outrune('\n');
		free(t->id);
		free(t->close);
		free(t);
	}
}

static void
rshow(Tag *t, Tag *end)
{
	if(t == nil || t == end)
		return;
	rshow(t->next, end);
	out(t->open);
}

void
ihtml(Rune *id, Rune *s)
{
	Tag *t, *tt, **l;

	for(t=tagset; t; t=t->next){
		if(runestrcmp(t->id, id) == 0){
			if(s && t->open && runestrcmp(t->open, s) == 0)
				return;
			for(l=&tagset; (tt=*l); l=&tt->next){
				if(!hidingset)
					out(tt->close);
				if(tt == t)
					break;
			}
			*l = t->next;
			free(t->id);
			free(t->close);
			free(t->open);
			free(t);
			if(!hidingset)
				rshow(tagset, *l);
			goto cleared;
		}
	}

cleared:
	if(s == nil || s[0] == 0)
		return;
	t = emalloc(sizeof *t);
	t->id = erunestrdup(id);
	t->open = erunestrdup(s);
	t->close = closingtag(s);
	if(!hidingset)
		out(s);
	t->next = tagset;
	tagset = t;
}

void
hideihtml(void)
{
	Tag *t;

	if(hidingset)
		return;
	hidingset = 1;
	for(t=tagset; t; t=t->next)
		out(t->close);
}

void
showihtml(void)
{
	if(!hidingset)
		return;
	hidingset = 0;
	rshow(tagset, nil);
}

int
e_lt(void)
{
	return Ult;
}

int
e_gt(void)
{
	return Ugt;
}

int
e_at(void)
{
	return Uamp;
}

int
e_tick(void)
{
	return Utick;
}

int
e_btick(void)
{
	return Ubtick;
}

int
e_minus(void)
{
	return Uminus;
}

void
r_html(Rune *name)
{
	Rune *id, *line, *p;

	id = copyarg();
	line = readline(HtmlMode);
	for(p=line; *p; p++){
		switch(*p){
		case '<':
			*p = Ult;
			break;
		case '>':
			*p = Ugt;
			break;
		case '&':
			*p = Uamp;
			break;
		case ' ':
			*p = Uspace;
			break;
		}
	}
	if(name[0] == 'i')
		ihtml(id, line);
	else
		html(id, line);
	free(id);
	free(line);
}

char defaultfont[] =
	".ihtml f1\n"
	".ihtml f\n"
	".ihtml f <span style=\"font-size: \\n(.spt\">\n"
	".if \\n(.f==2 .ihtml f1 <i>\n"
	".if \\n(.f==3 .ihtml f1 <b>\n"
	".if \\n(.f==4 .ihtml f1 <b><i>\n"
	".if \\n(.f==5 .ihtml f1 <tt>\n"
	".if \\n(.f==6 .ihtml f1 <tt><i>\n"
	"..\n"
;

void
htmlinit(void)
{
	addraw(L("html"), r_html);
	addraw(L("ihtml"), r_html);

	addesc('<', e_lt, CopyMode);
	addesc('>', e_gt, CopyMode);
	addesc('\'', e_tick, CopyMode);
	addesc('`', e_btick, CopyMode);
	addesc('-', e_minus, CopyMode);
	addesc('@', e_at, CopyMode);

	ds(L("font"), L(defaultfont));
}
