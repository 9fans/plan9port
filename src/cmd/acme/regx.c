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

Rangeset	sel;
Rune		*lastregexp;

#undef class
#define class regxclass /* some systems declare "class" in system headers */

/*
 * Machine Information
 */
typedef struct Inst Inst;
struct Inst
{
	uint	type;	/* < OPERATOR ==> literal, otherwise action */
	union {
		int sid;
		int subid;
		int class;
		Inst *other;
		Inst *right;
	} u;
	union{
		Inst *left;
		Inst *next;
	} u1;
};

#define	NPROG	1024
Inst	program[NPROG];
Inst	*progp;
Inst	*startinst;	/* First inst. of program; might not be program[0] */
Inst	*bstartinst;	/* same for backwards machine */
Channel	*rechan;	/* chan(Inst*) */

typedef struct Ilist Ilist;
struct Ilist
{
	Inst	*inst;		/* Instruction of the thread */
	Rangeset se;
	uint	startp;		/* first char of match */
};

#define	NLIST	127

Ilist	*tl, *nl;	/* This list, next list */
Ilist	list[2][NLIST+1];	/* +1 for trailing null */
static	Rangeset sempty;

/*
 * Actions and Tokens
 *
 *	0x10000xx are operators, value == precedence
 *	0x20000xx are tokens, i.e. operands for operators
 */
#define	OPERATOR	0x1000000	/* Bit set in all operators */
#define	START		(OPERATOR+0)	/* Start, used for marker on stack */
#define	RBRA		(OPERATOR+1)	/* Right bracket, ) */
#define	LBRA		(OPERATOR+2)	/* Left bracket, ( */
#define	OR		(OPERATOR+3)	/* Alternation, | */
#define	CAT		(OPERATOR+4)	/* Concatentation, implicit operator */
#define	STAR		(OPERATOR+5)	/* Closure, * */
#define	PLUS		(OPERATOR+6)	/* a+ == aa* */
#define	QUEST		(OPERATOR+7)	/* a? == a|nothing, i.e. 0 or 1 a's */
#define	ANY		0x2000000	/* Any character but newline, . */
#define	NOP		(ANY+1)	/* No operation, internal use only */
#define	BOL		(ANY+2)	/* Beginning of line, ^ */
#define	EOL		(ANY+3)	/* End of line, $ */
#define	CCLASS		(ANY+4)	/* Character class, [] */
#define	NCCLASS		(ANY+5)	/* Negated character class, [^] */
#define	END		(ANY+0x77)	/* Terminate: match found */

#define	ISATOR		OPERATOR
#define	ISAND		ANY

#define	QUOTED	0x4000000	/* Bit set for \-ed lex characters */

/*
 * Parser Information
 */
typedef struct Node Node;
struct Node
{
	Inst	*first;
	Inst	*last;
};

#define	NSTACK	20
Node	andstack[NSTACK];
Node	*andp;
int	atorstack[NSTACK];
int	*atorp;
int	lastwasand;	/* Last token was operand */
int	cursubid;
int	subidstack[NSTACK];
int	*subidp;
int	backwards;
int	nbra;
Rune	*exprp;		/* pointer to next character in source expression */
#define	DCLASS	10	/* allocation increment */
int	nclass;		/* number active */
int	Nclass;		/* high water mark */
Rune	**class;
int	negateclass;

int	addinst(Ilist *l, Inst *inst, Rangeset *sep);
void	newmatch(Rangeset*);
void	bnewmatch(Rangeset*);
void	pushand(Inst*, Inst*);
void	pushator(int);
Node	*popand(int);
int	popator(void);
void	startlex(Rune*);
int	lex(void);
void	operator(int);
void	operand(int);
void	evaluntil(int);
void	optimize(Inst*);
void	bldcclass(void);

void
rxinit(void)
{
	rechan = chancreate(sizeof(Inst*), 0);
	chansetname(rechan, "rechan");
	lastregexp = runemalloc(1);
}

void
regerror(char *e)
{
	lastregexp[0] = 0;
	warning(nil, "regexp: %s\n", e);
	sendp(rechan, nil);
	threadexits(nil);
}

Inst *
newinst(int t)
{
	if(progp >= &program[NPROG])
		regerror("expression too long");
	progp->type = t;
	progp->u1.left = nil;
	progp->u.right = nil;
	return progp++;
}

