#include "rc.h"
#include "io.h"
#include "getflags.h"
#include "fns.h"

lexer *lex;

int doprompt = 1;
int nerror;

int
wordchr(int c)
{
	return !strchr("\n \t#;&|^$=`'{}()<>", c) && c!=EOF;
}

int
idchr(int c)
{
	/*
	 * Formerly:
	 * return 'a'<=c && c<='z' || 'A'<=c && c<='Z' || '0'<=c && c<='9'
	 *	|| c=='_' || c=='*';
	 */
	return c>' ' && !strchr("!\"#$%&'()+,-./:;<=>?@[\\]^`{|}~", c);
}

lexer*
newlexer(io *input, char *file)
{
	lexer *n = new(struct lexer);
	n->input = input;
	n->file = file;
	n->line = 1;
	n->eof = 0;
	n->future = EOF;
	n->peekc = '{';
	n->epilog = "}\n";
	n->lastc = 0;
	n->inquote = 0;
	n->incomm = 0;
	n->lastword = 0;
	n->lastdol = 0;
	n->iflast = 0;
	n->qflag = 0;
	n->tok[0] = 0;
	return n;
}

void
freelexer(lexer *p)
{
	closeio(p->input);
	free(p->file);
	free(p);
}

/*
 * read a character from the input stream
 */	
static int
getnext(void)
{
	int c;

	if(lex->peekc!=EOF){
		c = lex->peekc;
		lex->peekc = EOF;
		return c;
	}
	if(lex->eof){
epilog:
		if(*lex->epilog)
			return *lex->epilog++;
		doprompt = 1;
		return EOF;
	}
	if(doprompt)
		pprompt();
	c = rchr(lex->input);
	if(c=='\\' && !lex->inquote){
		c = rchr(lex->input);
		if(c=='\n' && !lex->incomm){		/* don't continue a comment */
			doprompt = 1;
			c=' ';
		}
		else{
			lex->peekc = c;
			c='\\';
		}
	}
	if(c==EOF){
		lex->eof = 1;
		goto epilog;
	} else {
		if(c=='\n')
			doprompt = 1;
		if((!lex->qflag && flag['v']!=0) || flag['V'])
			pchr(err, c);
	}
	return c;
}

/*
 * Look ahead in the input stream
 */
static int
nextc(void)
{
	if(lex->future==EOF)
		lex->future = getnext();
	return lex->future;
}

/*
 * Consume the lookahead character.
 */
static int
advance(void)
{
	int c = nextc();
	lex->lastc = lex->future;
	lex->future = EOF;
	if(c == '\n')
		lex->line++;
	return c;
}

static void
skipwhite(void)
{
	int c;
	for(;;){
		c = nextc();
		/* Why did this used to be  if(!inquote && c=='#') ?? */
		if(c=='#'){
			lex->incomm = 1;
			for(;;){
				c = nextc();
				if(c=='\n' || c==EOF) {
					lex->incomm = 0;
					break;
				}
				advance();
			}
		}
		if(c==' ' || c=='\t')
			advance();
		else return;
	}
}

void
skipnl(void)
{
	int c;
	for(;;){
		skipwhite();
		c = nextc();
		if(c!='\n')
			return;
		advance();
	}
}

static int
nextis(int c)
{
	if(nextc()==c){
		advance();
		return 1;
	}
	return 0;
}

static char*
addtok(char *p, int val)
{
	if(p==0)
		return 0;
	if(p==&lex->tok[NTOK-1]){
		*p = 0;
		yyerror("token buffer too short");
		return 0;
	}
	*p++=val;
	return p;
}

static char*
addutf(char *p, int c)
{
	int i, n;

	p = addtok(p, c);	/* 1-byte UTF runes are special */
	if(onebyte(c))
		return p;
	if(twobyte(c))
		n = 2;
	else if(threebyte(c))
		n = 3;
	else
		n = 4;
	for(i = 1; i < n; i++) {
		c = nextc();
		if(c == EOF || !xbyte(c))
			break;
		p = addtok(p, advance());
	}
	return p;
}

