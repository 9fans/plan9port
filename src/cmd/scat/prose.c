#include <u.h>
#include <libc.h>
#include <bio.h>
#include "sky.h"

extern	Biobuf	bout;

char*
append(char *p, char *s)
{
	while(*s)
		*p++ = *s++;
	return p;
}

int
matchlen(char *a, char *b)
{
	int n;

	for(n=0; *a==*b; a++, b++, n++)
		if(*a == 0)
			return n;
	if(*a == 0)
		return n;
	return 0;
}

char*
prose(char *s, char *desc[][2], short index[])
{
	static char buf[512];
	char *p=buf;
	int i, j, k, max;

	j = 0;
	while(*s){
		if(p >= buf+sizeof buf)
			abort();
		if(*s == ' '){
			if(p>buf && p[-1]!=' ')
				*p++ = ' ';
			s++;
			continue;
		}
		if(*s == ','){
			*p++ = ';', s++;
			continue;
		}
		if(s[0]=='M' && '0'<=s[1] && s[1]<='9'){	/* Messier tag */
			*p++ = *s++;
			continue;	/* below will copy the number */
		}
		if((i=index[(uchar)*s]) == -1){
	Dup:
			switch(*s){
			default:
				while(*s && *s!=',' && *s!=' ')
					*p++=*s++;
				break;

			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				while('0'<=*s && *s<='9')
					*p++ = *s++;
				if(*s=='\'' || *s=='s')
					*p++ = *s++;
				break;

			case '(': case ')':
			case '\'': case '"':
			case '&': case '-': case '+':
				*p++ = *s++;
				break;

			case '*':
				if('0'<=s[1] && s[1]<='9'){
					int flag=0;
					s++;
				Pnumber:
					while('0'<=*s && *s<='9')
						*p++=*s++;
					if(s[0] == '-'){
						*p++ = *s++;
						flag++;
						goto Pnumber;
					}
					if(s[0]==',' && s[1]==' ' && '0'<=s[2] && s[2]<='9'){
						*p++ = *s++;
						s++;	/* skip blank */
						flag++;
						goto Pnumber;
					}
					if(s[0] == '.'){
						if(s[1]=='.' && s[2]=='.'){
							*p++ = '-';
							s += 3;
							flag++;
							goto Pnumber;
						}
						*p++ = *s++;
						goto Pnumber;
					}
					p = append(p, "m star");
					if(flag)
						*p++ = 's';
					*p++ = ' ';
					break;
				}
				if(s[1] == '*'){
					if(s[2] == '*'){
						p = append(p, "triple star ");
						s += 3;
					}else{
						p = append(p, "double star ");
						s += 2;
					}
					break;
				}
				p = append(p, "star ");
				s++;
				break;
			}
			continue;
		}
		for(max=-1; desc[i][0] && desc[i][0][0]==*s; i++){
			k = matchlen(desc[i][0], s);
			if(k > max)
				max = k, j = i;
		}
		if(max == 0)
			goto Dup;
		s += max;
		for(k=0; desc[j][1][k]; k++)
			*p++=desc[j][1][k];
		if(*s == ' ')
			*p++ = *s++;
		else if(*s == ',')
			*p++ = ';', s++;
		else
			*p++ = ' ';
	}
	*p = 0;
	return buf;
}

void
prdesc(char *s, char *desc[][2], short index[])
{
	int c, j;

	if(index[0] == 0){
		index[0] = 1;
		for(c=1, j=0; c<128; c++)
			if(desc[j][0]==0 || desc[j][0][0]>c)
				index[c] = -1;
			else if(desc[j][0][0] == c){
				index[c] = j;
				while(desc[j][0] && desc[j][0][0] == c)
					j++;
				if(j >= NINDEX){
					fprint(2, "scat: internal error: too many prose entries\n");
					exits("NINDEX");
				}
			}
	}
	Bprint(&bout, "\t%s [%s]\n", prose(s, desc, index), s);
}