void
realcompile(void *arg)
{
	int token;
	Rune *s;

	threadsetname("regcomp");
	s = arg;
	startlex(s);
	atorp = atorstack;
	andp = andstack;
	subidp = subidstack;
	cursubid = 0;
	lastwasand = FALSE;
	/* Start with a low priority operator to prime parser */
	pushator(START-1);
	while((token=lex()) != END){
		if((token&ISATOR) == OPERATOR)
			operator(token);
		else
			operand(token);
	}
	/* Close with a low priority operator */
	evaluntil(START);
	/* Force END */
	operand(END);
	evaluntil(START);
	if(nbra)
		regerror("unmatched `('");
	--andp;	/* points to first and only operand */
	sendp(rechan, andp->first);
	threadexits(nil);
}

/* r is null terminated */
int
rxcompile(Rune *r)
{
	int i, nr;
	Inst *oprogp;

	nr = runestrlen(r)+1;
	if(runeeq(lastregexp, runestrlen(lastregexp)+1, r, nr)==TRUE)
		return TRUE;
	lastregexp[0] = 0;
	for(i=0; i<nclass; i++)
		free(class[i]);
	nclass = 0;
	progp = program;
	backwards = FALSE;
	bstartinst = nil;
	threadcreate(realcompile, r, STACK);
	startinst = recvp(rechan);
	if(startinst == nil)
		return FALSE;
	optimize(program);
	oprogp = progp;
	backwards = TRUE;
	threadcreate(realcompile, r, STACK);
	bstartinst = recvp(rechan);
	if(bstartinst == nil)
		return FALSE;
	optimize(oprogp);
	lastregexp = runerealloc(lastregexp, nr);
	runemove(lastregexp, r, nr);
	return TRUE;
}

void
operand(int t)
{
	Inst *i;
	if(lastwasand)
		operator(CAT);	/* catenate is implicit */
	i = newinst(t);
	if(t == CCLASS){
		if(negateclass)
			i->type = NCCLASS;	/* UGH */
		i->u.class = nclass-1;		/* UGH */
	}
	pushand(i, i);
	lastwasand = TRUE;
}

void
operator(int t)
{
	if(t==RBRA && --nbra<0)
		regerror("unmatched `)'");
	if(t==LBRA){
		cursubid++;	/* silently ignored */
		nbra++;
		if(lastwasand)
			operator(CAT);
	}else
		evaluntil(t);
	if(t!=RBRA)
		pushator(t);
	lastwasand = FALSE;
	if(t==STAR || t==QUEST || t==PLUS || t==RBRA)
		lastwasand = TRUE;	/* these look like operands */
}

void
pushand(Inst *f, Inst *l)
{
	if(andp >= &andstack[NSTACK])
		error("operand stack overflow");
	andp->first = f;
	andp->last = l;
	andp++;
}

void
pushator(int t)
{
	if(atorp >= &atorstack[NSTACK])
		error("operator stack overflow");
	*atorp++=t;
	if(cursubid >= NRange)
		*subidp++= -1;
	else
		*subidp++=cursubid;
}

Node *
popand(int op)
{
	char buf[64];

	if(andp <= &andstack[0])
		if(op){
			sprint(buf, "missing operand for %c", op);
			regerror(buf);
		}else
			regerror("malformed regexp");
	return --andp;
}

int
popator()
{
	if(atorp <= &atorstack[0])
		error("operator stack underflow");
	--subidp;
	return *--atorp;
}

