#include "lib9.h"
#include "regexp9.h"
#include "regcomp.h"

#define	TRUE	1
#define	FALSE	0

/*
 * Parser Information
 */
typedef
struct Node
{
	Reinst*	first;
	Reinst*	last;
}Node;

/* max rune ranges per character class is nelem(classp->spans)/2 */
#define NCCRUNE	nelem(classp->spans)

#define	NSTACK	20
static	Node	andstack[NSTACK];
static	Node	*andp;
static	int	atorstack[NSTACK];
static	int*	atorp;
static	int	cursubid;		/* id of current subexpression */
static	int	subidstack[NSTACK];	/* parallel to atorstack */
static	int*	subidp;
static	int	lastwasand;	/* Last token was operand */
static	int	nbra;
static	char*	exprp;		/* pointer to next character in source expression */
static	int	lexdone;
static	int	nclass;
static	Reclass*classp;
static	Reinst*	freep;
static	int	errors;
static	Rune	yyrune;		/* last lex'd rune */
static	Reclass*yyclassp;	/* last lex'd class */

/* predeclared crap */
static	void	operator(int);
static	void	pushand(Reinst*, Reinst*);
static	void	pushator(int);
static	void	evaluntil(int);
static	int	bldcclass(void);

static jmp_buf regkaboom;

static	void
rcerror(char *s)
{
	errors++;
	regerror(s);
	longjmp(regkaboom, 1);
}

static	Reinst*
newinst(int t)
{
	freep->type = t;
	freep->u2.left = 0;
	freep->u1.right = 0;
	return freep++;
}

static	void
operand(int t)
{
	Reinst *i;

	if(lastwasand)
		operator(CAT);	/* catenate is implicit */
	i = newinst(t);

	if(t == CCLASS || t == NCCLASS)
		i->u1.cp = yyclassp;
	if(t == RUNE)
		i->u1.r = yyrune;

	pushand(i, i);
	lastwasand = TRUE;
}

static	void
operator(int t)
{
	if(t==RBRA && --nbra<0)
		rcerror("unmatched right paren");
	if(t==LBRA){
		if(++cursubid >= NSUBEXP)
			rcerror ("too many subexpressions");
		nbra++;
		if(lastwasand)
			operator(CAT);
	} else
		evaluntil(t);
	if(t != RBRA)
		pushator(t);
	lastwasand = FALSE;
	if(t==STAR || t==QUEST || t==PLUS || t==RBRA)
		lastwasand = TRUE;	/* these look like operands */
}

static	void
regerr2(char *s, int c)
{
	char buf[100];
	char *cp = buf;
	while(*s)
		*cp++ = *s++;
	*cp++ = c;
	*cp = '\0';
	rcerror(buf);
}

static	void
cant(char *s)
{
	char buf[100];
	strcpy(buf, "can't happen: ");
	strcat(buf, s);
	rcerror(buf);
}

static	void
pushand(Reinst *f, Reinst *l)
{
	if(andp >= &andstack[NSTACK])
		cant("operand stack overflow");
	andp->first = f;
	andp->last = l;
	andp++;
}

static	void
pushator(int t)
{
	if(atorp >= &atorstack[NSTACK])
		cant("operator stack overflow");
	*atorp++ = t;
	*subidp++ = cursubid;
}

static	Node*
popand(int op)
{
	Reinst *inst;

	if(andp <= &andstack[0]){
		regerr2("missing operand for ", op);
		inst = newinst(NOP);
		pushand(inst,inst);
	}
	return --andp;
}

static	int
popator(void)
{
	if(atorp <= &atorstack[0])
		cant("operator stack underflow");
	--subidp;
	return *--atorp;
}