int
yylex(void)
{
	int glob, c, d = nextc();
	char *tok = lex->tok;
	char *w = tok;
	tree *t;

	yylval.tree = 0;

	/*
	 * Embarassing sneakiness:  if the last token read was a quoted or unquoted
	 * WORD then we alter the meaning of what follows.  If the next character
	 * is `(', we return SUB (a subscript paren) and consume the `('.  Otherwise,
	 * if the next character is the first character of a simple or compound word,
	 * we insert a `^' before it.
	 */
	if(lex->lastword){
		lex->lastword = 0;
		if(d=='('){
			advance();
			strcpy(tok, "( [SUB]");
			return SUB;
		}
		if(wordchr(d) || d=='\'' || d=='`' || d=='$' || d=='"'){
			strcpy(tok, "^");
			return '^';
		}
	}
	lex->inquote = 0;
	skipwhite();
	switch(c = advance()){
	case EOF:
		lex->lastdol = 0;
		strcpy(tok, "EOF");
		return EOF;
	case '$':
		lex->lastdol = 1;
		if(nextis('#')){
			strcpy(tok, "$#");
			return COUNT;
		}
		if(nextis('"')){
			strcpy(tok, "$\"");
			return '"';
		}
		strcpy(tok, "$");
		return '$';
	case '&':
		lex->lastdol = 0;
		if(nextis('&')){
			skipnl();
			strcpy(tok, "&&");
			return ANDAND;
		}
		strcpy(tok, "&");
		return '&';
	case '|':
		lex->lastdol = 0;
		if(nextis(c)){
			skipnl();
			strcpy(tok, "||");
			return OROR;
		}
	case '<':
	case '>':
		lex->lastdol = 0;
		/*
		 * funny redirection tokens:
		 *	redir:	arrow | arrow '[' fd ']'
		 *	arrow:	'<' | '<<' | '>' | '>>' | '|'
		 *	fd:	digit | digit '=' | digit '=' digit
		 *	digit:	'0'|'1'|'2'|'3'|'4'|'5'|'6'|'7'|'8'|'9'
		 * some possibilities are nonsensical and get a message.
		 */
		*w++=c;
		t = newtree();
		switch(c){
		case '|':
			t->type = PIPE;
			t->fd0 = 1;
			t->fd1 = 0;
			break;
		case '>':
			t->type = REDIR;
			if(nextis(c)){
				t->rtype = APPEND;
				*w++=c;
			}
			else t->rtype = WRITE;
			t->fd0 = 1;
			break;
		case '<':
			t->type = REDIR;
			if(nextis(c)){
				t->rtype = HERE;
				*w++=c;
			} else if (nextis('>')){
				t->rtype = RDWR;
				*w++=c;
			} else t->rtype = READ;
			t->fd0 = 0;
			break;
		}
		if(nextis('[')){
			*w++='[';
			c = advance();
			*w++=c;
			if(c<'0' || '9'<c){
			RedirErr:
				*w = 0;
				yyerror(t->type==PIPE?"pipe syntax"
						:"redirection syntax");
				return EOF;
			}
			t->fd0 = 0;
			do{
				t->fd0 = t->fd0*10+c-'0';
				*w++=c;
				c = advance();
			}while('0'<=c && c<='9');
			if(c=='='){
				*w++='=';
				if(t->type==REDIR)
					t->type = DUP;
				c = advance();
				if('0'<=c && c<='9'){
					t->rtype = DUPFD;
					t->fd1 = t->fd0;
					t->fd0 = 0;
					do{
						t->fd0 = t->fd0*10+c-'0';
						*w++=c;
						c = advance();
					}while('0'<=c && c<='9');
				}
				else{
					if(t->type==PIPE)
						goto RedirErr;
					t->rtype = CLOSE;
				}
			}
			if(c!=']'
			|| t->type==DUP && (t->rtype==HERE || t->rtype==APPEND))
				goto RedirErr;
			*w++=']';
		}
		*w='\0';
		yylval.tree = t;
		if(t->type==PIPE)
			skipnl();
		return t->type;
	case '\'':
		lex->lastdol = 0;
		lex->lastword = 1;
		lex->inquote = 1;
		for(;;){
			c = advance();
			if(c==EOF)
				break;
			if(c=='\''){
				if(nextc()!='\'')
					break;
				advance();
			}
			w = addutf(w, c);
		}
		if(w!=0)
			*w='\0';
		t = token(tok, WORD);
		t->quoted = 1;
		yylval.tree = t;
		return t->type;
	}
	if(!wordchr(c)){
		lex->lastdol = 0;
		tok[0] = c;
		tok[1]='\0';
		return c;
	}
	glob = 0;
	for(;;){
		if(c=='*' || c=='[' || c=='?' || c==GLOB){
			glob = 1;
			w = addtok(w, GLOB);
		}
		w = addutf(w, c);
		c = nextc();
		if(lex->lastdol?!idchr(c):!wordchr(c)) break;
		advance();
	}

	lex->lastword = 1;
	lex->lastdol = 0;
	if(w!=0)
		*w='\0';
	t = klook(tok);
	if(t->type!=WORD)
		lex->lastword = 0;
	else
		t->glob = glob;
	t->quoted = 0;
	yylval.tree = t;
	return t->type;
}

void
yyerror(char *m)
{
	pfln(err, lex->file, lex->line);
	pstr(err, ": ");
	if(lex->tok[0] && lex->tok[0]!='\n')
		pfmt(err, "token %q: ", lex->tok);
	pfmt(err, "%s\n", m);
	flushio(err);

	lex->lastword = 0;
	lex->lastdol = 0;
	while(lex->lastc!='\n' && lex->lastc!=EOF) advance();
	nerror++;

	setstatus(m);
}