void
evaluntil(int pri)
{
	Node *op1, *op2, *t;
	Inst *inst1, *inst2;

	while(pri==RBRA || atorp[-1]>=pri){
		switch(popator()){
		case LBRA:
			op1 = popand('(');
			inst2 = newinst(RBRA);
			inst2->u.subid = *subidp;
			op1->last->u1.next = inst2;
			inst1 = newinst(LBRA);
			inst1->u.subid = *subidp;
			inst1->u1.next = op1->first;
			pushand(inst1, inst2);
			return;		/* must have been RBRA */
		default:
			error("unknown regexp operator");
			break;
		case OR:
			op2 = popand('|');
			op1 = popand('|');
			inst2 = newinst(NOP);
			op2->last->u1.next = inst2;
			op1->last->u1.next = inst2;
			inst1 = newinst(OR);
			inst1->u.right = op1->first;
			inst1->u1.left = op2->first;
			pushand(inst1, inst2);
			break;
		case CAT:
			op2 = popand(0);
			op1 = popand(0);
			if(backwards && op2->first->type!=END){
				t = op1;
				op1 = op2;
				op2 = t;
			}
			op1->last->u1.next = op2->first;
			pushand(op1->first, op2->last);
			break;
		case STAR:
			op2 = popand('*');
			inst1 = newinst(OR);
			op2->last->u1.next = inst1;
			inst1->u.right = op2->first;
			pushand(inst1, inst1);
			break;
		case PLUS:
			op2 = popand('+');
			inst1 = newinst(OR);
			op2->last->u1.next = inst1;
			inst1->u.right = op2->first;
			pushand(op2->first, inst1);
			break;
		case QUEST:
			op2 = popand('?');
			inst1 = newinst(OR);
			inst2 = newinst(NOP);
			inst1->u1.left = inst2;
			inst1->u.right = op2->first;
			op2->last->u1.next = inst2;
			pushand(inst1, inst2);
			break;
		}
	}
}


void
optimize(Inst *start)
{
	Inst *inst, *target;

	for(inst=start; inst->type!=END; inst++){
		target = inst->u1.next;
		while(target->type == NOP)
			target = target->u1.next;
		inst->u1.next = target;
	}
}

void
startlex(Rune *s)
{
	exprp = s;
	nbra = 0;
}


int
lex(void){
	int c;

	c = *exprp++;
	switch(c){
	case '\\':
		if(*exprp)
			if((c= *exprp++)=='n')
				c='\n';
		break;
	case 0:
		c = END;
		--exprp;	/* In case we come here again */
		break;
	case '*':
		c = STAR;
		break;
	case '?':
		c = QUEST;
		break;
	case '+':
		c = PLUS;
		break;
	case '|':
		c = OR;
		break;
	case '.':
		c = ANY;
		break;
	case '(':
		c = LBRA;
		break;
	case ')':
		c = RBRA;
		break;
	case '^':
		c = BOL;
		break;
	case '$':
		c = EOL;
		break;
	case '[':
		c = CCLASS;
		bldcclass();
		break;
	}
	return c;
}

int
nextrec(void)
{
	if(exprp[0]==0 || (exprp[0]=='\\' && exprp[1]==0))
		regerror("malformed `[]'");
	if(exprp[0] == '\\'){
		exprp++;
		if(*exprp=='n'){
			exprp++;
			return '\n';
		}
		return *exprp++|QUOTED;
	}
	return *exprp++;
}

void
bldcclass(void)
{
	int c1, c2, n, na;
	Rune *classp;

	classp = runemalloc(DCLASS);
	n = 0;
	na = DCLASS;
	/* we have already seen the '[' */
	if(*exprp == '^'){
		classp[n++] = '\n';	/* don't match newline in negate case */
		negateclass = TRUE;
		exprp++;
	}else
		negateclass = FALSE;
	while((c1 = nextrec()) != ']'){
		if(c1 == '-'){
    Error:
			free(classp);
			regerror("malformed `[]'");
		}
		if(n+4 >= na){		/* 3 runes plus NUL */
			na += DCLASS;
			classp = runerealloc(classp, na);
		}
		if(*exprp == '-'){
			exprp++;	/* eat '-' */
			if((c2 = nextrec()) == ']')
				goto Error;
			classp[n+0] = Runemax;
			classp[n+1] = c1;
			classp[n+2] = c2;
			n += 3;
		}else
			classp[n++] = c1 & ~QUOTED;
	}
	classp[n] = 0;
	if(nclass == Nclass){
		Nclass += DCLASS;
		class = realloc(class, Nclass*sizeof(Rune*));
	}
	class[nclass++] = classp;
}

int
classmatch(int classno, int c, int negate)
{
	Rune *p;

	p = class[classno];
	while(*p){
		if(*p == Runemax){
			if(p[1]<=c && c<=p[2])
				return !negate;
			p += 3;
		}else if(*p++ == c)
			return !negate;
	}
	return negate;
}

/*
 * Note optimization in addinst:
 * 	*l must be pending when addinst called; if *l has been looked
 *		at already, the optimization is a bug.
 */
int
addinst(Ilist *l, Inst *inst, Rangeset *sep)
{
	Ilist *p;

	for(p = l; p->inst; p++){
		if(p->inst==inst){
			if((sep)->r[0].q0 < p->se.r[0].q0)
				p->se= *sep;	/* this would be bug */
			return 0;	/* It's already there */
		}
	}
	p->inst = inst;
	p->se= *sep;
	(p+1)->inst = nil;
	return 1;
}

