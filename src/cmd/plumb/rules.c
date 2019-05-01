#include <u.h>
#include <libc.h>
#include <bio.h>
#include <regexp.h>
#include <thread.h>
#include <ctype.h>
#include <plumb.h>
#include "plumber.h"

typedef struct Input Input;
typedef struct Var Var;

struct Input
{
	char		*file;		/* name of file */
	Biobuf	*fd;		/* input buffer, if from real file */
	uchar	*s;		/* input string, if from /mnt/plumb/rules */
	uchar	*end;	/* end of input string */
	int		lineno;
	Input	*next;	/* file to read after EOF on this one */
};

struct Var
{
	char	*name;
	char	*value;
	char *qvalue;
};

static int		parsing;
static int		nvars;
static Var		*vars;
static Input	*input;

static char 	ebuf[4096];

char *badports[] =
{
	".",
	"..",
	"send",
	nil
};

char *objects[] =
{
	"arg",
	"attr",
	"data",
	"dst",
	"plumb",
	"src",
	"type",
	"wdir",
	nil
};

char *verbs[] =
{
	"add",
	"client",
	"delete",
	"is",
	"isdir",
	"isfile",
	"matches",
	"set",
	"start",
	"to",
	nil
};

static void
printinputstackrev(Input *in)
{
	if(in == nil)
		return;
	printinputstackrev(in->next);
	fprint(2, "%s:%d: ", in->file, in->lineno);
}

void
printinputstack(void)
{
	printinputstackrev(input);
}

static void
pushinput(char *name, int fd, uchar *str)
{
	Input *in;
	int depth;

	depth = 0;
	for(in=input; in; in=in->next)
		if(depth++ >= 10)	/* prevent deep C stack in plumber and bad include structure */
			parseerror("include stack too deep; max 10");

	in = emalloc(sizeof(Input));
	in->file = estrdup(name);
	in->next = input;
	input = in;
	if(str)
		in->s = str;
	else{
		in->fd = emalloc(sizeof(Biobuf));
		if(Binit(in->fd, fd, OREAD) < 0)
			parseerror("can't initialize Bio for rules file: %r");
	}

}

int
popinput(void)
{
	Input *in;

	in = input;
	if(in == nil)
		return 0;
	input = in->next;
	if(in->fd){
		Bterm(in->fd);
		free(in->fd);
	}
	free(in);
	return 1;
}

static int
getc(void)
{
	if(input == nil)
		return Beof;
	if(input->fd)
		return Bgetc(input->fd);
	if(input->s < input->end)
		return *(input->s)++;
	return -1;
}

char*
getline(void)
{
	static int n = 0;
	static char *s /*, *incl*/;
	int c, i;

	i = 0;
	for(;;){
		c = getc();
		if(c < 0)
			return nil;
		if(i == n){
			n += 100;
			s = erealloc(s, n);
		}
		if(c<0 || c=='\0' || c=='\n')
			break;
		s[i++] = c;
	}
	s[i] = '\0';
	return s;
}

int
lookup(char *s, char *tab[])
{
	int i;

	for(i=0; tab[i]!=nil; i++)
		if(strcmp(s, tab[i])==0)
			return i;
	return -1;
}

Var*
lookupvariable(char *s, int n)
{
	int i;

	for(i=0; i<nvars; i++)
		if(n==strlen(vars[i].name) && memcmp(s, vars[i].name, n)==0)
			return vars+i;
	return nil;
}

char*
variable(char *s, int n)
{
	Var *var;

	var = lookupvariable(s, n);
	if(var)
		return var->qvalue;
	return nil;
}

void
setvariable(char  *s, int n, char *val, char *qval)
{
	Var *var;

	var = lookupvariable(s, n);
	if(var){
		free(var->value);
		free(var->qvalue);
	}else{
		vars = erealloc(vars, (nvars+1)*sizeof(Var));
		var = vars+nvars++;
		var->name = emalloc(n+1);
		memmove(var->name, s, n);
	}
	var->value = estrdup(val);
	var->qvalue = estrdup(qval);
}

static char*
nonnil(char *s)
{
	if(s == nil)
		return "";
	return s;
}

static char*
filename(Exec *e, char *name)
{
	static char *buf;	/* rock to hold value so we don't leak the strings */

	free(buf);
	/* if name is defined, used it */
	if(name!=nil && name[0]!='\0'){
		buf = estrdup(name);
		return cleanname(buf);
	}
	/* if data is an absolute file name, or wdir is empty, use it */
	if(e->msg->data[0]=='/' || e->msg->wdir==nil || e->msg->wdir[0]=='\0'){
		buf = estrdup(e->msg->data);
		return cleanname(buf);
	}
	buf = emalloc(strlen(e->msg->wdir)+1+strlen(e->msg->data)+1);
	sprint(buf, "%s/%s", e->msg->wdir, e->msg->data);
	return cleanname(buf);
}

