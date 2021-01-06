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
#include "edit.h"
#include "fns.h"

static char	linex[]="\n";
static char	wordx[]=" \t\n";
struct cmdtab cmdtab[]={
/*	cmdc	text	regexp	addr	defcmd	defaddr	count	token	 fn	*/
	'\n',	0,	0,	0,	0,	aDot,	0,	0,	nl_cmd,
	'a',	1,	0,	0,	0,	aDot,	0,	0,	a_cmd,
	'b',	0,	0,	0,	0,	aNo,	0,	linex,	b_cmd,
	'c',	1,	0,	0,	0,	aDot,	0,	0,	c_cmd,
	'd',	0,	0,	0,	0,	aDot,	0,	0,	d_cmd,
	'e',	0,	0,	0,	0,	aNo,	0,	wordx,	e_cmd,
	'f',	0,	0,	0,	0,	aNo,	0,	wordx,	f_cmd,
	'g',	0,	1,	0,	'p',	aDot,	0,	0,	g_cmd,
	'i',	1,	0,	0,	0,	aDot,	0,	0,	i_cmd,
	'm',	0,	0,	1,	0,	aDot,	0,	0,	m_cmd,
	'p',	0,	0,	0,	0,	aDot,	0,	0,	p_cmd,
	'r',	0,	0,	0,	0,	aDot,	0,	wordx,	e_cmd,
	's',	0,	1,	0,	0,	aDot,	1,	0,	s_cmd,
	't',	0,	0,	1,	0,	aDot,	0,	0,	m_cmd,
	'u',	0,	0,	0,	0,	aNo,	2,	0,	u_cmd,
	'v',	0,	1,	0,	'p',	aDot,	0,	0,	g_cmd,
	'w',	0,	0,	0,	0,	aAll,	0,	wordx,	w_cmd,
	'x',	0,	1,	0,	'p',	aDot,	0,	0,	x_cmd,
	'y',	0,	1,	0,	'p',	aDot,	0,	0,	x_cmd,
	'=',	0,	0,	0,	0,	aDot,	0,	linex,	eq_cmd,
	'B',	0,	0,	0,	0,	aNo,	0,	linex,	B_cmd,
	'D',	0,	0,	0,	0,	aNo,	0,	linex,	D_cmd,
	'X',	0,	1,	0,	'f',	aNo,	0,	0,	X_cmd,
	'Y',	0,	1,	0,	'f',	aNo,	0,	0,	X_cmd,
	'<',	0,	0,	0,	0,	aDot,	0,	linex,	pipe_cmd,
	'|',	0,	0,	0,	0,	aDot,	0,	linex,	pipe_cmd,
	'>',	0,	0,	0,	0,	aDot,	0,	linex,	pipe_cmd,
/* deliberately unimplemented:
	'k',	0,	0,	0,	0,	aDot,	0,	0,	k_cmd,
	'n',	0,	0,	0,	0,	aNo,	0,	0,	n_cmd,
	'q',	0,	0,	0,	0,	aNo,	0,	0,	q_cmd,
	'!',	0,	0,	0,	0,	aNo,	0,	linex,	plan9_cmd,
 */
	0,	0,	0,	0,	0,	0,	0,	0
};

Cmd	*parsecmd(int);
Addr	*compoundaddr(void);
Addr	*simpleaddr(void);
void	freecmd(void);
void	okdelim(int);

Rune	*cmdstartp;
Rune *cmdendp;
Rune	*cmdp;
Channel	*editerrc;

String	*lastpat;
int	patset;

List	cmdlist;
List	addrlist;
List	stringlist;
Text	*curtext;
int	editing = Inactive;

String*	newstring(int);

void
editthread(void *v)
{
	Cmd *cmdp;

	USED(v);
	threadsetname("editthread");
	while((cmdp=parsecmd(0)) != 0){
		if(cmdexec(curtext, cmdp) == 0)
			break;
		freecmd();
	}
	sendp(editerrc, nil);
}

void
allelogterm(Window *w, void *x)
{
	USED(x);
	elogterm(w->body.file);
}

