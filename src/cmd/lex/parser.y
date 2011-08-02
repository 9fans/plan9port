%token CHAR CCL NCCL STR DELIM SCON ITER NEWE NULLS
%left SCON '/' NEWE
%left '|'
%left '$' '^'
%left CHAR CCL NCCL '(' '.' STR NULLS
%left ITER
%left CAT
%left '*' '+' '?'

%{
# include "ldefs.h"
#define YYSTYPE union _yystype_
union _yystype_
{
	int	i;
	uchar	*cp;
};
%}
%%
%{
int i;
int j,k;
int g;
uchar *p;
%}
acc	:	lexinput
	={	
# ifdef DEBUG
		if(debug) sect2dump();
# endif
	}
	;
lexinput:	defns delim prods end
	|	defns delim end
	={
		if(!funcflag)phead2();
		funcflag = TRUE;
	}
	| error
	={
# ifdef DEBUG
		if(debug) {
			sect1dump();
			sect2dump();
			}
# endif
		}
	;
end:		delim | ;
defns:	defns STR STR
	={	strcpy((char*)dp,(char*)$2.cp);
		def[dptr] = dp;
		dp += strlen((char*)$2.cp) + 1;
		strcpy((char*)dp,(char*)$3.cp);
		subs[dptr++] = dp;
		if(dptr >= DEFSIZE)
			error("Too many definitions");
		dp += strlen((char*)$3.cp) + 1;
		if(dp >= dchar+DEFCHAR)
			error("Definitions too long");
		subs[dptr]=def[dptr]=0;	/* for lookup - require ending null */
	}
	|
	;
delim:	DELIM
	={
# ifdef DEBUG
		if(sect == DEFSECTION && debug) sect1dump();
# endif
		sect++;
		}
	;
prods:	prods pr
	={	$$.i = mn2(RNEWE,$1.i,$2.i);
		}
	|	pr
	={	$$.i = $1.i;}
	;
pr:	r NEWE
	={
		if(divflg == TRUE)
			i = mn1(S1FINAL,casecount);
		else i = mn1(FINAL,casecount);
		$$.i = mn2(RCAT,$1.i,i);
		divflg = FALSE;
		casecount++;
		}
	| error NEWE
	={
# ifdef DEBUG
		if(debug) sect2dump();
# endif
		}
r:	CHAR
	={	$$.i = mn0($1.i); }
	| STR
	={
		p = $1.cp;
		i = mn0(*p++);
		while(*p)
			i = mn2(RSTR,i,*p++);
		$$.i = i;
		}
	| '.'
	={	symbol['\n'] = 0;
		if(psave == FALSE){
			p = ccptr;
			psave = ccptr;
			for(i=1;i<'\n';i++){
				symbol[i] = 1;
				*ccptr++ = i;
				}
			for(i='\n'+1;i<NCH;i++){
				symbol[i] = 1;
				*ccptr++ = i;
				}
			*ccptr++ = 0;
			if(ccptr > ccl+CCLSIZE)
				error("Too many large character classes");
			}
		else
			p = psave;
		$$.i = mnp(RCCL,p);
		cclinter(1);
		}
	| CCL
	={	$$.i = mnp(RCCL,$1.cp); }
	| NCCL
	={	$$.i = mnp(RNCCL,$1.cp); }
	| r '*'
	={	$$.i = mn1(STAR,$1.i); }
	| r '+'
	={	$$.i = mn1(PLUS,$1.i); }
	| r '?'
	={	$$.i = mn1(QUEST,$1.i); }
	| r '|' r
	={	$$.i = mn2(BAR,$1.i,$3.i); }
	| r r %prec CAT
	={	$$.i = mn2(RCAT,$1.i,$2.i); }
	| r '/' r
	={	if(!divflg){
			j = mn1(S2FINAL,-casecount);
			i = mn2(RCAT,$1.i,j);
			$$.i = mn2(DIV,i,$3.i);
			}
		else {
			$$.i = mn2(RCAT,$1.i,$3.i);
			warning("Extra slash removed");
			}
		divflg = TRUE;
		}
	| r ITER ',' ITER '}'
	={	if($2.i > $4.i){
			i = $2.i;
			$2.i = $4.i;
			$4.i = i;
			}
		if($4.i <= 0)
			warning("Iteration range must be positive");
		else {
			j = $1.i;
			for(k = 2; k<=$2.i;k++)
				j = mn2(RCAT,j,dupl($1.i));
			for(i = $2.i+1; i<=$4.i; i++){
				g = dupl($1.i);
				for(k=2;k<=i;k++)
					g = mn2(RCAT,g,dupl($1.i));
				j = mn2(BAR,j,g);
				}
			$$.i = j;
			}
	}
	| r ITER '}'
	={
		if($2.i < 0)warning("Can't have negative iteration");
		else if($2.i == 0) $$.i = mn0(RNULLS);
		else {
			j = $1.i;
			for(k=2;k<=$2.i;k++)
				j = mn2(RCAT,j,dupl($1.i));
			$$.i = j;
			}
		}
	| r ITER ',' '}'
	={
				/* from n to infinity */
		if($2.i < 0)warning("Can't have negative iteration");
		else if($2.i == 0) $$.i = mn1(STAR,$1.i);
		else if($2.i == 1)$$.i = mn1(PLUS,$1.i);
		else {		/* >= 2 iterations minimum */
			j = $1.i;
			for(k=2;k<$2.i;k++)
				j = mn2(RCAT,j,dupl($1.i));
			k = mn1(PLUS,dupl($1.i));
			$$.i = mn2(RCAT,j,k);
			}
		}
	| SCON r
	={	$$.i = mn2(RSCON,$2.i,(uintptr)$1.cp); }
	| '^' r
	={	$$.i = mn1(CARAT,$2.i); }
	| r '$'
	={	i = mn0('\n');
		if(!divflg){
			j = mn1(S2FINAL,-casecount);
			k = mn2(RCAT,$1.i,j);
			$$.i = mn2(DIV,k,i);
			}
		else $$.i = mn2(RCAT,$1.i,i);
		divflg = TRUE;
		}
	| '(' r ')'
	={	$$.i = $2.i; }
	|	NULLS
	={	$$.i = mn0(RNULLS); }
	;
