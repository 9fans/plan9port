#include "rc.h"
#include "exec.h"
#include "io.h"
#include "fns.h"
struct here *here, **ehere;
int ser=0;
char tmp[]="/tmp/here0000.0000";
char hex[]="0123456789abcdef";
void psubst(io*, char*);
void pstrs(io*, word*);
void hexnum(char *p, int n)
{
	*p++=hex[(n>>12)&0xF];
	*p++=hex[(n>>8)&0xF];
	*p++=hex[(n>>4)&0xF];
	*p=hex[n&0xF];
}
tree *heredoc(tree *tag)
{
	struct here *h=new(struct here);
	if(tag->type!=WORD) yyerror("Bad here tag");
	h->next=0;
	if(here)
		*ehere=h;
	else
		here=h;
	ehere=&h->next;
	h->tag=tag;
	hexnum(&tmp[9], getpid());
	hexnum(&tmp[14], ser++);
	h->name=strdup(tmp);
	return token(tmp, WORD);
}
/*
 * bug: lines longer than NLINE get split -- this can cause spurious
 * missubstitution, or a misrecognized EOF marker.
 */
#define	NLINE	4096
void readhere(void){
	struct here *h, *nexth;
	io *f;
	char *s, *tag;
	int c, subst;
	char line[NLINE+1];
	for(h=here;h;h=nexth){
		subst=!h->tag->quoted;
		tag=h->tag->str;
		c=Creat(h->name);
		if(c<0) yyerror("can't create here document");
		f=openfd(c);
		s=line;
		pprompt();
		while((c=rchr(runq->cmdfd))!=EOF){
			if(c=='\n' || s==&line[NLINE]){
				*s='\0';
				if(strcmp(line, tag)==0) break;
				if(subst) psubst(f, line);
				else pstr(f, line);
				s=line;
				if(c=='\n'){
					pprompt();
					pchr(f, c);
				}
				else *s++=c;
			}
			else *s++=c;
		}
		flush(f);
		closeio(f);
		cleanhere(h->name);
		nexth=h->next;
		efree((char *)h);
	}
	here=0;
	doprompt=1;
}
void psubst(io *f, char *s)
{
	char *t, *u;
	int savec, n;
	word *star;
	while(*s){
		if(*s!='$'){
			if(0xa0<=(*s&0xff) && (*s&0xff)<=0xf5){
				pchr(f, *s++);
				if(*s=='\0') break;
			}
			else if(0xf6<=(*s&0xff) && (*s&0xff)<=0xf7){
				pchr(f, *s++);
				if(*s=='\0') break;
				pchr(f, *s++);
				if(*s=='\0') break;
			}
			pchr(f, *s++);
		}
		else{
			t=++s;
			if(*t=='$') pchr(f, *t++);
			else{
				while(*t && idchr(*t)) t++;
				savec=*t;
				*t='\0';
				n=0;
				for(u=s;*u && '0'<=*u && *u<='9';u++) n=n*10+*u-'0';
				if(n && *u=='\0'){
					star=vlook("*")->val;
					if(star && 1<=n && n<=count(star)){
						while(--n) star=star->next;
						pstr(f, star->word);
					}
				}
				else
					pstrs(f, vlook(s)->val);
				*t=savec;
				if(savec=='^') t++;
			}
			s=t;
		}
	}
}
void pstrs(io *f, word *a)
{
	if(a){
		while(a->next && a->next->word){
			pstr(f, a->word);
			pchr(f, ' ');
			a=a->next;
		}
		pstr(f, a->word);
	}
}