void
alleditinit(Window *w, void *x)
{
	USED(x);
	textcommit(&w->tag, TRUE);
	textcommit(&w->body, TRUE);
	w->body.file->editclean = FALSE;
}

void
allupdate(Window *w, void *x)
{
	Text *t;
	int i;
	File *f;

	USED(x);
	t = &w->body;
	f = t->file;
	if(f->curtext != t)	/* do curtext only */
		return;
	if(f->elog.type == Null)
		elogterm(f);
	else if(f->elog.type != Empty){
		elogapply(f);
		if(f->editclean){
			f->mod = FALSE;
			for(i=0; i<f->ntext; i++)
				f->text[i]->w->dirty = FALSE;
		}
	}
	textsetselect(t, t->q0, t->q1);
	textscrdraw(t);
	winsettag(w);
}

void
editerror(char *fmt, ...)
{
	va_list arg;
	char *s;

	va_start(arg, fmt);
	s = vsmprint(fmt, arg);
	va_end(arg);
	freecmd();
	allwindows(allelogterm, nil);	/* truncate the edit logs */
	sendp(editerrc, s);
	threadexits(nil);
}

void
editcmd(Text *ct, Rune *r, uint n)
{
	char *err;

	if(n == 0)
		return;
	if(2*n > RBUFSIZE){
		warning(nil, "string too long\n");
		return;
	}

	allwindows(alleditinit, nil);
	if(cmdstartp)
		free(cmdstartp);
	cmdstartp = runemalloc(n+2);
	runemove(cmdstartp, r, n);
	if(r[n-1] != '\n')
		cmdstartp[n++] = '\n';
	cmdstartp[n] = '\0';
	cmdendp = cmdstartp+n;
	cmdp = cmdstartp;
	if(ct->w == nil)
		curtext = nil;
	else
		curtext = &ct->w->body;
	resetxec();
	if(editerrc == nil){
		editerrc = chancreate(sizeof(char*), 0);
		chansetname(editerrc, "editerrc");
		lastpat = allocstring(0);
	}
	threadcreate(editthread, nil, STACK);
	err = recvp(editerrc);
	editing = Inactive;
	if(err != nil){
		if(err[0] != '\0')
			warning(nil, "Edit: %s\n", err);
		free(err);
	}

	/* update everyone whose edit log has data */
	allwindows(allupdate, nil);
}

int
getch(void)
{
	if(cmdp == cmdendp)
		return -1;
	return *cmdp++;
}

int
nextc(void)
{
	if(cmdp == cmdendp)
		return -1;
	return *cmdp;
}

void
ungetch(void)
{
	if(--cmdp < cmdstartp)
		error("ungetch");
}

