#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>

Biobuf in;
Biobuf out;

enum
{
	Empty,
	Sys,
	Dk,
	Ip,
	Domain,
};

int
iscomment(char *name)
{
	return *name == '#';
}

/*
 *  is this a fully specified datakit name?
 */
int
isdk(char *name)
{
	int slash;

	slash = 0;
	for(; *name; name++){
		if(isalnum(*name))
			continue;
		if(*name == '/'){
			slash = 1;
			continue;
		}
		return 0;
	}
	return slash;
}

/*
 *  Is this an internet domain name?
 */
int
isdomain(char *name)
{
	int dot = 0;
	int alpha = 0;

	for(; *name; name++){
		if(isalpha(*name) || *name == '-'){
			alpha = 1;
			continue;
		}
		if(*name == '.'){
			dot = 1;
			continue;
		}
		if(isdigit(*name))
			continue;
		return 0;
	}
	return dot && alpha;
}

/*
 *  is this an ip address?
 */
int
isip(char *name)
{
	int dot = 0;

	for(; *name; name++){
		if(*name == '.'){
			dot = 1;
			continue;
		}
		if(isdigit(*name))
			continue;
		return 0;
	}
	return dot;
}

char tup[64][64];
int ttype[64];
int ntup;

void
tprint(void)
{
	int i, tab;
	char *p;

	tab = 0;
	for(i = 0; i < ntup; i++){
		if(ttype[i] == Sys){
			Bprint(&out, "sys = %s\n", tup[i]);
			tab = 1;
			ttype[i] = Empty;
			break;
		}
	}
	for(i = 0; i < ntup; i++){
		if(ttype[i] == Empty)
			continue;
		if(tab)
			Bprint(&out, "\t");
		tab = 1;

		switch(ttype[i]){
		case Domain:
			Bprint(&out, "dom=%s\n", tup[i]);
			break;
		case Ip:
			Bprint(&out, "ip=%s\n", tup[i]);
			break;
		case Dk:
			p = strrchr(tup[i], '/');
			if(p){
				p++;
				if((*p == 'C' || *p == 'R')
				&& strncmp(tup[i], "nj/astro/", p-tup[i]) == 0)
					Bprint(&out, "flavor=console ");
			}
			Bprint(&out, "dk=%s\n", tup[i]);
			break;
		case Sys:
			Bprint(&out, "sys=%s\n", tup[i]);
			break;
		}
	}
}

#define NFIELDS 64

/*
 *  make a database file from a merged uucp/inet database
 */
void
main(void)
{
	int n, i, j;
	char *l;
	char *fields[NFIELDS];
	int ftype[NFIELDS];
	int same, match;

	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	ntup = 0;
	while(l = Brdline(&in, '\n')){
		l[Blinelen(&in)-1] = 0;
		n = getfields(l, fields, NFIELDS, 1, " \t");
		same = 0;
		for(i = 0; i < n; i++){
			if(iscomment(fields[i])){
				n = i;
				break;
			}
			if(isdomain(fields[i])){
				ftype[i] = Domain;
				for(j = 0; j < ntup; j++)
					if(ttype[j] == Domain && strcmp(fields[i], tup[j]) == 0){
						same = 1;
						ftype[i] = Empty;
						break;
					}
			} else if(isip(fields[i]))
				ftype[i] = Ip;
			else if(isdk(fields[i]))
				ftype[i] = Dk;
			else
				ftype[i] = Sys;
		}
		if(!same && ntup){
			tprint();
			ntup = 0;
		}
		for(i = 0; i < n; i++){
			match = 0;
			for(j = 0; j < ntup; j++){
				if(ftype[i] == ttype[j] && strcmp(fields[i], tup[j]) == 0){
					match = 1;
					break;
				}
			}
			if(!match){
				ttype[ntup] = ftype[i];
				strcpy(tup[ntup], fields[i]);
				ntup++;
			}
		}
	}
	if(ntup)
		tprint();
	exits(0);
}
