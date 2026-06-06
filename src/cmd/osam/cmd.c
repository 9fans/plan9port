#include "sam.h"
#include "parse.h"

static char	linex[]="\n";
static char	wordx[]=" \t\n";
struct Cmdtab cmdtab[]={
/*	cmdc	text	regexp	addr	defcmd	defaddr	count	token	 fn	*/
	{'\n',	0,	0,	0,	0,	aDot,	0,	0,	nl_cmd},
	{'a',	1,	0,	0,	0,	aDot,	0,	0,	a_cmd},
	{'b',	0,	0,	0,	0,	aNo,	0,	linex,	b_cmd},
	{'B',	0,	0,	0,	0,	aNo,	0,	linex,	b_cmd},
	{'c',	1,	0,	0,	0,	aDot,	0,	0,	c_cmd},
	{'d',	0,	0,	0,	0,	aDot,	0,	0,	d_cmd},
	{'D',	0,	0,	0,	0,	aNo,	0,	linex,	D_cmd},
	{'e',	0,	0,	0,	0,	aNo,	0,	wordx,	e_cmd},
	{'f',	0,	0,	0,	0,	aNo,	0,	wordx,	f_cmd},
	{'g',	0,	1,	0,	'p',	aDot,	0,	0,	g_cmd},
	{'i',	1,	0,	0,	0,	aDot,	0,	0,	i_cmd},
	{'k',	0,	0,	0,	0,	aDot,	0,	0,	k_cmd},
	{'m',	0,	0,	1,	0,	aDot,	0,	0,	m_cmd},
	{'n',	0,	0,	0,	0,	aNo,	0,	0,	n_cmd},
	{'p',	0,	0,	0,	0,	aDot,	0,	0,	p_cmd},
	{'q',	0,	0,	0,	0,	aNo,	0,	0,	q_cmd},
	{'r',	0,	0,	0,	0,	aDot,	0,	wordx,	e_cmd},
	{'s',	0,	1,	0,	0,	aDot,	1,	0,	s_cmd},
	{'t',	0,	0,	1,	0,	aDot,	0,	0,	m_cmd},
	{'u',	0,	0,	0,	0,	aNo,	2,	0,	u_cmd},
	{'v',	0,	1,	0,	'p',	aDot,	0,	0,	g_cmd},
	{'w',	0,	0,	0,	0,	aAll,	0,	wordx,	w_cmd},
	{'x',	0,	1,	0,	'p',	aDot,	0,	0,	x_cmd},
	{'y',	0,	1,	0,	'p',	aDot,	0,	0,	x_cmd},
	{'X',	0,	1,	0,	'f',	aNo,	0,	0,	X_cmd},
	{'Y',	0,	1,	0,	'f',	aNo,	0,	0,	X_cmd},
	{'!',	0,	0,	0,	0,	aNo,	0,	linex,	plan9_cmd},
	{'>',	0,	0,	0,	0,	aDot,	0,	linex,	plan9_cmd},
	{'<',	0,	0,	0,	0,	aDot,	0,	linex,	plan9_cmd},
	{'|',	0,	0,	0,	0,	aDot,	0,	linex,	plan9_cmd},
	{'=',	0,	0,	0,	0,	aDot,	0,	linex,	eq_cmd},
	{'c'|0x100,0,	0,	0,	0,	aNo,	0,	wordx,	cd_cmd},
	{0,	0,	0,	0,	0,	0,	0,	0},
};
Cmd	*parsecmd(int);
Addr	*compoundaddr(void);
Addr	*simpleaddr(void);
void	freecmd(void);
void	okdelim(int);

Rune	line[BLOCKSIZE];
Rune	termline[BLOCKSIZE];
Rune	*linep = line;
Rune	*terminp = termline;
Rune	*termoutp = termline;

List	cmdlist = { 'p' };
List	addrlist = { 'p' };
List	relist = { 'p' };
List	stringlist = { 'p' };

int	eof;

void
resetcmd(void)
{
	linep = line;
	*linep = 0;
	terminp = termoutp = termline;
	freecmd();
}