char*
dollar(Exec *e, char *s, int *namelen)
{
	int n;
	static char *abuf;
	char *t;

	*namelen = 1;
	if(e!=nil && '0'<=s[0] && s[0]<='9')
		return nonnil(e->match[s[0]-'0']);

	for(t=s; isalnum((uchar)*t); t++)
		;
	n = t-s;
	*namelen = n;

	if(e != nil){
		if(n == 3){
			if(memcmp(s, "src", 3) == 0)
				return nonnil(e->msg->src);
			if(memcmp(s, "dst", 3) == 0)
				return nonnil(e->msg->dst);
			if(memcmp(s, "dir", 3) == 0)
				return filename(e, e->dir);
		}
		if(n == 4){
			if(memcmp(s, "attr", 4) == 0){
				free(abuf);
				abuf = plumbpackattr(e->msg->attr);
				return nonnil(abuf);
			}
			if(memcmp(s, "data", 4) == 0)
				return nonnil(e->msg->data);
			if(memcmp(s, "file", 4) == 0)
				return filename(e, e->file);
			if(memcmp(s, "type", 4) == 0)
				return nonnil(e->msg->type);
			if(memcmp(s, "wdir", 3) == 0)
				return nonnil(e->msg->wdir);
		}
	}

	return variable(s, n);
}

/* expand one blank-terminated string, processing quotes and $ signs */
char*
expand(Exec *e, char *s, char **ends)
{
	char *p, *ep, *val;
	int namelen, quoting;

	p = ebuf;
	ep = ebuf+sizeof ebuf-1;
	quoting = 0;
	while(p<ep && *s!='\0' && (quoting || (*s!=' ' && *s!='\t'))){
		if(*s == '\''){
			s++;
			if(!quoting)
				quoting = 1;
			else  if(*s == '\''){
				*p++ = '\'';
				s++;
			}else
				quoting = 0;
			continue;
		}
		if(quoting || *s!='$'){
			*p++ = *s++;
			continue;
		}
		s++;
		val = dollar(e, s, &namelen);
		if(val == nil){
			*p++ = '$';
			continue;
		}
		if(ep-p < strlen(val))
			return "string-too-long";
		strcpy(p, val);
		p += strlen(val);
		s += namelen;
	}
	if(ends)
		*ends = s;
	*p = '\0';
	return ebuf;
}

void
regerror(char *msg)
{
	if(parsing){
		parsing = 0;
		parseerror("%s", msg);
	}
	error("%s", msg);
}

void
parserule(Rule *r)
{
	r->qarg = estrdup(expand(nil, r->arg, nil));
	switch(r->obj){
	case OArg:
	case OAttr:
	case OData:
	case ODst:
	case OType:
	case OWdir:
	case OSrc:
		if(r->verb==VClient || r->verb==VStart || r->verb==VTo)
			parseerror("%s not valid verb for object %s", verbs[r->verb], objects[r->obj]);
		if(r->obj!=OAttr && (r->verb==VAdd || r->verb==VDelete))
			parseerror("%s not valid verb for object %s", verbs[r->verb], objects[r->obj]);
		if(r->verb == VMatches){
			r->regex = regcomp(r->qarg);
			return;
		}
		break;
	case OPlumb:
		if(r->verb!=VClient && r->verb!=VStart && r->verb!=VTo)
			parseerror("%s not valid verb for object %s", verbs[r->verb], objects[r->obj]);
		break;
	}
}

int
assignment(char *p)
{
	char *var, *qval;
	int n;

	if(!isalpha((uchar)p[0]))
		return 0;
	for(var=p; isalnum((uchar)*p); p++)
		;
	n = p-var;
	while(*p==' ' || *p=='\t')
			p++;
	if(*p++ != '=')
		return 0;
	while(*p==' ' || *p=='\t')
			p++;
	qval = expand(nil, p, nil);
	setvariable(var, n, p, qval);
	return 1;
}