long
getnum(int signok)
{
	long n;
	int c, sign;

	n = 0;
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
cmdskipbl(void)
{
	int c;
	do
		c = getch();
	while(c==' ' || c=='\t');
	if(c >= 0)
		ungetch();
	return c;
}

/*
 * Check that list has room for one more element.
 */
void
growlist(List *l)
{
	if(l->u.listptr==0 || l->nalloc==0){
		l->nalloc = INCR;
		l->u.listptr = emalloc(INCR*sizeof(void*));
		l->nused = 0;
	}else if(l->nused == l->nalloc){
		l->u.listptr = erealloc(l->u.listptr, (l->nalloc+INCR)*sizeof(void*));
		memset(l->u.ptr+l->nalloc, 0, INCR*sizeof(void*));
		l->nalloc += INCR;
	}
}

/*
 * Remove the ith element from the list
 */
void
dellist(List *l, int i)
{
	memmove(&l->u.ptr[i], &l->u.ptr[i+1], (l->nused-(i+1))*sizeof(void*));
	l->nused--;
}

/*
 * Add a new element, whose position is i, to the list
 */
void
inslist(List *l, int i, void *v)
{
	growlist(l);
	memmove(&l->u.ptr[i+1], &l->u.ptr[i], (l->nused-i)*sizeof(void*));
	l->u.ptr[i] = v;
	l->nused++;
}

void
listfree(List *l)
{
	free(l->u.listptr);
	free(l);
}

String*
allocstring(int n)
{
	String *s;

	s = emalloc(sizeof(String));
	s->n = n;
	s->nalloc = n+10;
	s->r = emalloc(s->nalloc*sizeof(Rune));
	s->r[n] = '\0';
	return s;
}

void
freestring(String *s)
{
	free(s->r);
	free(s);
}

Cmd*
newcmd(void){
	Cmd *p;

	p = emalloc(sizeof(Cmd));
	inslist(&cmdlist, cmdlist.nused, p);
	return p;
}

String*
newstring(int n)
{
	String *p;

	p = allocstring(n);
	inslist(&stringlist, stringlist.nused, p);
	return p;
}

Addr*
newaddr(void)
{
	Addr *p;

	p = emalloc(sizeof(Addr));
	inslist(&addrlist, addrlist.nused, p);
	return p;
}

void
freecmd(void)
{
	int i;

	while(cmdlist.nused > 0)
		free(cmdlist.u.ucharptr[--cmdlist.nused]);
	while(addrlist.nused > 0)
		free(addrlist.u.ucharptr[--addrlist.nused]);
	while(stringlist.nused>0){
		i = --stringlist.nused;
		freestring(stringlist.u.stringptr[i]);
	}
}

void
okdelim(int c)
{
	if(c=='\\' || ('a'<=c && c<='z')
	|| ('A'<=c && c<='Z') || ('0'<=c && c<='9'))
		editerror("bad delimiter %c\n", c);
}

void
atnl(void)
{
	int c;

	cmdskipbl();
	c = getch();
	if(c != '\n')
		editerror("newline expected (saw %C)", c);
}

void
Straddc(String *s, int c)
{
	if(s->n+1 >= s->nalloc){
		s->nalloc += 10;
		s->r = erealloc(s->r, s->nalloc*sizeof(Rune));
	}
	s->r[s->n++] = c;
	s->r[s->n] = '\0';
}

void
getrhs(String *s, int delim, int cmd)
{
	int c;

	while((c = getch())>0 && c!=delim && c!='\n'){
		if(c == '\\'){
			if((c=getch()) <= 0)
				error("bad right hand side");
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
	ungetch();	/* let client read whether delimiter, '\n' or whatever */
}

String *
collecttoken(char *end)
{
	String *s = newstring(0);
	int c;

	while((c=nextc())==' ' || c=='\t')
		Straddc(s, getch()); /* blanks significant for getname() */
	while((c=getch())>0 && utfrune(end, c)==0)
		Straddc(s, c);
	if(c != '\n')
		atnl();
	return s;
}

String *
collecttext(void)
{
	String *s;
	int begline, i, c, delim;

	s = newstring(0);
	if(cmdskipbl()=='\n'){
		getch();
		i = 0;
		do{
			begline = i;
			while((c = getch())>0 && c!='\n')
				i++, Straddc(s, c);
			i++, Straddc(s, '\n');
			if(c < 0)
				goto Return;
		}while(s->r[begline]!='.' || s->r[begline+1]!='\n');
		s->r[s->n-2] = '\0';
		s->n -= 2;
	}else{
		okdelim(delim = getch());
		getrhs(s, delim, 'a');
		if(nextc()==delim)
			getch();
		atnl();
	}
    Return:
	return s;
}

int
cmdlookup(int c)
{
	int i;

	for(i=0; cmdtab[i].cmdc; i++)
		if(cmdtab[i].cmdc == c)
			return i;
	return -1;
}

Cmd*
parsecmd(int nest)
{
	int i, c;
	struct cmdtab *ct;
	Cmd *cp, *ncp;
	Cmd cmd;

	cmd.next = cmd.u.cmd = 0;
	cmd.re = 0;
	cmd.flag = cmd.num = 0;
	cmd.addr = compoundaddr();
	if(cmdskipbl() == -1)
		return 0;
	if((c=getch())==-1)
		return 0;
	cmd.cmdc = c;
	if(cmd.cmdc=='c' && nextc()=='d'){	/* sleazy two-character case */
		getch();		/* the 'd' */
		cmd.cmdc='c'|0x100;
	}
	i = cmdlookup(cmd.cmdc);
	if(i >= 0){
		if(cmd.cmdc == '\n')
			goto Return;	/* let nl_cmd work it all out */
		ct = &cmdtab[i];
		if(ct->defaddr==aNo && cmd.addr)
			editerror("command takes no address");
		if(ct->count)
			cmd.num = getnum(ct->count);
		if(ct->regexp){
			/* x without pattern -> .*\n, indicated by cmd.re==0 */
			/* X without pattern is all files */
			if((ct->cmdc!='x' && ct->cmdc!='X') ||
			   ((c = nextc())!=' ' && c!='\t' && c!='\n')){
				cmdskipbl();
				if((c = getch())=='\n' || c<0)
					editerror("no address");
				okdelim(c);
				cmd.re = getregexp(c);
				if(ct->cmdc == 's'){
					cmd.u.text = newstring(0);
					getrhs(cmd.u.text, c, 's');
					if(nextc() == c){
						getch();
						if(nextc() == 'g')
							cmd.flag = getch();
					}

				}
			}
		}
		if(ct->addr && (cmd.u.mtaddr=simpleaddr())==0)
			editerror("bad address");
		if(ct->defcmd){
			if(cmdskipbl() == '\n'){
				getch();
				cmd.u.cmd = newcmd();
				cmd.u.cmd->cmdc = ct->defcmd;
			}else if((cmd.u.cmd = parsecmd(nest))==0)
				error("defcmd");
		}else if(ct->text)
			cmd.u.text = collecttext();
		else if(ct->token)
			cmd.u.text = collecttoken(ct->token);
		else
			atnl();
	}else
		switch(cmd.cmdc){
		case '{':
			cp = 0;
			do{
				if(cmdskipbl()=='\n')
					getch();
				ncp = parsecmd(nest+1);
				if(cp)
					cp->next = ncp;
				else
					cmd.u.cmd = ncp;
			}while(cp = ncp);
			break;
		case '}':
			atnl();
			if(nest==0)
				editerror("right brace with no left brace");
			return 0;
		default:
			editerror("unknown command %c", cmd.cmdc);
		}
    Return:
	cp = newcmd();
	*cp = cmd;
	return cp;
}

String*
getregexp(int delim)
{
	String *buf, *r;
	int i, c;

	buf = allocstring(0);
	for(i=0; ; i++){
		if((c = getch())=='\\'){
			if(nextc()==delim)
				c = getch();
			else if(nextc()=='\\'){
				Straddc(buf, c);
				c = getch();
			}
		}else if(c==delim || c=='\n')
			break;
		if(i >= RBUFSIZE)
			editerror("regular expression too long");
		Straddc(buf, c);
	}
	if(c!=delim && c)
		ungetch();
	if(buf->n > 0){
		patset = TRUE;
		freestring(lastpat);
		lastpat = buf;
	}else
		freestring(buf);
	if(lastpat->n == 0)
		editerror("no regular expression defined");
	r = newstring(lastpat->n);
	runemove(r->r, lastpat->r, lastpat->n);	/* newstring put \0 at end */
	return r;
}

Addr *
simpleaddr(void)
{
	Addr addr;
	Addr *ap, *nap;

	addr.num = 0;
	addr.next = 0;
	addr.u.left = 0;
	switch(cmdskipbl()){
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
		addr.u.re = getregexp(addr.type = getch());
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
			editerror("bad address syntax");
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
			error("simpleaddr");
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

	addr.u.left = simpleaddr();
	if((addr.type = cmdskipbl())!=',' && addr.type!=';')
		return addr.u.left;
	getch();
	next = addr.next = compoundaddr();
	if(next && (next->type==',' || next->type==';') && next->u.left==0)
		editerror("bad address syntax");
	ap = newaddr();
	*ap = addr;
	return ap;
}