int
inputc(void)
{
	int n, nbuf;
	char buf[UTFmax];
	Rune r;

    Again:
	nbuf = 0;
	if(downloaded){
		while(termoutp == terminp){
			cmdupdate();
			if(patset)
				tellpat();
			while(termlocked > 0){
				outT0(Hunlock);
				termlocked--;
			}
			if(rcv() == 0)
				return -1;
		}
		r = *termoutp++;
		if(termoutp == terminp)
			terminp = termoutp = termline;
	}else{
   		do{
			n = read(0, buf+nbuf, 1);
			if(n <= 0)
				return -1;
			nbuf += n;
		}while(!fullrune(buf, nbuf));
		chartorune(&r, buf);
	}
	if(r == 0){
		warn(Wnulls);
		goto Again;
	}
	return r;
}

int
inputline(void)
{
	int i, c, start;

	/*
	 * Could set linep = line and i = 0 here and just
	 * error(Etoolong) below, but this way we keep
	 * old input buffer history around for a while.
	 * This is useful only for debugging.
	 */
	i = linep - line;
	do{
		if((c = inputc())<=0)
			return -1;
		if(i == nelem(line)-1){
			if(linep == line)
				error(Etoolong);
			start = linep - line;
			runemove(line, linep, i-start);
			i -= start;
			linep = line;
		}
	}while((line[i++]=c) != '\n');
	line[i] = 0;
	return 1;
}

int
getch(void)
{
	if(eof)
		return -1;
	if(*linep==0 && inputline()<0){
		eof = TRUE;
		return -1;
	}
	return *linep++;
}

int
nextc(void)
{
	if(*linep == 0)
		return -1;
	return *linep;
}

void
ungetch(void)
{
	if(--linep < line)
		panic("ungetch");
}

Posn
getnum(int signok)
{
	Posn n=0;
	int c, sign;

	sign = 1;
	if(signok>1 && nextc()=='-'){
		sign = -1;
		getch();
	}
	if((c=nextc())<'0' || '9'<c)	/* no number defaults to 1 */
		return sign;
	while('0'<=(c=getch()) && c<='9')
		n = n*10 + (c-'0');
	ungetch();
	return sign*n;
}

int
skipbl(void)
{
	int c;
	do
		c = getch();
	while(c==' ' || c=='\t');
	if(c >= 0)
		ungetch();
	return c;
}

void
termcommand(void)
{
	Posn p;

	for(p=cmdpt; p<cmd->b.nc; p++){
		if(terminp >= &termline[BLOCKSIZE]){
			cmdpt = cmd->b.nc;
			error(Etoolong);
		}
		*terminp++ = filereadc(cmd, p);
	}
	cmdpt = cmd->b.nc;
}

void
cmdloop(void)
{
	Cmd *cmdp;
	File *ocurfile;
	int loaded;

	for(;;){
		if(!downloaded && curfile && curfile->unread)
			load(curfile);
		if((cmdp = parsecmd(0))==0){
			if(downloaded){
				rescue();
				exits("eof");
			}
			break;
		}
		ocurfile = curfile;
		loaded = curfile && !curfile->unread;
		if(cmdexec(curfile, cmdp) == 0)
			break;
		freecmd();
		cmdupdate();
		update();
		if(downloaded && curfile &&
		    (ocurfile!=curfile || (!loaded && !curfile->unread)))
			outTs(Hcurrent, curfile->tag);
			/* don't allow type ahead on files that aren't bound */
		if(downloaded && curfile && curfile->rasp == 0)
			terminp = termoutp;
	}
}

Cmd *
newcmd(void){
	Cmd *p;

	p = emalloc(sizeof(Cmd));
	inslist(&cmdlist, cmdlist.nused, (long)p);
	return p;
}

Addr*
newaddr(void)
{
	Addr *p;

	p = emalloc(sizeof(Addr));
	inslist(&addrlist, addrlist.nused, (long)p);
	return p;
}

String*
newre(void)
{
	String *p;

	p = emalloc(sizeof(String));
	inslist(&relist, relist.nused, (long)p);
	Strinit(p);
	return p;
}

