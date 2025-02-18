#include "rc.h"
#include "exec.h"
#include "io.h"
#include "fns.h"

void psubst(io*, unsigned char*);
void pstrs(io*, word*);

static char*
readhere1(tree *tag, io *in)
{
	io *out;
	char c, *m;

	pprompt();
	out = openiostr();
	m = tag->str;
	while((c = rchr(in)) != EOF){
		if(c=='\0'){
			yyerror("NUL bytes in here doc");
			closeio(out);
			return 0;
		}
		if(c=='\n'){
			lex->line++;
			if(m && *m=='\0'){
				out->bufp -= m - tag->str;
				*out->bufp='\0';
				break;
			}
			pprompt();
			m = tag->str;
		} else if(m){
			if(*m == c){
				m++;
			} else {
				m = 0;
			}
		}
		pchr(out, c);
	}
	doprompt = 1;
	return closeiostr(out);
}

static tree *head, *tail;

tree*
heredoc(tree *redir)
{
	if(redir->child[0]->type!=WORD){
		yyerror("Bad here tag");
		return nil;
	}
	redir->child[2]=0;
	if(head)
		tail->child[2]=redir;
	else
		head=redir;
	tail=redir;
    return tail;
}

void
readhere(io *in)
{
	while(head){
		tail=head->child[2];
		head->child[2]=0;
		head->str=readhere1(head->child[0], in);
		head=tail;
	}
}

void
psubst(io *f, unsigned char *s)
{
	unsigned char *t, *u;
	word *star;
	int savec, n;

	while(*s){
		if(*s!='$'){
			if(0xa0 <= *s && *s <= 0xf5){
				pchr(f, *s++);
				if(*s=='\0')
					break;
			}
			else if(0xf6 <= *s && *s <= 0xf7){
				pchr(f, *s++);
				if(*s=='\0')
					break;
				pchr(f, *s++);
				if(*s=='\0')
					break;
			}
			pchr(f, *s++);
		}
		else{
			t=++s;
			if(*t=='$')
				pchr(f, *t++);
			else{
				while(*t && idchr(*t)) t++;
				savec=*t;
				*t='\0';
				n = 0;
				for(u = s;*u && '0'<=*u && *u<='9';u++) n = n*10+*u-'0';
				if(n && *u=='\0'){
					star = vlook("*")->val;
					if(star && 1<=n && n<=count(star)){
						while(--n) star = star->next;
						pstr(f, star->word);
					}
				}
				else
					pstrs(f, vlook((char *)s)->val);
				*t = savec;
				if(savec=='^')
					t++;
			}
			s = t;
		}
	}
}

void
pstrs(io *f, word *a)
{
	if(a){
		while(a->next && a->next->word){
			pstr(f, a->word);
			pchr(f, ' ');
			a = a->next;
		}
		pstr(f, a->word);
	}
}
