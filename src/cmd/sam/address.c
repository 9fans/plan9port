#include "sam.h"
#include "parse.h"

Address	addr;
String	lastpat;
int	patset;
File	*menu;

File	*matchfile(String*);
Address	charaddr(Posn, Address, int);

Address
address(Addr *ap, Address a, int sign)
{
	File *f = a.f;
	Address a1, a2;

	do{
		switch(ap->type){
		case 'l':
		case '#':
			a = (*(ap->type=='#'?charaddr:lineaddr))(ap->num, a, sign);
			break;

		case '.':
			a = f->dot;
			break;

		case '$':
			a.r.p1 = a.r.p2 = f->b.nc;
			break;

		case '\'':
			a.r = f->mark;
			break;

		case '?':
			sign = -sign;
			if(sign == 0)
				sign = -1;
			/* fall through */
		case '/':
			nextmatch(f, ap->are, sign>=0? a.r.p2 : a.r.p1, sign);
			a.r = sel.p[0];
			break;

		case '"':
			a = matchfile(ap->are)->dot;
			f = a.f;
			if(f->unread)
				load(f);
			break;

		case '*':
			a.r.p1 = 0, a.r.p2 = f->b.nc;
			return a;

		case ',':
		case ';':
			if(ap->left)
				a1 = address(ap->left, a, 0);
			else
				a1.f = a.f, a1.r.p1 = a1.r.p2 = 0;
			if(ap->type == ';'){
				f = a1.f;
				a = a1;
				f->dot = a1;
			}
			if(ap->next)
				a2 = address(ap->next, a, 0);
			else
				a2.f = a.f, a2.r.p1 = a2.r.p2 = f->b.nc;
			if(a1.f != a2.f)
				error(Eorder);
			a.f = a1.f, a.r.p1 = a1.r.p1, a.r.p2 = a2.r.p2;
			if(a.r.p2 < a.r.p1)
				error(Eorder);
			return a;

		case '+':
		case '-':
			sign = 1;
			if(ap->type == '-')
				sign = -1;
			if(ap->next==0 || ap->next->type=='+' || ap->next->type=='-')
				a = lineaddr(1L, a, sign);
			break;
		default:
			panic("address");
			return a;
		}
	}while(ap = ap->next);	/* assign = */
	return a;
}

void
nextmatch(File *f, String *r, Posn p, int sign)
{
	compile(r);
	if(sign >= 0){
		if(!execute(f, p, INFINITY))
			error(Esearch);
		if(sel.p[0].p1==sel.p[0].p2 && sel.p[0].p1==p){
			if(++p>f->b.nc)
				p = 0;
			if(!execute(f, p, INFINITY))
				panic("address");
		}
	}else{
		if(!bexecute(f, p))
			error(Esearch);
		if(sel.p[0].p1==sel.p[0].p2 && sel.p[0].p2==p){
			if(--p<0)
				p = f->b.nc;
			if(!bexecute(f, p))
				panic("address");
		}
	}
}

File *
matchfile(String *r)
{
	File *f;
	File *match = 0;
	int i;

	for(i = 0; i<file.nused; i++){
		f = file.filepptr[i];
		if(f == cmd)
			continue;
		if(filematch(f, r)){
			if(match)
				error(Emanyfiles);
			match = f;
		}
	}
	if(!match)
		error(Efsearch);
	return match;
}

int
filematch(File *f, String *r)
{
	char *c, buf[STRSIZE+100];
	String *t;

	c = Strtoc(&f->name);
	sprint(buf, "%c%c%c %s\n", " '"[f->mod],
		"-+"[f->rasp!=0], " ."[f==curfile], c);
	free(c);
	t = tmpcstr(buf);
	Strduplstr(&genstr, t);
	freetmpstr(t);
	/* A little dirty... */
	if(menu == 0)
		menu = fileopen();
	bufreset(&menu->b);
	bufinsert(&menu->b, 0, genstr.s, genstr.n);
	compile(r);
	return execute(menu, 0, menu->b.nc);
}

Address
charaddr(Posn l, Address addr, int sign)
{
	if(sign == 0)
		addr.r.p1 = addr.r.p2 = l;
	else if(sign < 0)
		addr.r.p2 = addr.r.p1-=l;
	else if(sign > 0)
		addr.r.p1 = addr.r.p2+=l;
	if(addr.r.p1<0 || addr.r.p2>addr.f->b.nc)
		error(Erange);
	return addr;
}

Address
lineaddr(Posn l, Address addr, int sign)
{
	int n;
	int c;
	File *f = addr.f;
	Address a;
	Posn p;

	a.f = f;
	if(sign >= 0){
		if(l == 0){
			if(sign==0 || addr.r.p2==0){
				a.r.p1 = a.r.p2 = 0;
				return a;
			}
			a.r.p1 = addr.r.p2;
			p = addr.r.p2-1;
		}else{
			if(sign==0 || addr.r.p2==0){
				p = (Posn)0;
				n = 1;
			}else{
				p = addr.r.p2-1;
				n = filereadc(f, p++)=='\n';
			}
			while(n < l){
				if(p >= f->b.nc)
					error(Erange);
				if(filereadc(f, p++) == '\n')
					n++;
			}
			a.r.p1 = p;
		}
		while(p < f->b.nc && filereadc(f, p++)!='\n')
			;
		a.r.p2 = p;
	}else{
		p = addr.r.p1;
		if(l == 0)
			a.r.p2 = addr.r.p1;
		else{
			for(n = 0; n<l; ){	/* always runs once */
				if(p == 0){
					if(++n != l)
						error(Erange);
				}else{
					c = filereadc(f, p-1);
					if(c != '\n' || ++n != l)
						p--;
				}
			}
			a.r.p2 = p;
			if(p > 0)
				p--;
		}
		while(p > 0 && filereadc(f, p-1)!='\n')	/* lines start after a newline */
			p--;
		a.r.p1 = p;
	}
	return a;
}
