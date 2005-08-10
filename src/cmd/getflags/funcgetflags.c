#include <u.h>
#include <libc.h>
#include <ctype.h>
#include "getflags.h"

char **flag[NFLAG];
char cmdline[NCMDLINE+1];
char *cmdname;
char *flagset[];
char *flagset[]={"<flag>"};
static char *flagarg="";
static void reverse(char **, char **);
static int scanflag(int, char *);
static int reason;
#define	RESET	1
#define	ARGCCOUNT	2
#define	FLAGSYN	3
#define	BADFLAG	4
static int badflag;
char *getflagsargv[NGETFLAGSARGV+2];	/* original argv stored here for people who need it */

int
getflags(int argc, char *argv[], char *flags)
{
	char *s, *t;
	int i, j, c, count;
	flagarg=flags;
	if(cmdname==0){
		cmdname=argv[0];
		for(i=0;i!=argc && i!=NGETFLAGSARGV;i++) getflagsargv[i]=argv[i];
		if(argc>NGETFLAGSARGV) getflagsargv[i++]="...";
		getflagsargv[i]=0;
	}
	s=cmdline;
	for(i=0;i!=argc;i++){
		for(t=argv[i];*t;)
			if(s!=&cmdline[NCMDLINE])
				*s++=*t++;
			else
				break;
		if(i!=argc-1 && s!=&cmdline[NCMDLINE])
			*s++=' ';
	}
	*s='\0';
	i=1;
	while(i!=argc && argv[i][0]=='-'){
		s=argv[i]+1;
		if(*s=='\0'){	/* if argument is "-", stop scanning and delete it */
			for(j=i+1;j<=argc;j++)
				argv[j-1]=argv[j];
			return argc-1;
		}
		while(*s){
			c=*s++;
			count=scanflag(c, flags);
			if(count==-1) return -1;
			if(flag[c]){ reason=RESET; badflag=c; return -1; }
			if(count==0){
				flag[c]=flagset;
				if(*s=='\0'){
					for(j=i+1;j<=argc;j++)
						argv[j-1]=argv[j];
					--argc;
				}
			}
			else{
				if(*s=='\0'){
					for(j=i+1;j<=argc;j++)
						argv[j-1]=argv[j];
					--argc;
					s=argv[i];
				}
				if(argc-i<count){
					reason=ARGCCOUNT;
					badflag=c;
					return -1;
				}
				reverse(argv+i, argv+argc);
				reverse(argv+i, argv+argc-count);
				reverse(argv+argc-count+1, argv+argc);
				argc-=count;
				flag[c]=argv+argc+1;
				flag[c][0]=s;
				s="";
			}
		}
	}
	return argc;
}

void
static reverse(char **p, char **q)
{
	register char *t;
	for(;p<q;p++,--q){ t=*p; *p=*q; *q=t; }
}

static int
scanflag(int c, char *f)
{
	int fc, count;
	if(0<=c && c<NFLAG) while(*f){
		if(*f==' '){
			f++;
			continue;
		}
		fc=*f++;
		if(*f==':'){
			f++;
			if(!isdigit(*f)){ reason=FLAGSYN; return -1; }
			count=strtol(f, &f, 10);
		}
		else
			count=0;
		if(*f=='['){
			int depth=1;
			do{
				f++;
				if(*f=='\0'){ reason=FLAGSYN; return -1; }
				if(*f=='[') depth++;
				if(*f==']') depth--;
			}while(depth>0);
			f++;
		}
		if(c==fc) return count;
	}
	reason=BADFLAG;
	badflag=c;
	return -1;
}

static void errn(char *, int), errs(char *), errc(int);

void
usage(char *tail)
{
	char *s, *t, c;
	int count, nflag=0;
	switch(reason){
	case RESET:
		errs("Flag -");
		errc(badflag);
		errs(": set twice\n");
		break;
	case ARGCCOUNT:
		errs("Flag -");
		errc(badflag);
		errs(": too few arguments\n");
		break;
	case FLAGSYN:
		errs("Bad argument to getflags!\n");
		break;
	case BADFLAG:
		errs("Illegal flag -");
		errc(badflag);
		errc('\n');
		break;
	}
	errs("Usage: ");
	errs(cmdname);
	for(s=flagarg;*s;){
		c=*s;
		if(*s++==' ') continue;
		if(*s==':'){
			s++;
			count=strtol(s, &s, 10);
		}
		else count=0;
		if(count==0){
			if(nflag==0) errs(" [-");
			nflag++;
			errc(c);
		}
		if(*s=='['){
			int depth=1;
			s++;
			for(;*s!='\0' && depth>0; s++)
				if (*s==']') depth--;
				else if (*s=='[') depth++;
		}
	}
	if(nflag) errs("]");
	for(s=flagarg;*s;){
		c=*s;
		if(*s++==' ') continue;
		if(*s==':'){
			s++;
			count=strtol(s, &s, 10);
		}
		else count=0;
		if(count!=0){
			errs(" [-");
			errc(c);
			if(*s=='['){
				int depth=1;
				s++;
				t=s;
				for(;*s!='\0' && depth>0; s++)
					if (*s==']') depth--;
					else if (*s=='[') depth++;
				errs(" ");
				errn(t, s-t);
			}
			else
				while(count--) errs(" arg");
			errs("]");
		}
		else if(*s=='['){
			int depth=1;
			s++;
			for(;*s!='\0' && depth>0; s++)
				if (*s==']') depth--;
				else if (*s=='[') depth++;
		}
	}
	if(tail){
		errs(" ");
		errs(tail);
	}
	errs("\n");
	exits("usage");
}

static void
errn(char *s, int count)
{
	while(count){ errc(*s++); --count; }
}

static void
errs(char *s)
{
	while(*s) errc(*s++);
}

#define	NBUF	80
static char buf[NBUF], *bufp=buf;

static void
errc(int c){
	*bufp++=c;
	if(bufp==&buf[NBUF] || c=='\n'){
		write(2, buf, bufp-buf);
		bufp=buf;
	}
}

#ifdef TEST
#include <stdio.h>
main(int argc, char *argv[])
{
	int c, i, n;
	if(argc<3){
		fprint(2, "Usage: %s flags cmd ...\n", argv[0]);
		exits("usage");
	}
	n=getflags(argc-2, argv+2, argv[1]);
	if(n<0) usage("...");
	putchar('\n');
	for(c=0;c!=128;c++) if(flag[c]){
		print("\t-.%c. ", c);
		n=scanflag(c, argv[1]);
		for(i=0;i!=n;i++) print(" <%s>", flag[c][i]);
		putchar('\n');
	}
}
#endif