static	void
evaluntil(int pri)
{
	Node *op1, *op2;
	Reinst *inst1, *inst2;

	while(pri==RBRA || atorp[-1]>=pri){
		switch(popator()){
		default:
			rcerror("unknown operator in evaluntil");
			break;
		case LBRA:		/* must have been RBRA */
			op1 = popand('(');
			inst2 = newinst(RBRA);
			inst2->u1.subid = *subidp;
			op1->last->u2.next = inst2;
			inst1 = newinst(LBRA);
			inst1->u1.subid = *subidp;
			inst1->u2.next = op1->first;
			pushand(inst1, inst2);
			return;
		case OR:
			op2 = popand('|');
			op1 = popand('|');
			inst2 = newinst(NOP);
			op2->last->u2.next = inst2;
			op1->last->u2.next = inst2;
			inst1 = newinst(OR);
			inst1->u1.right = op1->first;
			inst1->u2.left = op2->first;
			pushand(inst1, inst2);
			break;
		case CAT:
			op2 = popand(0);
			op1 = popand(0);
			op1->last->u2.next = op2->first;
			pushand(op1->first, op2->last);
			break;
		case STAR:
			op2 = popand('*');
			inst1 = newinst(OR);
			op2->last->u2.next = inst1;
			inst1->u1.right = op2->first;
			pushand(inst1, inst1);
			break;
		case PLUS:
			op2 = popand('+');
			inst1 = newinst(OR);
			op2->last->u2.next = inst1;
			inst1->u1.right = op2->first;
			pushand(op2->first, inst1);
			break;
		case QUEST:
			op2 = popand('?');
			inst1 = newinst(OR);
			inst2 = newinst(NOP);
			inst1->u2.left = inst2;
			inst1->u1.right = op2->first;
			op2->last->u2.next = inst2;
			pushand(inst1, inst2);
			break;
		}
	}
}

static	Reprog*
optimize(Reprog *pp)
{
	Reinst *inst, *target;
	int size;
	Reprog *npp;
	Reclass *cl;
	ptrdiff_t diff;

	/*
	 *  get rid of NOOP chains
	 */
	for(inst=pp->firstinst; inst->type!=END; inst++){
		target = inst->u2.next;
		while(target->type == NOP)
			target = target->u2.next;
		inst->u2.next = target;
	}

	/*
	 *  The original allocation is for an area larger than
	 *  necessary.  Reallocate to the actual space used
	 *  and then relocate the code.
	 */
	size = sizeof(Reprog) + (freep - pp->firstinst)*sizeof(Reinst);
	npp = realloc(pp, size);
	if(npp==0 || npp==pp)
		return pp;
	diff = (char *)npp - (char *)pp;
	freep = (Reinst *)((char *)freep + diff);
	for(inst=npp->firstinst; inst<freep; inst++){
		switch(inst->type){
		case OR:
		case STAR:
		case PLUS:
		case QUEST:
			inst->u1.right = (void*)((char*)inst->u1.right + diff);
			break;
		case CCLASS:
		case NCCLASS:
			inst->u1.right = (void*)((char*)inst->u1.right + diff);
			cl = inst->u1.cp;
			cl->end = (void*)((char*)cl->end + diff);
			break;
		}
		inst->u2.left = (void*)((char*)inst->u2.left + diff);
	}
	npp->startinst = (void*)((char*)npp->startinst + diff);
	return npp;
}

#ifdef	DEBUG
static	void
dumpstack(void){
	Node *stk;
	int *ip;

	print("operators\n");
	for(ip=atorstack; ip<atorp; ip++)
		print("0%o\n", *ip);
	print("operands\n");
	for(stk=andstack; stk<andp; stk++)
		print("0%o\t0%o\n", stk->first->type, stk->last->type);
}

static	void
dump(Reprog *pp)
{
	Reinst *l;
	Rune *p;

	l = pp->firstinst;
	do{
		print("%d:\t0%o\t%d\t%d", l-pp->firstinst, l->type,
			l->u2.left-pp->firstinst, l->u1.right-pp->firstinst);
		if(l->type == RUNE)
			print("\t%C\n", l->u1.r);
		else if(l->type == CCLASS || l->type == NCCLASS){
			print("\t[");
			if(l->type == NCCLASS)
				print("^");
			for(p = l->u1.cp->spans; p < l->u1.cp->end; p += 2)
				if(p[0] == p[1])
					print("%C", p[0]);
				else
					print("%C-%C", p[0], p[1]);
			print("]\n");
		} else
			print("\n");
	}while(l++->type);
}
#endif

static	Reclass*
newclass(void)
{
	if(nclass >= nelem(((Reprog*)0)->class))
		rcerror("too many character classes; increase Reprog.class size");
	return &(classp[nclass++]);
}

static	int
nextc(Rune *rp)
{
	if(lexdone){
		*rp = 0;
		return 1;
	}
	exprp += chartorune(rp, exprp);
	if(*rp == '\\'){
		exprp += chartorune(rp, exprp);
		return 1;
	}
	if(*rp == 0)
		lexdone = 1;
	return 0;
}

