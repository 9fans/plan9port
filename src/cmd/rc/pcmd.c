#include "rc.h"
#include "io.h"
#include "fns.h"

#define	c0	t->child[0]
#define	c1	t->child[1]
#define	c2	t->child[2]

static void
pdeglob(io *f, char *s)
{
	while(*s){
		if(*s==GLOB)
			s++;
		pchr(f, *s++);
	}
}

static int ntab = 0;

static char*
tabs(void)
{
	return "\t\t\t\t\t\t\t\t"+8-(ntab%8);
}

void
pcmd(io *f, tree *t)
{
	if(t==0)
		return;
	switch(t->type){
	default:	pfmt(f, "bad %d %p %p %p", t->type, c0, c1, c2);
	break;
	case '$':	pfmt(f, "$%t", c0);
	break;
	case '"':	pfmt(f, "$\"%t", c0);
	break;
	case '&':	pfmt(f, "%t&", c0);
	break;
	case '^':	pfmt(f, "%t^%t", c0, c1);
	break;
	case '`':	pfmt(f, "`%t%t", c0, c1);
	break;
	case ANDAND:	pfmt(f, "%t && %t", c0, c1);
	break;
	case BANG:	pfmt(f, "! %t", c0);
	break;
	case BRACE:
			ntab++;
			pfmt(f, "{\n%s%t", tabs(), c0);
			ntab--;
			pfmt(f, "\n%s}", tabs());
	break;
	case COUNT:	pfmt(f, "$#%t", c0);
	break;
	case FN:	pfmt(f, "fn %t %t", c0, c1);
	break;
	case IF:	pfmt(f, "if%t%t", c0, c1);
	break;
	case NOT:	pfmt(f, "if not %t", c0);
	break;
	case OROR:	pfmt(f, "%t || %t", c0, c1);
	break;
	case PCMD:
	case PAREN:	pfmt(f, "(%t)", c0);
	break;
	case SUB:	pfmt(f, "$%t(%t)", c0, c1);
	break;
	case SIMPLE:	pfmt(f, "%t", c0);
	break;
	case SUBSHELL:	pfmt(f, "@ %t", c0);
	break;
	case SWITCH:	pfmt(f, "switch %t %t", c0, c1);
	break;
	case TWIDDLE:	pfmt(f, "~ %t %t", c0, c1);
	break;
	case WHILE:	pfmt(f, "while %t%t", c0, c1);
	break;
	case ARGLIST:
		if(c0==0)
			pfmt(f, "%t", c1);
		else if(c1==0)
			pfmt(f, "%t", c0);
		else
			pfmt(f, "%t %t", c0, c1);
		break;
	case ';':
		if(c0){
			pfmt(f, "%t", c0);
			if(c1){
				if(c0->line==c1->line)
					pstr(f, "; ");
				else
					pfmt(f, "\n%s", tabs());
				pfmt(f, "%t", c1);
			}
		}
		else pfmt(f, "%t", c1);
		break;
	case WORDS:
		if(c0)
			pfmt(f, "%t ", c0);
		pfmt(f, "%t", c1);
		break;
	case FOR:
		pfmt(f, "for(%t", c0);
		if(c1)
			pfmt(f, " in %t", c1);
		pfmt(f, ")%t", c2);
		break;
	case WORD:
		if(t->quoted)
			pfmt(f, "%Q", t->str);
		else pdeglob(f, t->str);
		break;
	case DUP:
		if(t->rtype==DUPFD)
			pfmt(f, ">[%d=%d]", t->fd1, t->fd0); /* yes, fd1, then fd0; read lex.c */
		else
			pfmt(f, ">[%d=]", t->fd0);
		pfmt(f, "%t", c1);
		break;
	case PIPEFD:
	case REDIR:
		pchr(f, ' ');
		switch(t->rtype){
		case HERE:
			if(c1)
				pfmt(f, "%t ", c1);
			pchr(f, '<');
		case READ:
		case RDWR:
			pchr(f, '<');
			if(t->rtype==RDWR)
				pchr(f, '>');
			if(t->fd0!=0)
				pfmt(f, "[%d]", t->fd0);
			break;
		case APPEND:
			pchr(f, '>');
		case WRITE:
			pchr(f, '>');
			if(t->fd0!=1)
				pfmt(f, "[%d]", t->fd0);
			break;
		}
		pfmt(f, "%t", c0);
		if(t->rtype == HERE)
			pfmt(f, "\n%s%s\n", t->str, c0->str);
		else if(c1)
			pfmt(f, " %t", c1);
		break;
	case '=':
		pfmt(f, "%t=%t", c0, c1);
		if(c2)
			pfmt(f, " %t", c2);
		break;
	case PIPE:
		pfmt(f, "%t|", c0);
		if(t->fd1==0){
			if(t->fd0!=1)
				pfmt(f, "[%d]", t->fd0);
		}
		else pfmt(f, "[%d=%d]", t->fd0, t->fd1);
		pfmt(f, "%t", c1);
		break;
	}
}
