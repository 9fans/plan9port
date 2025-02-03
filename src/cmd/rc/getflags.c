#include "rc.h"
#include "getflags.h"
#include "fns.h"
char *flagset[] = {"<flag>"};
char **flag[NFLAG];
char *cmdname;
static char *flagarg="";
static void reverse(char**, char**);
static int scanflag(int, char*);
static void errn(char*, int);
static void errs(char*);
static void errc(int);
static int reason;
#define	RESET	1
#define	FEWARGS	2
#define	FLAGSYN	3
#define	BADFLAG	4
static int badflag;

int
getflags(int argc, char *argv[], char *flags, int stop)
{
	char *s;
	int i, j, c, count;
	flagarg = flags;
	if(cmdname==0)
		cmdname = argv[0];

	i = 1;
	while(i!=argc){
		if(argv[i][0] != '-' || argv[i][1] == '\0'){
			if(stop)		/* always true in rc */
				return argc;
			i++;
			continue;
		}
		s = argv[i]+1;
		while(*s){
			c=*s++;
			count = scanflag(c, flags);
			if(count==-1)
				return -1;
			if(flag[c]){ reason = RESET; badflag = c; return -1; }
			if(count==0){
				flag[c] = flagset;
				if(*s=='\0'){
					for(j = i+1;j<=argc;j++)
						argv[j-1] = argv[j];
					--argc;
				}
			}
			else{
				if(*s=='\0'){
					for(j = i+1;j<=argc;j++)
						argv[j-1] = argv[j];
					--argc;
					s = argv[i];
				}
				if(argc-i<count){
					reason = FEWARGS;
					badflag = c;
					return -1;
				}
				reverse(argv+i, argv+argc);
				reverse(argv+i, argv+argc-count);
				reverse(argv+argc-count+1, argv+argc);
				argc-=count;
				flag[c] = argv+argc+1;
				flag[c][0] = s;
				s="";
			}
		}
	}
	return argc;
}

static void
reverse(char **p, char **q)
{
	char *t;
	for(;p<q;p++,--q){ t=*p; *p=*q; *q = t; }
}

static int
scanflag(int c, char *f)
{
	int fc, count;
	if(0<=c && c<NFLAG)
		while(*f){
			if(*f==' '){
				f++;
				continue;
			}
			fc=*f++;
			if(*f==':'){
				f++;
				if(*f<'0' || '9'<*f){ reason = FLAGSYN; return -1; }
				count = 0;
				while('0'<=*f && *f<='9') count = count*10+*f++-'0';
			}
			else
				count = 0;
			if(*f=='['){
				do{
					f++;
					if(*f=='\0'){ reason = FLAGSYN; return -1; }
				}while(*f!=']');
				f++;
			}
			if(c==fc)
				return count;
		}
	reason = BADFLAG;
	badflag = c;
	return -1;
}

void
usage(char *tail)
{
	char *s, *t, c;
	int count, nflag = 0;
	switch(reason){
	case RESET:
		errs("Flag -");
		errc(badflag);
		errs(": set twice\n");
		break;
	case FEWARGS:
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
	for(s = flagarg;*s;){
		c=*s;
		if(*s++==' ')
			continue;
		if(*s==':'){
			s++;
			count = 0;
			while('0'<=*s && *s<='9') count = count*10+*s++-'0';
		}
		else count = 0;
		if(count==0){
			if(nflag==0)
				errs(" [-");
			nflag++;
			errc(c);
		}
		if(*s=='['){
			s++;
			while(*s!=']' && *s!='\0') s++;
			if(*s==']')
				s++;
		}
	}
	if(nflag)
		errs("]");
	for(s = flagarg;*s;){
		c=*s;
		if(*s++==' ')
			continue;
		if(*s==':'){
			s++;
			count = 0;
			while('0'<=*s && *s<='9') count = count*10+*s++-'0';
		}
		else count = 0;
		if(count!=0){
			errs(" [-");
			errc(c);
			if(*s=='['){
				s++;
				t = s;
				while(*s!=']' && *s!='\0') s++;
				errs(" ");
				errn(t, s-t);
				if(*s==']')
					s++;
			}
			else
				while(count--) errs(" arg");
			errs("]");
		}
		else if(*s=='['){
			s++;
			while(*s!=']' && *s!='\0') s++;
			if(*s==']')
				s++;
		}
	}
	if(tail){
		errs(" ");
		errs(tail);
	}
	errs("\n");
	setstatus("bad flags");
	Exit();
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
static char buf[NBUF], *bufp = buf;

static void
errc(int c)
{
	*bufp++=c;
	if(bufp==&buf[NBUF] || c=='\n'){
		Write(2, buf, bufp-buf);
		bufp = buf;
	}
}