static	int
lex(int literal, int dot_type)
{
	int quoted;

	quoted = nextc(&yyrune);
	if(literal || quoted){
		if(yyrune == 0)
			return END;
		return RUNE;
	}

	switch(yyrune){
	case 0:
		return END;
	case '*':
		return STAR;
	case '?':
		return QUEST;
	case '+':
		return PLUS;
	case '|':
		return OR;
	case '.':
		return dot_type;
	case '(':
		return LBRA;
	case ')':
		return RBRA;
	case '^':
		return BOL;
	case '$':
		return EOL;
	case '[':
		return bldcclass();
	}
	return RUNE;
}

static int
bldcclass(void)
{
	int type;
	Rune r[NCCRUNE];
	Rune *p, *ep, *np;
	Rune rune;
	int quoted;

	/* we have already seen the '[' */
	type = CCLASS;
	yyclassp = newclass();

	/* look ahead for negation */
	/* SPECIAL CASE!!! negated classes don't match \n */
	ep = r;
	quoted = nextc(&rune);
	if(!quoted && rune == '^'){
		type = NCCLASS;
		quoted = nextc(&rune);
		*ep++ = '\n';
		*ep++ = '\n';
	}

	/* parse class into a set of spans */
	while(ep < &r[NCCRUNE-1]){
		if(rune == 0){
			rcerror("malformed '[]'");
			return 0;
		}
		if(!quoted && rune == ']')
			break;
		if(!quoted && rune == '-'){
			if(ep == r){
				rcerror("malformed '[]'");
				return 0;
			}
			quoted = nextc(&rune);
			if((!quoted && rune == ']') || rune == 0){
				rcerror("malformed '[]'");
				return 0;
			}
			*(ep-1) = rune;
		} else {
			*ep++ = rune;
			*ep++ = rune;
		}
		quoted = nextc(&rune);
	}
	if(ep >= &r[NCCRUNE-1]) {
		rcerror("char class too large; increase Reclass.spans size");
		return 0;
	}

	/* sort on span start */
	for(p = r; p < ep; p += 2){
		for(np = p; np < ep; np += 2)
			if(*np < *p){
				rune = np[0];
				np[0] = p[0];
				p[0] = rune;
				rune = np[1];
				np[1] = p[1];
				p[1] = rune;
			}
	}

	/* merge spans */
	np = yyclassp->spans;
	p = r;
	if(r == ep)
		yyclassp->end = np;
	else {
		np[0] = *p++;
		np[1] = *p++;
		for(; p < ep; p += 2)
			/* overlapping or adjacent ranges? */
			if(p[0] <= np[1] + 1){
				if(p[1] >= np[1])
					np[1] = p[1];	/* coalesce */
			} else {
				np += 2;
				np[0] = p[0];
				np[1] = p[1];
			}
		yyclassp->end = np+2;
	}

	return type;
}

static	Reprog*
regcomp1(char *s, int literal, int dot_type)
{
	int token;
	Reprog *volatile pp;

	/* get memory for the program */
	pp = malloc(sizeof(Reprog) + 6*sizeof(Reinst)*strlen(s));
	if(pp == 0){
		regerror("out of memory");
		return 0;
	}
	freep = pp->firstinst;
	classp = pp->class;
	errors = 0;

	if(setjmp(regkaboom))
		goto out;

	/* go compile the sucker */
	lexdone = 0;
	exprp = s;
	nclass = 0;
	nbra = 0;
	atorp = atorstack;
	andp = andstack;
	subidp = subidstack;
	lastwasand = FALSE;
	cursubid = 0;

	/* Start with a low priority operator to prime parser */
	pushator(START-1);
	while((token = lex(literal, dot_type)) != END){
		if((token&0300) == OPERATOR)
			operator(token);
		else
			operand(token);
	}

	/* Close with a low priority operator */
	evaluntil(START);

	/* Force END */
	operand(END);
	evaluntil(START);
#ifdef DEBUG
	dumpstack();
#endif
	if(nbra)
		rcerror("unmatched left paren");
	--andp;	/* points to first and only operand */
	pp->startinst = andp->first;
#ifdef DEBUG
	dump(pp);
#endif
	pp = optimize(pp);
#ifdef DEBUG
	print("start: %d\n", andp->first-pp->firstinst);
	dump(pp);
#endif
out:
	if(errors){
		free(pp);
		pp = 0;
	}
	return pp;
}

extern	Reprog*
regcomp(char *s)
{
	return regcomp1(s, 0, ANY);
}

extern	Reprog*
regcomplit(char *s)
{
	return regcomp1(s, 1, ANY);
}

extern	Reprog*
regcompnl(char *s)
{
	return regcomp1(s, 0, ANYNL);
}