int
include(char *s)
{
	char *t, *args[3], buf[128];
	int n, fd;

	if(strncmp(s, "include", 7) != 0)
		return 0;
	/* either an include or an error */
	n = tokenize(s, args, nelem(args));
	if(n < 2)
		goto Err;
	if(strcmp(args[0], "include") != 0)
		goto Err;
	if(args[1][0] == '#')
		goto Err;
	if(n>2 && args[2][0] != '#')
		goto Err;
	t = args[1];
	fd = open(t, OREAD);
	if(fd<0 && t[0]!='/' && strncmp(t, "./", 2)!=0 && strncmp(t, "../", 3)!=0){
		snprint(buf, sizeof buf, "#9/plumb/%s", t);
		t = unsharp(buf);
		fd = open(t, OREAD);
	}
	if(fd < 0)
		parseerror("can't open %s for inclusion", t);
	pushinput(t, fd, nil);
	return 1;

    Err:
	parseerror("malformed include statement");
	return 0;
}

Rule*
readrule(int *eof)
{
	Rule *rp;
	char *line, *p;
	char *word;

Top:
	line = getline();
	if(line == nil){
		/*
		 * if input is from string, and bytes remain (input->end is within string),
		 * morerules() will pop input and save remaining data.  otherwise pop
		 * the stack here, and if there's more input, keep reading.
		 */
		if((input!=nil && input->end==nil) && popinput())
			goto Top;
		*eof = 1;
		return nil;
	}
	input->lineno++;

	for(p=line; *p==' ' || *p=='\t'; p++)
		;
	if(*p=='\0' || *p=='#')	/* empty or comment line */
		return nil;

	if(include(p))
		goto Top;

	if(assignment(p))
		return nil;

	rp = emalloc(sizeof(Rule));

	/* object */
	for(word=p; *p!=' ' && *p!='\t'; p++)
		if(*p == '\0')
			parseerror("malformed rule");
	*p++ = '\0';
	rp->obj = lookup(word, objects);
	if(rp->obj < 0){
		if(strcmp(word, "kind") == 0)	/* backwards compatibility */
			rp->obj = OType;
		else
			parseerror("unknown object %s", word);
	}

	/* verb */
	while(*p==' ' || *p=='\t')
		p++;
	for(word=p; *p!=' ' && *p!='\t'; p++)
		if(*p == '\0')
			parseerror("malformed rule");
	*p++ = '\0';
	rp->verb = lookup(word, verbs);
	if(rp->verb < 0)
		parseerror("unknown verb %s", word);

	/* argument */
	while(*p==' ' || *p=='\t')
		p++;
	if(*p == '\0')
		parseerror("malformed rule");
	rp->arg = estrdup(p);

	parserule(rp);

	return rp;
}

void
freerule(Rule *r)
{
	free(r->arg);
	free(r->qarg);
	free(r->regex);
}

void
freerules(Rule **r)
{
	while(*r)
		freerule(*r++);
}

void
freeruleset(Ruleset *rs)
{
	freerules(rs->pat);
	free(rs->pat);
	freerules(rs->act);
	free(rs->act);
	free(rs->port);
	free(rs);
}

Ruleset*
readruleset(void)
{
	Ruleset *rs;
	Rule *r;
	int eof, inrule, i, ncmd;
	char *plan9root;

	plan9root = get9root();
	if(plan9root)
		setvariable("plan9", 5, plan9root, plan9root);

   Again:
	eof = 0;
	rs = emalloc(sizeof(Ruleset));
	rs->pat = emalloc(sizeof(Rule*));
	rs->act = emalloc(sizeof(Rule*));
	inrule = 0;
	ncmd = 0;
	for(;;){
		r = readrule(&eof);
		if(eof)
			break;
		if(r==nil){
			if(inrule)
				break;
			continue;
		}
		inrule = 1;
		switch(r->obj){
		case OArg:
		case OAttr:
		case OData:
		case ODst:
		case OType:
		case OWdir:
		case OSrc:
			rs->npat++;
			rs->pat = erealloc(rs->pat, (rs->npat+1)*sizeof(Rule*));
			rs->pat[rs->npat-1] = r;
			rs->pat[rs->npat] = nil;
			break;
		case OPlumb:
			rs->nact++;
			rs->act = erealloc(rs->act, (rs->nact+1)*sizeof(Rule*));
			rs->act[rs->nact-1] = r;
			rs->act[rs->nact] = nil;
			if(r->verb == VTo){
				if(rs->npat>0 && rs->port != nil)	/* npat==0 implies port declaration */
					parseerror("too many ports");
				if(lookup(r->qarg, badports) >= 0)
					parseerror("illegal port name %s", r->qarg);
				rs->port = estrdup(r->qarg);
			}else
				ncmd++;	/* start or client rule */
			break;
		}
	}
	if(ncmd > 1){
		freeruleset(rs);
		parseerror("ruleset has more than one client or start action");
	}
	if(rs->npat>0 && rs->nact>0)
		return rs;
	if(rs->npat==0 && rs->nact==0){
		freeruleset(rs);
		return nil;
	}
	if(rs->nact==0 || rs->port==nil){
		freeruleset(rs);
		parseerror("ruleset must have patterns and actions");
		return nil;
	}

	/* declare ports */
	for(i=0; i<rs->nact; i++)
		if(rs->act[i]->verb != VTo){
			freeruleset(rs);
			parseerror("ruleset must have actions");
			return nil;
		}
	for(i=0; i<rs->nact; i++)
		addport(rs->act[i]->qarg);
	freeruleset(rs);
	goto Again;
}

