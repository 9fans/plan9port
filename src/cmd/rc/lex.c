#include "rc.h"
#include "exec.h"
#include "io.h"
#include "getflags.h"
#include "fns.h"
int getnext(void);

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
int future = EOF;
int doprompt = 1;
int inquote;
int incomm;
/*
 * Look ahead in the input stream
 */

int
nextc(void)
{
	if(future==EOF)
		future = getnext();
	return future;
}
/*
 * Consume the lookahead character.
 */

int
advance(void)
{
	int c = nextc();
	lastc = future;
	future = EOF;
	return c;
}
/*
 * read a character from the input stream
 */

int
getnext(void)
{
	int c;
	static int peekc = EOF;
	if(peekc!=EOF){
		c = peekc;
		peekc = EOF;
		return c;
	}
	if(runq->eof)
		return EOF;
	if(doprompt)
		pprompt();
	c = rchr(runq->cmdfd);
	if(!inquote && c=='\\'){
		c = rchr(runq->cmdfd);
		if(c=='\n' && !incomm){		/* don't continue a comment */
			doprompt = 1;
			c=' ';
		}
		else{
			peekc = c;
			c='\\';
		}
	}
	doprompt = doprompt || c=='\n' || c==EOF;
	if(c==EOF)
		runq->eof++;
	else if(flag['V'] || ndot>=2 && flag['v']) pchr(err, c);
	return c;
}

void
pprompt(void)
{
	var *prompt;
	if(runq->iflag){
		pstr(err, promptstr);
		flush(err);
		prompt = vlook("prompt");
		if(prompt->val && prompt->val->next)
			promptstr = prompt->val->next->word;
		else
			promptstr="\t";
	}
	runq->lineno++;
	doprompt = 0;
}

int
skipwhite(void)
{
	int c, skipped;
	skipped = 0;
	for(;;){
		c = nextc();
		/* Why did this used to be  if(!inquote && c=='#') ?? */
		if(c=='#'){
			incomm = 1;
			skipped = 1;
			for(;;){
				c = nextc();
				if(c=='\n' || c==EOF) {
					incomm = 0;
					break;
				}
				advance();
			}
		}
		if(c==' ' || c=='\t') {
			skipped = 1;
			advance();
		}
		else
			return skipped;
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

int
nextis(int c)
{
	if(nextc()==c){
		advance();
		return 1;
	}
	return 0;
}

char*
addtok(char *p, int val)
{
	if(p==0)
		return 0;
	if(p==&tok[NTOK-1]){
		*p = 0;
		yyerror("token buffer too short");
		return 0;
	}
	*p++=val;
	return p;
}

char*
addutf(char *p, int c)
{
	p = addtok(p, c);
	if(twobyte(c))	 /* 2-byte escape */
		return addtok(p, advance());
	if(threebyte(c)){	/* 3-byte escape */
		p = addtok(p, advance());
		return addtok(p, advance());
	}
	if(fourbyte(c)){	/* 4-byte escape */
		p = addtok(p, advance());
		p = addtok(p, advance());
		return addtok(p, advance());
	}
	return p;
}
int lastdol;	/* was the last token read '$' or '$#' or '"'? */
int lastword;	/* was the last token read a word or compound word terminator? */

int
yylex(void)
{
	int c, d = nextc();
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
	if(lastword && flag['Y']){
		lastword = 0;
		if(d=='('){
			advance();
			strcpy(tok, "(");
			return SUB;
		}
		if(wordchr(d) || d=='\'' || d=='`' || d=='$' || d=='"'){
			strcpy(tok, "^");
			return '^';
		}
	}
	inquote = 0;
	if(skipwhite() && !flag['Y'])
		return ' ';
	switch(c = advance()){
	case EOF:
		lastdol = 0;
		strcpy(tok, "EOF");
		return EOF;
	case '$':
		lastdol = 1;
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
		lastdol = 0;
		if(nextis('&')){
			if(flag['Y'])
				skipnl();
			strcpy(tok, "&&");
			return ANDAND;
		}
		strcpy(tok, "&");
		return '&';
	case '|':
		lastdol = 0;
		if(nextis(c)){
			if(flag['Y'])
				skipnl();
			strcpy(tok, "||");
			return OROR;
		}
	case '<':
	case '>':
		lastdol = 0;
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
		if(t->type==PIPE && flag['Y'])
			skipnl();
		if(t->type==REDIR) {
			skipwhite();
			if(nextc() == '{')
				t->type = REDIRW;
		}
		return t->type;
	case '\'':
		lastdol = 0;
		lastword = 1;
		inquote = 1;
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
		lastdol = 0;
		tok[0] = c;
		tok[1]='\0';
		return c;
	}
	for(;;){
		/* next line should have (char)c==GLOB, but ken's compiler is broken */
		if(c=='*' || c=='[' || c=='?' || c==(unsigned char)GLOB)
			w = addtok(w, GLOB);
		w = addutf(w, c);
		c = nextc();
		if(lastdol?!idchr(c):!wordchr(c)) break;
		advance();
	}

	lastword = 1;
	lastdol = 0;
	if(w!=0)
		*w='\0';
	t = klook(tok);
	if(t->type!=WORD)
		lastword = 0;
	t->quoted = 0;
	yylval.tree = t;
	return t->type;
}