int
rxnull(void)
{
	return startinst==nil || bstartinst==nil;
}

/* either t!=nil or r!=nil, and we match the string in the appropriate place */
int
rxexecute(Text *t, Rune *r, uint startp, uint eof, Rangeset *rp)
{
	int flag;
	Inst *inst;
	Ilist *tlp;
	uint p;
	int nnl, ntl;
	int nc, c;
	int wrapped;
	int startchar;

	flag = 0;
	p = startp;
	startchar = 0;
	wrapped = 0;
	nnl = 0;
	if(startinst->type<OPERATOR)
		startchar = startinst->type;
	list[0][0].inst = list[1][0].inst = nil;
	sel.r[0].q0 = -1;
	if(t != nil)
		nc = t->file->b.nc;
	else
		nc = runestrlen(r);
	/* Execute machine once for each character */
	for(;;p++){
	doloop:
		if(p>=eof || p>=nc){
			switch(wrapped++){
			case 0:		/* let loop run one more click */
			case 2:
				break;
			case 1:		/* expired; wrap to beginning */
				if(sel.r[0].q0>=0 || eof!=Infinity)
					goto Return;
				list[0][0].inst = list[1][0].inst = nil;
				p = 0;
				goto doloop;
			default:
				goto Return;
			}
			c = 0;
		}else{
			if(((wrapped && p>=startp) || sel.r[0].q0>0) && nnl==0)
				break;
			if(t != nil)
				c = textreadc(t, p);
			else
				c = r[p];
		}
		/* fast check for first char */
		if(startchar && nnl==0 && c!=startchar)
			continue;
		tl = list[flag];
		nl = list[flag^=1];
		nl->inst = nil;
		ntl = nnl;
		nnl = 0;
		if(sel.r[0].q0<0 && (!wrapped || p<startp || startp==eof)){
			/* Add first instruction to this list */
			sempty.r[0].q0 = p;
			if(addinst(tl, startinst, &sempty))
			if(++ntl >= NLIST){
	Overflow:
				warning(nil, "regexp list overflow\n");
				sel.r[0].q0 = -1;
				goto Return;
			}
		}
		/* Execute machine until this list is empty */
		for(tlp = tl; inst = tlp->inst; tlp++){	/* assignment = */
	Switchstmt:
			switch(inst->type){
			default:	/* regular character */
				if(inst->type==c){
	Addinst:
					if(addinst(nl, inst->u1.next, &tlp->se))
					if(++nnl >= NLIST)
						goto Overflow;
				}
				break;
			case LBRA:
				if(inst->u.subid>=0)
					tlp->se.r[inst->u.subid].q0 = p;
				inst = inst->u1.next;
				goto Switchstmt;
			case RBRA:
				if(inst->u.subid>=0)
					tlp->se.r[inst->u.subid].q1 = p;
				inst = inst->u1.next;
				goto Switchstmt;
			case ANY:
				if(c!='\n')
					goto Addinst;
				break;
			case BOL:
				if(p==0 || (t!=nil && textreadc(t, p-1)=='\n') || (r!=nil && r[p-1]=='\n')){
	Step:
					inst = inst->u1.next;
					goto Switchstmt;
				}
				break;
			case EOL:
				if(c == '\n')
					goto Step;
				break;
			case CCLASS:
				if(c>=0 && classmatch(inst->u.class, c, 0))
					goto Addinst;
				break;
			case NCCLASS:
				if(c>=0 && classmatch(inst->u.class, c, 1))
					goto Addinst;
				break;
			case OR:
				/* evaluate right choice later */
				if(addinst(tlp, inst->u.right, &tlp->se))
				if(++ntl >= NLIST)
					goto Overflow;
				/* efficiency: advance and re-evaluate */
				inst = inst->u1.left;
				goto Switchstmt;
			case END:	/* Match! */
				tlp->se.r[0].q1 = p;
				newmatch(&tlp->se);
				break;
			}
		}
	}
    Return:
	*rp = sel;
	return sel.r[0].q0 >= 0;
}

void
newmatch(Rangeset *sp)
{
	if(sel.r[0].q0<0 || sp->r[0].q0<sel.r[0].q0 ||
	   (sp->r[0].q0==sel.r[0].q0 && sp->r[0].q1>sel.r[0].q1))
		sel = *sp;
}