String*
newstring(void)
{
	String *p;

	p = emalloc(sizeof(String));
	inslist(&stringlist, stringlist.nused, (long)p);
	Strinit(p);
	return p;
}

void
freecmd(void)
{
	int i;

	while(cmdlist.nused > 0)
		free(cmdlist.voidpptr[--cmdlist.nused]);
	while(addrlist.nused > 0)
		free(addrlist.voidpptr[--addrlist.nused]);
	while(relist.nused > 0){
		i = --relist.nused;
		Strclose(relist.stringpptr[i]);
		free(relist.stringpptr[i]);
	}
	while(stringlist.nused>0){
		i = --stringlist.nused;
		Strclose(stringlist.stringpptr[i]);
		free(stringlist.stringpptr[i]);
	}
}

int
lookup(int c)
{
	int i;

	for(i=0; cmdtab[i].cmdc; i++)
		if(cmdtab[i].cmdc == c)
			return i;
	return -1;
}

void
okdelim(int c)
{
	if(c=='\\' || ('a'<=c && c<='z')
	|| ('A'<=c && c<='Z') || ('0'<=c && c<='9'))
		error_c(Edelim, c);
}

void
atnl(void)
{
	skipbl();
	if(getch() != '\n')
		error(Enewline);
}

void
getrhs(String *s, int delim, int cmd)
{
	int c;

	while((c = getch())>0 && c!=delim && c!='\n'){
		if(c == '\\'){
			if((c=getch()) <= 0)
				error(Ebadrhs);
			if(c == '\n'){
				ungetch();
				c='\\';
			}else if(c == 'n')
				c='\n';
			else if(c!=delim && (cmd=='s' || c!='\\'))	/* s does its own */
				Straddc(s, '\\');
		}
		Straddc(s, c);
	}
	ungetch();	/* let client read whether delimeter, '\n' or whatever */
}

String *
collecttoken(char *end)
{
	String *s = newstring();
	int c;

	while((c=nextc())==' ' || c=='\t')
		Straddc(s, getch()); /* blanks significant for getname() */
	while((c=getch())>0 && utfrune(end, c)==0)
		Straddc(s, c);
	Straddc(s, 0);
	if(c != '\n')
		atnl();
	return s;
}

String *
collecttext(void)
{
	String *s = newstring();
	int begline, i, c, delim;

	if(skipbl()=='\n'){
		getch();
		i = 0;
		do{
			begline = i;
			while((c = getch())>0 && c!='\n')
				i++, Straddc(s, c);
			i++, Straddc(s, '\n');
			if(c < 0)
				goto Return;
		}while(s->s[begline]!='.' || s->s[begline+1]!='\n');
		Strdelete(s, s->n-2, s->n);
	}else{
		okdelim(delim = getch());
		getrhs(s, delim, 'a');
		if(nextc()==delim)
			getch();
		atnl();
	}
    Return:
	Straddc(s, 0);		/* JUST FOR CMDPRINT() */
	return s;
}