Ruleset**
readrules(char *name, int fd)
{
	Ruleset *rs, **rules;
	int n;

	parsing = 1;
	pushinput(name, fd, nil);
	rules = emalloc(sizeof(Ruleset*));
	for(n=0; (rs=readruleset())!=nil; n++){
		rules = erealloc(rules, (n+2)*sizeof(Ruleset*));
		rules[n] = rs;
		rules[n+1] = nil;
	}
	popinput();
	parsing = 0;
	return rules;
}

char*
concat(char *s, char *t)
{
	if(t == nil)
		return s;
	if(s == nil)
		s = estrdup(t);
	else{
		s = erealloc(s, strlen(s)+strlen(t)+1);
		strcat(s, t);
	}
	return s;
}

char*
printpat(Rule *r)
{
	char *s;

	s = emalloc(strlen(objects[r->obj])+1+strlen(verbs[r->verb])+1+strlen(r->arg)+1+1);
	sprint(s, "%s\t%s\t%s\n", objects[r->obj], verbs[r->verb], r->arg);
	return s;
}

char*
printvar(Var *v)
{
	char *s;

	s = emalloc(strlen(v->name)+1+strlen(v->value)+2+1);
	sprint(s, "%s=%s\n\n", v->name, v->value);
	return s;
}

char*
printrule(Ruleset *r)
{
	int i;
	char *s;

	s = nil;
	for(i=0; i<r->npat; i++)
		s = concat(s, printpat(r->pat[i]));
	for(i=0; i<r->nact; i++)
		s = concat(s, printpat(r->act[i]));
	s = concat(s, "\n");
	return s;
}

char*
printport(char *port)
{
	char *s;

	s = nil;
	s = concat(s, "plumb to ");
	s = concat(s, port);
	s = concat(s, "\n");
	return s;
}

char*
printrules(void)
{
	int i;
	char *s;

	s = nil;
	for(i=0; i<nvars; i++)
		s = concat(s, printvar(&vars[i]));
	for(i=0; i<nports; i++)
		s = concat(s, printport(ports[i]));
	s = concat(s, "\n");
	for(i=0; rules[i]; i++)
		s = concat(s, printrule(rules[i]));
	return s;
}

char*
stringof(char *s, int n)
{
	char *t;

	t = emalloc(n+1);
	memmove(t, s, n);
	return t;
}

uchar*
morerules(uchar *text, int done)
{
	int n;
	Ruleset *rs;
	uchar *otext, *s, *endofrule;

	pushinput("<rules input>", -1, text);
	if(done)
		input->end = text+strlen((char*)text);
	else{
		/*
		 * Help user by sending any full rules to parser so any parse errors will
		 * occur on write rather than close. A heuristic will do: blank line ends rule.
		 */
		endofrule = nil;
		for(s=text; *s!='\0'; s++)
			if(*s=='\n' && *(s+1)=='\n')
				endofrule = s+2;
		if(endofrule == nil)
			return text;
		input->end = endofrule;
	}
	for(n=0; rules[n]; n++)
		;
	while((rs=readruleset()) != nil){
		rules = erealloc(rules, (n+2)*sizeof(Ruleset*));
		rules[n++] = rs;
		rules[n] = nil;
	}
	otext =text;
	if(input == nil)
		text = (uchar*)estrdup("");
	else
		text = (uchar*)estrdup((char*)input->end);
	popinput();
	free(otext);
	return text;
}

char*
writerules(char *s, int n)
{
	static uchar *text;
	char *tmp;

	free(lasterror);
	lasterror = nil;
	parsing = 1;
	if(setjmp(parsejmp) == 0){
		tmp = stringof(s, n);
		text = (uchar*)concat((char*)text, tmp);
		free(tmp);
		text = morerules(text, n==0);
	}
	if(s == nil){
		free(text);
		text = nil;
	}
	parsing = 0;
	makeports(rules);
	return lasterror;
}
