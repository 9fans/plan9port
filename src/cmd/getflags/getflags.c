/*% cyntax % && cc -go # %
 * getflags: process flags for command files
 * Usage: ifs='' eval `{getflags [-s] flagfmt [arg ...]}	# rc
 * Usage: IFS=   eval `getflags -b [-s] flagfmt [arg...]`	# Bourne shell
 *	-b means give Bourne-shell compatible output
 */
#include <u.h>
#include <libc.h>
#include "getflags.h"

/* predefine functions */
void bourneprint(int, char *[]);
void bournearg(char *);
void rcprint(int, char *[]);
void usmsg(char *);
int count(int, char *);
void rcarg(char *);

void
main(int argc, char *argv[])
{
	int bourne;
	argc=getflags(argc, argv, "b");
	if(argc<2) usage("flagfmt [arg ...]");
	bourne=flag['b']!=0;
	flag['b']=0;
	if((argc=getflags(argc-1, argv+1, argv[1]))<0){
		usmsg(argv[1]);
		exits(0);
	}
	if(bourne) bourneprint(argc, argv);
	else rcprint(argc, argv);
	exits(0);
}
void
bourneprint(int argc, char *argv[])
{
	register int c, i, n;
	for(c=0;c!=NFLAG;c++) if(flag[c]){
		print("FLAG%c=", c);		/* bug -- c could be a bad char */
		n=count(c, argv[1]);
		if(n==0)
			print("1\n");
		else{
			print("'");
			bournearg(flag[c][0]);
			for(i=1;i!=n;i++){
				print(" ");
				bournearg(flag[c][i]);
			}
			print("'\n");
		}
	}
	print("set --");
	for(c=1;c!=argc;c++){
		print(" ");
		bournearg(argv[c+1]);
	}
	print("\n");
}
void
bournearg(char *s)
{
	for(;*s;s++)
		if(*s=='\'')
			print("'\\''");
		else
			print("%c", *s);
}
void
rcprint(int argc, char *argv[])
{
	register c, i, n;
	for(c=0;c!=NFLAG;c++) if(flag[c]){
		print("FLAG%c=", c);		/* bug -- c could be a bad char */
		n=count(c, argv[1]);
		if(n==0)
			print("''");
		else if(n==1)
			rcarg(flag[c][0]);
		else{
			print("(");
			rcarg(flag[c][0]);
			for(i=1;i!=n;i++){
				print(" ");
				rcarg(flag[c][i]);
			}
			print(")");
		}
		print("\n");
	}
	print("*=");
	if(argc==1) print("()");
	else if(argc==2) rcarg(argv[2]);
	else{
		print("(");
		rcarg(argv[2]);
		for(c=2;c!=argc;c++){
			print(" ");
			rcarg(argv[c+1]);
		}
		print(")");
	}
	print("\n");
}
void
usmsg(char *flagarg)
{
	register char *s, *t, c;
	register count, nflag=0;
	print("echo Usage: $0'");
	for(s=flagarg;*s;){
		c=*s;
		if(*s++==' ') continue;
		if(*s==':')
			count = strtol(++s, &s, 10);
		else count=0;
		if(count==0){
			if(nflag==0) print(" [-");
			nflag++;
			print("%c", c);
		}
		if(*s=='['){
			int depth=1;
			s++;
			for(;*s!='\0' && depth>0; s++)
				if (*s==']') depth--;
				else if (*s=='[') depth++;
		}
	}
	if(nflag) print("]");
	for(s=flagarg;*s;){
		c=*s;
		if(*s++==' ') continue;
		if(*s==':')
			count = strtol(++s, &s, 10);
		else count=0;
		if(count!=0){
			print(" [-");
			print("%c", c);
			if(*s=='['){
				int depth=1;
				s++;
				t=s;
				for(;*s!='\0' && depth>0; s++)
					if (*s==']') depth--;
					else if (*s=='[') depth++;
				print(" ");
				write(1, t, s - t);
			}
			else
				while(count--) print(" arg");
			print("]");
		}
		else if(*s=='['){
			int depth=1;
			s++;
			for(;*s!='\0' && depth>0; s++)
				if (*s==']') depth--;
				else if (*s=='[') depth++;
		}
	}
	print("' $usage;\n");
	print("exit 'usage'\n");
}
int
count(int flag, char *flagarg)
{
	register char *s, c;
	register n;
	for(s=flagarg;*s;){
		c=*s;
		if(*s++==' ') continue;
		if(*s==':')
			n = strtol(++s, &s, 10);
		else n=0;
		if(*s=='['){
			int depth=1;
			s++;
			for(;*s!='\0' && depth>0; s++)
				if (*s==']') depth--;
				else if (*s=='[') depth++;
		}
		if(c==flag) return n;
	}
	return -1;		/* never happens */
}
void
rcarg(char *s)
{
	if(*s=='\0' || strpbrk(s, "\n \t#;&|^$=`'{}()<>?")){
		print("\'");
		for(;*s;s++)
			if(*s=='\'') print("''");
			else print("%c", *s);
		print("\'");
	}
	else	print("%s", s);
}