Cmd *
parsecmd(int nest)
{
	int i, c;
	Cmdtab *ct;
	Cmd *cp, *ncp;
	Cmd cmd;

	cmd.next = cmd.ccmd = 0;
	cmd.re = 0;
	cmd.flag = cmd.num = 0;
	cmd.addr = compoundaddr();
	if(skipbl() == -1)
		return 0;
	if((c=getch())==-1)
		return 0;
	cmd.cmdc = c;
	if(cmd.cmdc=='c' && nextc()=='d'){	/* sleazy two-character case */
		getch();		/* the 'd' */
		cmd.cmdc='c'|0x100;
	}
	i = lookup(cmd.cmdc);
	if(i >= 0){
		if(cmd.cmdc == '\n')
			goto Return;	/* let nl_cmd work it all out */
		ct = &cmdtab[i];
		if(ct->defaddr==aNo && cmd.addr)
			error(Enoaddr);
		if(ct->count)
			cmd.num = getnum(ct->count);
		if(ct->regexp){
			/* x without pattern -> .*\n, indicated by cmd.re==0 */
			/* X without pattern is all files */
			if((ct->cmdc!='x' && ct->cmdc!='X') ||
			   ((c = nextc())!=' ' && c!='\t' && c!='\n')){
				skipbl();
				if((c = getch())=='\n' || c<0)
					error(Enopattern);
				okdelim(c);
				cmd.re = getregexp(c);
				if(ct->cmdc == 's'){
					cmd.ctext = newstring();
					getrhs(cmd.ctext, c, 's');
					if(nextc() == c){
						getch();
						if(nextc() == 'g')
							cmd.flag = getch();
					}

				}
			}
		}
		if(ct->addr && (cmd.caddr=simpleaddr())==0)
			error(Eaddress);
		if(ct->defcmd){
			if(skipbl() == '\n'){
				getch();
				cmd.ccmd = newcmd();
				cmd.ccmd->cmdc = ct->defcmd;
			}else if((cmd.ccmd = parsecmd(nest))==0)
				panic("defcmd");
		}else if(ct->text)
			cmd.ctext = collecttext();
		else if(ct->token)
			cmd.ctext = collecttoken(ct->token);
		else
			atnl();
	}else
		switch(cmd.cmdc){
		case '{':
			cp = 0;
			do{
				if(skipbl()=='\n')
					getch();
				ncp = parsecmd(nest+1);
				if(cp)
					cp->next = ncp;
				else
					cmd.ccmd = ncp;
			}while(cp = ncp);
			break;
		case '}':
			atnl();
			if(nest==0)
				error(Enolbrace);
			return 0;
		default:
			error_c(Eunk, cmd.cmdc);
		}
    Return:
	cp = newcmd();
	*cp = cmd;
	return cp;
}

String*				/* BUGGERED */
getregexp(int delim)
{
	String *r = newre();
	int c;

	for(Strzero(&genstr); ; Straddc(&genstr, c))
		if((c = getch())=='\\'){
			if(nextc()==delim)
				c = getch();
			else if(nextc()=='\\'){
				Straddc(&genstr, c);
				c = getch();
			}
		}else if(c==delim || c=='\n')
			break;
	if(c!=delim && c)
		ungetch();
	if(genstr.n > 0){
		patset = TRUE;
		Strduplstr(&lastpat, &genstr);
		Straddc(&lastpat, '\0');
	}
	if(lastpat.n <= 1)
		error(Epattern);
	Strduplstr(r, &lastpat);
	return r;
}

Addr *
simpleaddr(void)
{
	Addr addr;
	Addr *ap, *nap;

	addr.next = 0;
	addr.left = 0;
	addr.num = 0;
	switch(skipbl()){
	case '#':
		addr.type = getch();
		addr.num = getnum(1);
		break;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		addr.num = getnum(1);
		addr.type='l';
		break;
	case '/': case '?': case '"':
		addr.are = getregexp(addr.type = getch());
		break;
	case '.':
	case '$':
	case '+':
	case '-':
	case '\'':
		addr.type = getch();
		break;
	default:
		return 0;
	}
	if(addr.next = simpleaddr())
		switch(addr.next->type){
		case '.':
		case '$':
		case '\'':
			if(addr.type=='"')
				break;
			/* fall through */
		case '"':
			error(Eaddress);
			break;
		case 'l':
		case '#':
			if(addr.type=='"')
				break;
			/* fall through */
		case '/':
		case '?':
			if(addr.type!='+' && addr.type!='-'){
				/* insert the missing '+' */
				nap = newaddr();
				nap->type='+';
				nap->next = addr.next;
				addr.next = nap;
			}
			break;
		case '+':
		case '-':
			break;
		default:
			panic("simpleaddr");
		}
	ap = newaddr();
	*ap = addr;
	return ap;
}

Addr *
compoundaddr(void)
{
	Addr addr;
	Addr *ap, *next;

	addr.left = simpleaddr();
	if((addr.type = skipbl())!=',' && addr.type!=';')
		return addr.left;
	getch();
	next = addr.next = compoundaddr();
	if(next && (next->type==',' || next->type==';') && next->left==0)
		error(Eaddress);
	ap = newaddr();
	*ap = addr;
	return ap;
}
