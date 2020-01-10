#include "a.h"

Sx *Brdsx1(Biobuf*);

Sx*
Brdsx(Biobuf *b)
{
	Sx **sx, *x;
	int nsx;

	nsx = 0;
	sx = nil;
	while((x = Brdsx1(b)) != nil){
		sx = erealloc(sx, (nsx+1)*sizeof sx[0]);
		sx[nsx++] = x;
	}
	x = emalloc(sizeof *x);
	x->sx = sx;
	x->nsx = nsx;
	x->type = SxList;
	return x;
}

int
sxwalk(Sx *sx)
{
	int i, n;

	if(sx == nil)
		return 1;
	switch(sx->type){
	default:
	case SxAtom:
	case SxString:
	case SxNumber:
		return 1;
	case SxList:
		n = 0;
		for(i=0; i<sx->nsx; i++)
			n += sxwalk(sx->sx[i]);
		return n;
	}
}

void
freesx(Sx *sx)
{
	int i;

	if(sx == nil)
		return;
	switch(sx->type){
	case SxAtom:
	case SxString:
		free(sx->data);
		break;
	case SxList:
		for(i=0; i<sx->nsx; i++)
			freesx(sx->sx[i]);
		free(sx->sx);
		break;
	}
	free(sx);
}

Sx*
Brdsx1(Biobuf *b)
{
	int c, len, nbr;
	char *s;
	vlong n;
	Sx *x;

	c = Bgetc(b);
	if(c == ' ')
		c = Bgetc(b);
	if(c < 0)
		return nil;
	if(c == '\r')
		c = Bgetc(b);
	if(c == '\n')
		return nil;
	if(c == ')'){	/* end of list */
		Bungetc(b);
		return nil;
	}
	if(c == '('){	/* parenthesized list */
		x = Brdsx(b);
		c = Bgetc(b);
		if(c != ')')	/* oops! not good */
			Bungetc(b);
		return x;
	}
	if(c == '{'){	/* length-prefixed string */
		len = 0;
		while((c = Bgetc(b)) >= 0 && isdigit(c))
			len = len*10 + c-'0';
		if(c != '}')	/* oops! not good */
			Bungetc(b);
		c = Bgetc(b);
		if(c != '\r')	/* oops! not good */
			;
		c = Bgetc(b);
		if(c != '\n')	/* oops! not good */
			;
		x = emalloc(sizeof *x);
		x->data = emalloc(len+1);
		if(Bread(b, x->data, len) != len)
			;	/* oops! */
		x->data[len] = 0;
		x->ndata = len;
		x->type = SxString;
		return x;
	}
	if(c == '"'){	/* quoted string */
		s = nil;
		len = 0;
		while((c = Bgetc(b)) >= 0 && c != '"'){
			if(c == '\\')
				c = Bgetc(b);
			s = erealloc(s, len+1);
			s[len++] = c;
		}
		s = erealloc(s, len+1);
		s[len] = 0;
		x = emalloc(sizeof *x);
		x->data = s;
		x->ndata = len;
		x->type = SxString;
		return x;
	}
	if(isdigit(c)){	/* number */
		n = c-'0';;
		while((c = Bgetc(b)) >= 0 && isdigit(c))
			n = n*10 + c-'0';
		Bungetc(b);
		x = emalloc(sizeof *x);
		x->number = n;
		x->type = SxNumber;
		return x;
	}
	/* atom */
	len = 1;
	s = emalloc(1);
	s[0] = c;
	nbr = 0;
	while((c = Bgetc(b)) >= 0 && c > ' ' && !strchr("(){}", c)){
		/* allow embedded brackets as in BODY[] */
		if(c == '['){
			if(s[0] == '[')
				break;
			else
				nbr++;
		}
		if(c == ']'){
			if(nbr > 0)
				nbr--;
			else
				break;
		}
		s = erealloc(s, len+1);
		s[len++] = c;
	}
	if(c != ' ')
		Bungetc(b);
	s = erealloc(s, len+1);
	s[len] = 0;
	x = emalloc(sizeof *x);
	x->type = SxAtom;
	x->data = s;
	x->ndata = len;
	return x;
}

int
sxfmt(Fmt *fmt)
{
	int i, paren;
	Sx *sx;

	sx = va_arg(fmt->args, Sx*);
	if(sx == nil)
		return 0;

	switch(sx->type){
	case SxAtom:
	case SxString:
		return fmtprint(fmt, "%q", sx->data);

	case SxNumber:
		return fmtprint(fmt, "%lld", sx->number);

	case SxList:
		paren = !(fmt->flags&FmtSharp);
		if(paren)
			fmtrune(fmt, '(');
		for(i=0; i<sx->nsx; i++){
			if(i)
				fmtrune(fmt, ' ');
			fmtprint(fmt, "%$", sx->sx[i]);
		}
		if(paren)
			return fmtrune(fmt, ')');
		return 0;

	default:
		return fmtstrcpy(fmt, "?");
	}
}

int
oksx(Sx *sx)
{
	return sx->nsx >= 2
		&& sx->sx[1]->type == SxAtom
		&& cistrcmp(sx->sx[1]->data, "OK") == 0;
}