int
rxbexecute(Text *t, uint startp, Rangeset *rp)
{
	int flag;
	Inst *inst;
	Ilist *tlp;
	int p;
	int nnl, ntl;
	int c;
	int wrapped;
	int startchar;

	flag = 0;
	nnl = 0;
	wrapped = 0;
	p = startp;
	startchar = 0;
	if(bstartinst->type<OPERATOR)
		startchar = bstartinst->type;
	list[0][0].inst = list[1][0].inst = nil;
	sel.r[0].q0= -1;
	/* Execute machine once for each character, including terminal NUL */
	for(;;--p){
	doloop:
		if(p <= 0){
			switch(wrapped++){
			case 0:		/* let loop run one more click */
			case 2:
				break;
			case 1:		/* expired; wrap to end */
				if(sel.r[0].q0>=0)
					goto Return;
				list[0][0].inst = list[1][0].inst = nil;
				p = t->file->b.nc;
				goto doloop;
			case 3:
			default:
				goto Return;
			}
			c = 0;
		}else{
			if(((wrapped && p<=startp) || sel.r[0].q0>0) && nnl==0)
				break;
			c = textreadc(t, p-1);
		}
		/* fast check for first char */
		if(startchar && nnl==0 && c!=startchar)
			continue;
		tl = list[flag];
		nl = list[flag^=1];
		nl->inst = nil;
		ntl = nnl;
		nnl = 0;
		if(sel.r[0].q0<0 && (!wrapped || p>startp)){
			/* Add first instruction to this list */
			/* the minus is so the optimizations in addinst work */
			sempty.r[0].q0 = -p;
			if(addinst(tl, bstartinst, &sempty))
			if(++ntl >= NLIST){
	Overflow:
				warning(nil, "regexp list overflow\n");
				sel.r[0].q0 = -1;
				goto Return;
			}
		}
		/* Execute machine until this list is empty */
		for(tlp = tl; inst = tlp->inst; tlp++){	/* assignment = */
	Switchstmt:
			switch(inst->type){
			default:	/* regular character */
				if(inst->type == c){
	Addinst:
					if(addinst(nl, inst->u1.next, &tlp->se))
					if(++nnl >= NLIST)
						goto Overflow;
				}
				break;
			case LBRA:
				if(inst->u.subid>=0)
					tlp->se.r[inst->u.subid].q0 = p;
				inst = inst->u1.next;
				goto Switchstmt;
			case RBRA:
				if(inst->u.subid >= 0)
					tlp->se.r[inst->u.subid].q1 = p;
				inst = inst->u1.next;
				goto Switchstmt;
			case ANY:
				if(c != '\n')
					goto Addinst;
				break;
			case BOL:
				if(c=='\n' || p==0){
	Step:
					inst = inst->u1.next;
					goto Switchstmt;
				}
				break;
			case EOL:
				if(p<t->file->b.nc && textreadc(t, p)=='\n')
					goto Step;
				break;
			case CCLASS:
				if(c>0 && classmatch(inst->u.class, c, 0))
					goto Addinst;
				break;
			case NCCLASS:
				if(c>0 && classmatch(inst->u.class, c, 1))
					goto Addinst;
				break;
			case OR:
				/* evaluate right choice later */
				if(addinst(tl, inst->u.right, &tlp->se))
				if(++ntl >= NLIST)
					goto Overflow;
				/* efficiency: advance and re-evaluate */
				inst = inst->u1.left;
				goto Switchstmt;
			case END:	/* Match! */
				tlp->se.r[0].q0 = -tlp->se.r[0].q0; /* minus sign */
				tlp->se.r[0].q1 = p;
				bnewmatch(&tlp->se);
				break;
			}
		}
	}
    Return:
	*rp = sel;
	return sel.r[0].q0 >= 0;
}

void
bnewmatch(Rangeset *sp)
{
        int  i;

        if(sel.r[0].q0<0 || sp->r[0].q0>sel.r[0].q1 || (sp->r[0].q0==sel.r[0].q1 && sp->r[0].q1<sel.r[0].q0))
                for(i = 0; i<NRange; i++){       /* note the reversal; q0<=q1 */
                        sel.r[i].q0 = sp->r[i].q1;
                        sel.r[i].q1 = sp->r[i].q0;
                }
}