%%
int
yylex(void)
{
	uchar *p;
	int c, i;
	uchar  *t, *xp;
	int n, j, k, x;
	static int sectbegin;
	static uchar token[TOKENSIZE];
	static int iter;

# ifdef DEBUG
	yylval.i = 0;
# endif

	if(sect == DEFSECTION) {		/* definitions section */
		while(!eof) {
			if(prev == '\n'){		/* next char is at beginning of line */
				getl(p=buf);
				switch(*p){
				case '%':
					switch(*(p+1)){
					case '%':
						lgate();
						Bprint(&fout,"#define YYNEWLINE %d\n",'\n');
						Bprint(&fout,"int\nyylex(void){\nint nstr; extern int yyprevious;\nif(yyprevious){}\n");
						sectbegin = TRUE;
						i = treesize*(sizeof(*name)+sizeof(*left)+
							sizeof(*right)+sizeof(*nullstr)+sizeof(*parent))+ALITTLEEXTRA;
						p = myalloc(i,1);
						if(p == 0)
							error("Too little core for parse tree");
						free(p);
						name = myalloc(treesize,sizeof(*name));
						left = myalloc(treesize,sizeof(*left));
						right = myalloc(treesize,sizeof(*right));
						nullstr = myalloc(treesize,sizeof(*nullstr));
						parent = myalloc(treesize,sizeof(*parent));
						ptr = myalloc(treesize,sizeof(*ptr));
						if(name == 0 || left == 0 || right == 0 || parent == 0 || nullstr == 0 || ptr == 0)
							error("Too little core for parse tree");
						return(freturn(DELIM));
					case 'p': case 'P':	/* has overridden number of positions */
						while(*p && !isdigit(*p))p++;
						maxpos = atol((char*)p);
# ifdef DEBUG
						if (debug) print("positions (%%p) now %d\n",maxpos);
# endif
						if(report == 2)report = 1;
						continue;
					case 'n': case 'N':	/* has overridden number of states */
						while(*p && !isdigit(*p))p++;
						nstates = atol((char*)p);
# ifdef DEBUG
						if(debug)print( " no. states (%%n) now %d\n",nstates);
# endif
						if(report == 2)report = 1;
						continue;
					case 'e': case 'E':		/* has overridden number of tree nodes */
						while(*p && !isdigit(*p))p++;
						treesize = atol((char*)p);
# ifdef DEBUG
						if (debug) print("treesize (%%e) now %d\n",treesize);
# endif
						if(report == 2)report = 1;
						continue;
					case 'o': case 'O':
						while (*p && !isdigit(*p))p++;
						outsize = atol((char*)p);
						if (report ==2) report=1;
						continue;
					case 'a': case 'A':		/* has overridden number of transitions */
						while(*p && !isdigit(*p))p++;
						if(report == 2)report = 1;
						ntrans = atol((char*)p);
# ifdef DEBUG
						if (debug)print("N. trans (%%a) now %d\n",ntrans);
# endif
						continue;
					case 'k': case 'K': /* overriden packed char classes */
						while (*p && !isdigit(*p))p++;
						if (report==2) report=1;
						free(pchar);
						pchlen = atol((char*)p);
# ifdef DEBUG
						if (debug) print( "Size classes (%%k) now %d\n",pchlen);
# endif
						pchar=pcptr=myalloc(pchlen, sizeof(*pchar));
						continue;
					case '{':
						lgate();
						while(getl(p) && strcmp((char*)p,"%}") != 0)
							Bprint(&fout, "%s\n",(char*)p);
						if(p[0] == '%') continue;
						error("Premature eof");
					case 's': case 'S':		/* start conditions */
						lgate();
						while(*p && strchr(" \t,", *p) == 0) p++;
						n = TRUE;
						while(n){
							while(*p && strchr(" \t,", *p)) p++;
							t = p;
							while(*p && strchr(" \t,", *p) == 0)p++;
							if(!*p) n = FALSE;
							*p++ = 0;
							if (*t == 0) continue;
							i = sptr*2;
							Bprint(&fout,"#define %s %d\n",(char*)t,i);
							strcpy((char*)sp, (char*)t);
							sname[sptr++] = sp;
							sname[sptr] = 0;	/* required by lookup */
							if(sptr >= STARTSIZE)
								error("Too many start conditions");
							sp += strlen((char*)sp) + 1;
							if(sp >= stchar+STARTCHAR)
								error("Start conditions too long");
						}
						continue;
					default:
						warning("Invalid request %s",p);
						continue;
					}	/* end of switch after seeing '%' */
				case ' ': case '\t':		/* must be code */
					lgate();
					Bprint(&fout, "%s\n",(char*)p);
					continue;
				default:		/* definition */
					while(*p && !isspace(*p)) p++;
					if(*p == 0)
						continue;
					prev = *p;
					*p = 0;
					bptr = p+1;
					yylval.cp = buf;
					if(isdigit(buf[0]))
						warning("Substitution strings may not begin with digits");
					return(freturn(STR));
				}
			}
			/* still sect 1, but prev != '\n' */
			else {
				p = bptr;
				while(*p && isspace(*p)) p++;
				if(*p == 0)
					warning("No translation given - null string assumed");
				strcpy((char*)token, (char*)p);
				yylval.cp = token;
				prev = '\n';
				return(freturn(STR));
			}
		}
		/* end of section one processing */
	} else if(sect == RULESECTION){		/* rules and actions */
		while(!eof){
			switch(c=gch()){
			case '\0':
				return(freturn(0));
			case '\n':
				if(prev == '\n') continue;
				x = NEWE;
				break;
			case ' ':
			case '\t':
				if(sectbegin == TRUE){
					cpyact();
					while((c=gch()) && c != '\n');
					continue;
				}
				if(!funcflag)phead2();
				funcflag = TRUE;
				Bprint(&fout,"case %d:\n",casecount);
				if(cpyact())
					Bprint(&fout,"break;\n");
				while((c=gch()) && c != '\n');
				if(peek == ' ' || peek == '\t' || sectbegin == TRUE){
					warning("Executable statements should occur right after %%");
					continue;
				}
				x = NEWE;
				break;
			case '%':
				if(prev != '\n') goto character;
				if(peek == '{'){	/* included code */
					getl(buf);
					while(!eof && getl(buf) && strcmp("%}",(char*)buf) != 0)
						Bprint(&fout,"%s\n",(char*)buf);
					continue;
				}
				if(peek == '%'){
					gch();
					gch();
					x = DELIM;
					break;
				}
				goto character;
			case '|':
				if(peek == ' ' || peek == '\t' || peek == '\n'){
					Bprint(&fout,"%d\n",30000+casecount++);
					continue;
				}
				x = '|';
				break;
			case '$':
				if(peek == '\n' || peek == ' ' || peek == '\t' || peek == '|' || peek == '/'){
					x = c;
					break;
				}
				goto character;
			case '^':
				if(prev != '\n' && scon != TRUE) goto character;	/* valid only at line begin */
				x = c;
				break;
			case '?':
			case '+':
			case '.':
			case '*':
			case '(':
			case ')':
			case ',':
			case '/':
				x = c;
				break;
			case '}':
				iter = FALSE;
				x = c;
				break;
			case '{':	/* either iteration or definition */
				if(isdigit(c=gch())){	/* iteration */
					iter = TRUE;
				ieval:
					i = 0;
					while(isdigit(c)){
						token[i++] = c;
						c = gch();
					}
					token[i] = 0;
					yylval.i = atol((char*)token);
					munputc(c);
					x = ITER;
					break;
				} else {		/* definition */
					i = 0;
					while(c && c!='}'){
						token[i++] = c;
						c = gch();
					}
					token[i] = 0;
					i = lookup(token,def);
					if(i < 0)
						warning("Definition %s not found",token);
					else
						munputs(subs[i]);
					continue;
				}
			case '<':		/* start condition ? */
				if(prev != '\n')		/* not at line begin, not start */
					goto character;
				t = slptr;
				do {
					i = 0;
					c = gch();
					while(c != ',' && c && c != '>'){
						token[i++] = c;
						c = gch();
					}
					token[i] = 0;
					if(i == 0)
						goto character;
					i = lookup(token,sname);
					if(i < 0) {
						warning("Undefined start condition %s",token);
						continue;
					}
					*slptr++ = i+1;
				} while(c && c != '>');
				*slptr++ = 0;
				/* check if previous value re-usable */
				for (xp=slist; xp<t; ){
					if (strcmp((char*)xp, (char*)t)==0)
						break;
					while (*xp++);
				}
				if (xp<t){
					/* re-use previous pointer to string */
					slptr=t;
					t=xp;
				}
				if(slptr > slist+STARTSIZE)		/* note not packed ! */
					error("Too many start conditions used");
				yylval.cp = t;
				x = SCON;
				break;
			case '"':
				i = 0;
				while((c=gch()) && c != '"' && c != '\n'){
					if(c == '\\') c = usescape(gch());
					token[i++] = c;
					if(i > TOKENSIZE){
						warning("String too long");
						i = TOKENSIZE-1;
						break;
					}
				}
				if(c == '\n') {
					yyline--;
					warning("Non-terminated string");
					yyline++;
				}
				token[i] = 0;
				if(i == 0)x = NULLS;
				else if(i == 1){
					yylval.i = token[0];
					x = CHAR;
				} else {
					yylval.cp = token;
					x = STR;
				}
				break;
			case '[':
				for(i=1;i<NCH;i++) symbol[i] = 0;
				x = CCL;
				if((c = gch()) == '^'){
					x = NCCL;
					c = gch();
				}
				while(c != ']' && c){
					if(c == '\\') c = usescape(gch());
					symbol[c] = 1;
					j = c;
					if((c=gch()) == '-' && peek != ']'){		/* range specified */
						c = gch();
						if(c == '\\') c = usescape(gch());
						k = c;
						if(j > k) {
							n = j;
							j = k;
							k = n;
						}
						if(!(('A' <= j && k <= 'Z') ||
						     ('a' <= j && k <= 'z') ||
						     ('0' <= j && k <= '9')))
							warning("Non-portable Character Class");
						for(n=j+1;n<=k;n++)
							symbol[n] = 1;		/* implementation dependent */
						c = gch();
					}
				}
				/* try to pack ccl's */
				i = 0;
				for(j=0;j<NCH;j++)
					if(symbol[j])token[i++] = j;
				token[i] = 0;
				p = ccl;
				while(p <ccptr && strcmp((char*)token,(char*)p) != 0)p++;
				if(p < ccptr)	/* found it */
					yylval.cp = p;
				else {
					yylval.cp = ccptr;
					strcpy((char*)ccptr,(char*)token);
					ccptr += strlen((char*)token) + 1;
					if(ccptr >= ccl+CCLSIZE)
						error("Too many large character classes");
				}
				cclinter(x==CCL);
				break;
			case '\\':
				c = usescape(gch());
			default:
			character:
				if(iter){	/* second part of an iteration */
					iter = FALSE;
					if('0' <= c && c <= '9')
						goto ieval;
				}
				if(isalpha(peek)){
					i = 0;
					yylval.cp = token;
					token[i++] = c;
					while(isalpha(peek))
						token[i++] = gch();
					if(peek == '?' || peek == '*' || peek == '+')
						munputc(token[--i]);
					token[i] = 0;
					if(i == 1){
						yylval.i = token[0];
						x = CHAR;
					}
					else x = STR;
				} else {
					yylval.i = c;
					x = CHAR;
				}
			}
			scon = FALSE;
			if(x == SCON)scon = TRUE;
			sectbegin = FALSE;
			return(freturn(x));
		}
	}
	/* section three */
	ptail();
# ifdef DEBUG
	if(debug)
		Bprint(&fout,"\n/*this comes from section three - debug */\n");
# endif
	while(getl(buf) && !eof)
		Bprint(&fout,"%s\n",(char*)buf);
	return(freturn(0));
}
/* end of yylex */
# ifdef DEBUG
int
freturn(int i)
{
	if(yydebug) {
		print("now return ");
		if(i < NCH) allprint(i);
		else print("%d",i);
		printf("   yylval = ");
		switch(i){
			case STR: case CCL: case NCCL:
				strpt(yylval.cp);
				break;
			case CHAR:
				allprint(yylval.i);
				break;
			default:
				print("%d",yylval.i);
				break;
		}
		print("\n");
	}
	return(i);
}
# endif
