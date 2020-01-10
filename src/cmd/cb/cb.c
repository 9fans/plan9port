#include <u.h>
#include <libc.h>
#include <bio.h>
#include "cb.h"
#include "cbtype.h"

void
main(int argc, char *argv[])
{
	Biobuf stdin, stdout;

	while (--argc > 0 && (*++argv)[0] == '-'){
		switch ((*argv)[1]){
		case 's':
			strict = 1;
			continue;
		case 'j':
			join = 1;
			continue;
		case 'l':
			if((*argv)[2] != '\0'){
				maxleng = atoi( &((*argv)[2]) );
			}
			else{
				maxleng = atoi(*++argv);
				argc--;
			}
			maxtabs = maxleng/TABLENG - 2;
			maxleng -= (maxleng + 5)/10;
			continue;
		default:
			fprint(2, "cb: illegal option %c\n", (*argv)[1]);
			exits("boom");
		}
	}
	Binit(&stdout, 1, OWRITE);
	output = &stdout;
	if (argc <= 0){
		Binit(&stdin, 0, OREAD);
		input = &stdin;
		work();
	} else {
		while (argc-- > 0){
			if ((input = Bopen( *argv, OREAD)) == 0){
				fprint(2, "cb: cannot open input file %s\n", *argv);
				exits("boom");
			}
			work();
			argv++;
		}
	}
	exits(0);
}
void
work(void){
	int c;
	struct keyw *lptr;
	char *pt;
	int cc;
	int ct;

	while ((c = getch()) != Beof){
		switch (c){
		case '{':
			if ((lptr = lookup(lastlook,p)) != 0){
				if (lptr->type == ELSE)gotelse();
				else if(lptr->type == DO)gotdo();
				else if(lptr->type == STRUCT)structlev++;
			}
			if(++clev >= &ind[CLEVEL-1]){
				fprint(2,"too many levels of curly brackets\n");
				clev = &ind[CLEVEL-1];
			}
			clev->pdepth = 0;
			clev->tabs = (clev-1)->tabs;
			clearif(clev);
			if(strict && clev->tabs > 0)
				putspace(' ',NO);
			putch(c,NO);
			getnl();
			if(keyflag == DATADEF){
				OUT;
			}
			else {
				OUTK;
			}
			clev->tabs++;
			pt = getnext(0);		/* to handle initialized structures */
			if(*pt == '{'){		/* hide one level of {} */
				while((c=getch()) != '{')
					if(c == Beof)error("{");
				putch(c,NO);
				if(strict){
					putch(' ',NO);
					eatspace();
				}
				keyflag = SINIT;
			}
			continue;
		case '}':
			pt = getnext(0);		/* to handle initialized structures */
			if(*pt == ','){
				if(strict){
					putspace(' ',NO);
					eatspace();
				}
				putch(c,NO);
				putch(*pt,NO);
				*pt = '\0';
				ct = getnl();
				pt = getnext(0);
				if(*pt == '{'){
					OUT;
					while((cc = getch()) != '{')
						if(cc == Beof)error("}");
					putch(cc,NO);
					if(strict){
						putch(' ',NO);
						eatspace();
					}
					getnext(0);
					continue;
				}
				else if(strict || ct){
					OUT;
				}
				continue;
			}
			else if(keyflag == SINIT && *pt == '}'){
				if(strict)
					putspace(' ',NO);
				putch(c,NO);
				getnl();
				OUT;
				keyflag = DATADEF;
				*pt = '\0';
				pt = getnext(0);
			}
			outs(clev->tabs);
			if(--clev < ind)clev = ind;
			ptabs(clev->tabs);
			putch(c,NO);
			lbegin = 0;
			lptr=lookup(pt,lastplace+1);
			c = *pt;
			if(*pt == ';' || *pt == ','){
				putch(*pt,NO);
				*pt = '\0';
				lastplace=pt;
			}
			ct = getnl();
			if((dolevel && clev->tabs <= dotabs[dolevel]) || (structlev )
			    || (lptr != 0 &&lptr->type == ELSE&& clev->pdepth == 0)){
				if(c == ';'){
					OUTK;
				}
				else if(strict || (lptr != 0 && lptr->type == ELSE && ct == 0)){
					putspace(' ',NO);
					eatspace();
				}
				else if(lptr != 0 && lptr->type == ELSE){
					OUTK;
				}
				if(structlev){
					structlev--;
					keyflag = DATADEF;
				}
			}
			else {
				OUTK;
				if(strict && clev->tabs == 0){
					if((c=getch()) != '\n'){
						Bputc(output, '\n');
						Bputc(output, '\n');
						unget(c);
					}
					else {
						lineno++;
						Bputc(output, '\n');
						if((c=getch()) != '\n')unget(c);
						else lineno++;
						Bputc(output, '\n');
					}
				}
			}
			if(lptr != 0 && lptr->type == ELSE && clev->pdepth != 0){
				UNBUMP;
			}
			if(lptr == 0 || lptr->type != ELSE){
				clev->iflev = 0;
				if(dolevel && docurly[dolevel] == NO && clev->tabs == dotabs[dolevel]+1)
					clev->tabs--;
				else if(clev->pdepth != 0){
					UNBUMP;
				}
			}
			continue;
		case '(':
			paren++;
			if ((lptr = lookup(lastlook,p)) != 0){
				if(!(lptr->type == TYPE || lptr->type == STRUCT))keyflag=KEYWORD;
				if (strict){
					putspace(lptr->punc,NO);
					opflag = 1;
				}
				putch(c,NO);
				if (lptr->type == IF)gotif();
			}
			else {
				putch(c,NO);
				lastlook = p;
				opflag = 1;
			}
			continue;
		case ')':
			if(--paren < 0)paren = 0;
			putch(c,NO);
			if((lptr = lookup(lastlook,p)) != 0){
				if(lptr->type == TYPE || lptr->type == STRUCT)
					opflag = 1;
			}
			else if(keyflag == DATADEF)opflag = 1;
			else opflag = 0;
			outs(clev->tabs);
			pt = getnext(1);
			if ((ct = getnl()) == 1 && !strict){
				if(dolevel && clev->tabs <= dotabs[dolevel])
					resetdo();
				if(clev->tabs > 0 && (paren != 0 || keyflag == 0)){
					if(join){
						eatspace();
						putch(' ',YES);
						continue;
					} else {
						OUT;
						split = 1;
						continue;
					}
				}
				else if(clev->tabs > 0 && *pt != '{'){
					BUMP;
				}
				OUTK;
			}
			else if(strict){
				if(clev->tabs == 0){
					if(*pt != ';' && *pt != ',' && *pt != '(' && *pt != '['){
						OUTK;
					}
				}
				else {
					if(keyflag == KEYWORD && paren == 0){
						if(dolevel && clev->tabs <= dotabs[dolevel]){
							resetdo();
							eatspace();
							continue;
						}
						if(*pt != '{'){
							BUMP;
							OUTK;
						}
						else {
							*pt='\0';
							eatspace();
							unget('{');
						}
					}
					else if(ct){
						if(paren){
							if(join){
								eatspace();
							} else {
								split = 1;
								OUT;
							}
						}
						else {
							OUTK;
						}
					}
				}
			}
			else if(dolevel && clev->tabs <= dotabs[dolevel])
				resetdo();
			continue;
		case ' ':
		case '\t':
			if ((lptr = lookup(lastlook,p)) != 0){
				if(!(lptr->type==TYPE||lptr->type==STRUCT))
					keyflag = KEYWORD;
				else if(paren == 0)keyflag = DATADEF;
				if(strict){
					if(lptr->type != ELSE){
						if(lptr->type == TYPE){
							if(paren != 0)putch(' ',YES);
						}
						else
							putch(lptr->punc,NO);
						eatspace();
					}
				}
				else putch(c,YES);
				switch(lptr->type){
				case CASE:
					outs(clev->tabs-1);
					continue;
				case ELSE:
					pt = getnext(1);
					eatspace();
					if((cc = getch()) == '\n' && !strict){
						unget(cc);
					}
					else {
						unget(cc);
						if(checkif(pt))continue;
					}
					gotelse();
					if(strict) unget(c);
					if(getnl() == 1 && !strict){
						OUTK;
						if(*pt != '{'){
							BUMP;
						}
					}
					else if(strict){
						if(*pt != '{'){
							OUTK;
							BUMP;
						}
					}
					continue;
				case IF:
					gotif();
					continue;
				case DO:
					gotdo();
					pt = getnext(1);
					if(*pt != '{'){
						eatallsp();
						OUTK;
						docurly[dolevel] = NO;
						dopdepth[dolevel] = clev->pdepth;
						clev->pdepth = 0;
						clev->tabs++;
					}
					continue;
				case TYPE:
					if(paren)continue;
					if(!strict)continue;
					gottype(lptr);
					continue;
				case STRUCT:
					gotstruct();
					continue;
				}
			}
			else if (lbegin == 0 || p > string)
				if(strict)
					putch(c,NO);
				else putch(c,YES);
			continue;
		case ';':
			putch(c,NO);
			if(paren != 0){
				if(strict){
					putch(' ',YES);
					eatspace();
				}
				opflag = 1;
				continue;
			}
			outs(clev->tabs);
			pt = getnext(0);
			lptr=lookup(pt,lastplace+1);
			if(lptr == 0 || lptr->type != ELSE){
				clev->iflev = 0;
				if(clev->pdepth != 0){
					UNBUMP;
				}
				if(dolevel && docurly[dolevel] == NO && clev->tabs <= dotabs[dolevel]+1)
					clev->tabs--;
/*
				else if(clev->pdepth != 0){
					UNBUMP;
				}
*/
			}
			getnl();
			OUTK;
			continue;
		case '\n':
			if ((lptr = lookup(lastlook,p)) != 0){
				pt = getnext(1);
				if (lptr->type == ELSE){
					if(strict)
						if(checkif(pt))continue;
					gotelse();
					OUTK;
					if(*pt != '{'){
						BUMP;
					}
				}
				else if(lptr->type == DO){
					OUTK;
					gotdo();
					if(*pt != '{'){
						docurly[dolevel] = NO;
						dopdepth[dolevel] = clev->pdepth;
						clev->pdepth = 0;
						clev->tabs++;
					}
				}
				else {
					OUTK;
					if(lptr->type == STRUCT)gotstruct();
				}
			}
			else if(p == string)Bputc(output, '\n');
			else {
				if(clev->tabs > 0 &&(paren != 0 || keyflag == 0)){
					if(join){
						putch(' ',YES);
						eatspace();
						continue;
					} else {
						OUT;
						split = 1;
						continue;
					}
				}
				else if(keyflag == KEYWORD){
					OUTK;
					continue;
				}
				OUT;
			}
			continue;
		case '"':
		case '\'':
			putch(c,NO);
			while ((cc = getch()) != c){
				if(cc == Beof)
					error("\" or '");
				putch(cc,NO);
				if (cc == '\\'){
					putch(getch(),NO);
				}
				if (cc == '\n'){
					outs(clev->tabs);
					lbegin = 1;
					count = 0;
				}
			}
			putch(cc,NO);
			opflag=0;
			if (getnl() == 1){
				unget('\n');
			}
			continue;
		case '\\':
			putch(c,NO);
			putch(getch(),NO);
			continue;
		case '?':
			question = 1;
			gotop(c);
			continue;
		case ':':
			if (question == 1){
				question = 0;
				gotop(c);
				continue;
			}
			putch(c,NO);
			if(structlev)continue;
			if ((lptr = lookup(lastlook,p)) != 0){
				if (lptr->type == CASE)outs(clev->tabs - 1);
			}
			else {
				lbegin = 0;
				outs(clev->tabs);
			}
			getnl();
			OUTK;
			continue;
		case '/':
			if ((cc = getch()) == '/') {
				putch(c,NO);
				putch(cc,NO);
				cpp_comment(YES);
				OUT;
				lastlook = 0;
				continue;
			}
			else if (cc != '*') {
				unget(cc);
				gotop(c);
				continue;
			}
			putch(c,NO);
			putch(cc,NO);
			cc = comment(YES);
			if(getnl() == 1){
				if(cc == 0){
					OUT;
				}
				else {
					outs(0);
					Bputc(output, '\n');
					lbegin = 1;
					count = 0;
				}
				lastlook = 0;
			}
			continue;
		case '[':
			putch(c,NO);
			ct = 0;
			while((c = getch()) != ']' || ct > 0){
				if(c == Beof)error("]");
				putch(c,NO);
				if(c == '[')ct++;
				if(c == ']')ct--;
			}
			putch(c,NO);
			continue;
		case '#':
			putch(c,NO);
			while ((cc = getch()) != '\n'){
				if(cc == Beof)error("newline");
				if (cc == '\\'){
					putch(cc,NO);
					cc = getch();
				}
				putch(cc,NO);
			}
			putch(cc,NO);
			lbegin = 0;
			outs(clev->tabs);
			lbegin = 1;
			count = 0;
			continue;
		default:
			if (c == ','){
				opflag = 1;
				putch(c,YES);
				if (strict){
					if ((cc = getch()) != ' ')unget(cc);
					if(cc != '\n')putch(' ',YES);
				}
			}
			else if(isop(c))gotop(c);
			else {
				if(isalnum(c) && lastlook == 0)lastlook = p;
				if(isdigit(c)){
					putch(c,NO);
					while(isdigit(c=Bgetc(input))||c == '.')putch(c,NO);
					if(c == 'e'){
						putch(c,NO);
						c = Bgetc(input);
						putch(c, NO);
						while(isdigit(c=Bgetc(input)))putch(c,NO);
					}
					Bungetc(input);
				}
				else putch(c,NO);
				if(keyflag != DATADEF)opflag = 0;
			}
		}
	}
}
void
gotif(void){
	outs(clev->tabs);
	if(++clev->iflev >= IFLEVEL-1){
		fprint(2,"too many levels of if %d\n",clev->iflev );
		clev->iflev = IFLEVEL-1;
	}
	clev->ifc[clev->iflev] = clev->tabs;
	clev->spdepth[clev->iflev] = clev->pdepth;
}
void
gotelse(void){
	clev->tabs = clev->ifc[clev->iflev];
	clev->pdepth = clev->spdepth[clev->iflev];
	if(--(clev->iflev) < 0)clev->iflev = 0;
}
int
checkif(char *pt)
{
	struct keyw *lptr;
	int cc;
	if((lptr=lookup(pt,lastplace+1))!= 0){
		if(lptr->type == IF){
			if(strict)putch(' ',YES);
			copy(lptr->name);
			*pt='\0';
			lastplace = pt;
			if(strict){
				putch(lptr->punc,NO);
				eatallsp();
			}
			clev->tabs = clev->ifc[clev->iflev];
			clev->pdepth = clev->spdepth[clev->iflev];
			keyflag = KEYWORD;
			return(1);
		}
	}
	return(0);
}
void
gotdo(void){
	if(++dolevel >= DOLEVEL-1){
		fprint(2,"too many levels of do %d\n",dolevel);
		dolevel = DOLEVEL-1;
	}
	dotabs[dolevel] = clev->tabs;
	docurly[dolevel] = YES;
}
void
resetdo(void){
	if(docurly[dolevel] == NO)
		clev->pdepth = dopdepth[dolevel];
	if(--dolevel < 0)dolevel = 0;
}
void
gottype(struct keyw *lptr)
{
	char *pt;
	struct keyw *tlptr;
	int c;
	while(1){
		pt = getnext(1);
		if((tlptr=lookup(pt,lastplace+1))!=0){
			putch(' ',YES);
			copy(tlptr->name);
			*pt='\0';
			lastplace = pt;
			if(tlptr->type == STRUCT){
				putch(tlptr->punc,YES);
				gotstruct();
				break;
			}
			lptr=tlptr;
			continue;
		}
		else{
			putch(lptr->punc,NO);
			while((c=getch())== ' ' || c == '\t');
			unget(c);
			break;
		}
	}
}
void
gotstruct(void){
	int c;
	int cc;
	char *pt;
	while((c=getch()) == ' ' || c == '\t')
		if(!strict)putch(c,NO);
	if(c == '{'){
		structlev++;
		unget(c);
		return;
	}
	if(isalpha(c)){
		putch(c,NO);
		while(isalnum(c=getch()))putch(c,NO);
	}
	unget(c);
	pt = getnext(1);
	if(*pt == '{')structlev++;
	if(strict){
		eatallsp();
		putch(' ',NO);
	}
}
void
gotop(int c)
{
	char optmp[OPLENGTH];
	char *op_ptr;
	struct op *s_op;
	char *a, *b;
	op_ptr = optmp;
	*op_ptr++ = c;
	while (isop((uchar)( *op_ptr = getch())))op_ptr++;
	if(!strict)unget(*op_ptr);
	else if (*op_ptr != ' ')unget( *op_ptr);
	*op_ptr = '\0';
	s_op = op;
	b = optmp;
	while ((a = s_op->name) != 0){
		op_ptr = b;
		while ((*op_ptr == *a) && (*op_ptr != '\0')){
			a++;
			op_ptr++;
		}
		if (*a == '\0'){
			keep(s_op);
			opflag = s_op->setop;
			if (*op_ptr != '\0'){
				b = op_ptr;
				s_op = op;
				continue;
			}
			else break;
		}
		else s_op++;
	}
}
void
keep(struct op *o)
{
	char	*s;
	int ok;
	if(o->blanks == NEVER)ok = NO;
	else ok = YES;
	if (strict && ((o->blanks & ALWAYS)
	    || ((opflag == 0 && o->blanks & SOMETIMES) && clev->tabs != 0)))
		putspace(' ',YES);
	for(s=o->name; *s != '\0'; s++){
		if(*(s+1) == '\0')putch(*s,ok);
		else
			putch(*s,NO);
	}
	if (strict && ((o->blanks & ALWAYS)
	    || ((opflag == 0 && o->blanks & SOMETIMES) && clev->tabs != 0))) putch(' ',YES);
}
int
getnl(void){
	int ch;
	char *savp;
	int gotcmt;
	gotcmt = 0;
	savp = p;
	while ((ch = getch()) == '\t' || ch == ' ')putch(ch,NO);
	if (ch == '/'){
		if ((ch = getch()) == '*'){
			putch('/',NO);
			putch('*',NO);
			comment(NO);
			ch = getch();
			gotcmt=1;
		}
		else if (ch == '/') {
			putch('/',NO);
			putch('/',NO);
			cpp_comment(NO);
			ch = getch();
			gotcmt = 1;
		}
		else {
			if(inswitch)*(++lastplace) = ch;
			else {
				inswitch = 1;
				*lastplace = ch;
			}
			unget('/');
			return(0);
		}
	}
	if(ch == '\n'){
		if(gotcmt == 0)p=savp;
		return(1);
	}
	unget(ch);
	return(0);
}
void
ptabs(int n){
	int	i;
	int num;
	if(n > maxtabs){
		if(!folded){
			Bprint(output, "/* code folded from here */\n");
			folded = 1;
		}
		num = n-maxtabs;
	}
	else {
		num = n;
		if(folded){
			folded = 0;
			Bprint(output, "/* unfolding */\n");
		}
	}
	for (i = 0; i < num; i++)Bputc(output, '\t');
}
void
outs(int n){
	if (p > string){
		if (lbegin){
			ptabs(n);
			lbegin = 0;
			if (split == 1){
				split = 0;
				if (clev->tabs > 0)Bprint(output, "    ");
			}
		}
		*p = '\0';
		Bprint(output, "%s", string);
		lastlook = p = string;
	}
	else {
		if (lbegin != 0){
			lbegin = 0;
			split = 0;
		}
	}
}
void
putch(char c,int ok)
{
	int cc;
	if(p < &string[LINE-1]){
		if(count+TABLENG*clev->tabs >= maxleng && ok && !folded){
			if(c != ' ')*p++ = c;
			OUT;
			split = 1;
			if((cc=getch()) != '\n')unget(cc);
		}
		else {
			*p++ = c;
			count++;
		}
	}
	else {
		outs(clev->tabs);
		*p++ = c;
		count = 0;
	}
}
struct keyw *
lookup(char *first, char *last)
{
	struct keyw *ptr;
	char	*cptr, *ckey, *k;

	if(first == last || first == 0)return(0);
	cptr = first;
	while (*cptr == ' ' || *cptr == '\t')cptr++;
	if(cptr >= last)return(0);
	ptr = key;
	while ((ckey = ptr->name) != 0){
		for (k = cptr; (*ckey == *k && *ckey != '\0'); k++, ckey++);
		if(*ckey=='\0' && (k==last|| (k<last && !isalnum((uchar)*k)))){
			opflag = 1;
			lastlook = 0;
			return(ptr);
		}
		ptr++;
	}
	return(0);
}
int
comment(int ok)
{
	int ch;
	int hitnl;

	hitnl = 0;
	while ((ch  = getch()) != Beof){
		putch(ch, NO);
		if (ch == '*'){
gotstar:
			if ((ch  = getch()) == '/'){
				putch(ch,NO);
				return(hitnl);
			}
			putch(ch,NO);
			if (ch == '*')goto gotstar;
		}
		if (ch == '\n'){
			if(ok && !hitnl){
				outs(clev->tabs);
			}
			else {
				outs(0);
			}
			lbegin = 1;
			count = 0;
			hitnl = 1;
		}
	}
	return(hitnl);
}
int
cpp_comment(int ok)
{
	int ch;
	int hitnl;

	hitnl = 0;
	while ((ch = getch()) != -1) {
		if (ch == '\n') {
			if (ok && !hitnl)
				outs(clev->tabs);
			else
				outs(0);
			lbegin = 1;
			count = 0;
			hitnl = 1;
			break;
		}
		putch(ch, NO);
	}
	return hitnl;
}
void
putspace(char ch, int ok)
{
	if(p == string)putch(ch,ok);
	else if (*(p - 1) != ch) putch(ch,ok);
}
int
getch(void){
	char c;
	if(inswitch){
		if(next != '\0'){
			c=next;
			next = '\0';
			return(c);
		}
		if(tptr <= lastplace){
			if(*tptr != '\0')return(*tptr++);
			else if(++tptr <= lastplace)return(*tptr++);
		}
		inswitch=0;
		lastplace = tptr = temp;
	}
	return(Bgetc(input));
}
void
unget(char c)
{
	if(inswitch){
		if(tptr != temp)
			*(--tptr) = c;
		else next = c;
	}
	else Bungetc(input);
}
char *
getnext(int must){
	int c;
	char *beg;
	int prect,nlct;
	prect = nlct = 0;
	if(tptr > lastplace){
		tptr = lastplace = temp;
		err = 0;
		inswitch = 0;
	}
	tp = lastplace;
	if(inswitch && tptr <= lastplace)
		if (isalnum((uchar)*lastplace)||ispunct((uchar)*lastplace)||isop((uchar)*lastplace))return(lastplace);
space:
	while(isspace(c=Bgetc(input)))puttmp(c,1);
	beg = tp;
	puttmp(c,1);
	if(c == '/'){
		if(puttmp(Bgetc(input),1) == '*'){
cont:
			while((c=Bgetc(input)) != '*'){
				puttmp(c,0);
				if(must == 0 && c == '\n')
					if(nlct++ > 2)goto done;
			}
			puttmp(c,1);
star:
			if(puttmp((c=Bgetc(input)),1) == '/'){
				beg = tp;
				puttmp((c=Bgetc(input)),1);
			}
			else if(c == '*')goto star;
			else goto cont;
		}
		else goto done;
	}
	if(isspace(c))goto space;
	if(c == '#' && tp > temp+1 && *(tp-2) == '\n'){
		if(prect++ > 2)goto done;
		while(puttmp((c=Bgetc(input)),1) != '\n')
			if(c == '\\')puttmp(Bgetc(input),1);
		goto space;
	}
	if(isalnum(c)){
		while(isalnum(c = Bgetc(input)))puttmp(c,1);
		Bungetc(input);
	}
done:
	puttmp('\0',1);
	lastplace = tp-1;
	inswitch = 1;
	return(beg);
}
void
copy(char *s)
{
	while(*s != '\0')putch(*s++,NO);
}
void
clearif(struct indent *cl)
{
	int i;
	for(i=0;i<IFLEVEL-1;i++)cl->ifc[i] = 0;
}
char
puttmp(char c, int keep)
{
	if(tp < &temp[TEMP-120])
		*tp++ = c;
	else {
		if(keep){
			if(tp >= &temp[TEMP-1]){
				fprint(2,"can't look past huge comment - quiting\n");
				exits("boom");
			}
			*tp++ = c;
		}
		else if(err == 0){
			err++;
			fprint(2,"truncating long comment\n");
		}
	}
	return(c);
}
void
error(char *s)
{
	fprint(2,"saw EOF while looking for %s\n",s);
	exits("boom");
}
