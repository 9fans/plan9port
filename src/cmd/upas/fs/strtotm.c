#include <u.h>
#include <libc.h>
#include <ctype.h>

static char*
skiptext(char *q)
{
	while(*q!='\0' && *q!=' ' && *q!='\t' && *q!='\r' && *q!='\n')
		q++;
	return q;
}

static char*
skipwhite(char *q)
{
	while(*q==' ' || *q=='\t' || *q=='\r' || *q=='\n')
		q++;
	return q;
}

static char* months[] = {
	"jan", "feb", "mar", "apr",
	"may", "jun", "jul", "aug",
	"sep", "oct", "nov", "dec"
};

static int
strcmplwr(char *a, char *b, int n)
{
	char *eb;

	eb = b+n;
	while(*a && *b && b<eb){
		if(tolower(*a) != tolower(*b))
			return 1;
		a++;
		b++;
	}
	if(b==eb)
		return 0;
	return *a != *b;
}

int
strtotm(char *p, Tm *tmp)
{
	char *q, *r;
	int j;
	Tm tm;
	int delta;

	delta = 0;
	memset(&tm, 0, sizeof(tm));
	tm.mon = -1;
	tm.hour = -1;
	tm.min = -1;
	tm.year = -1;
	tm.mday = -1;
	for(p=skipwhite(p); *p; p=skipwhite(q)){
		q = skiptext(p);

		/* look for time in hh:mm[:ss] */
		if(r = memchr(p, ':', q-p)){
			tm.hour = strtol(p, 0, 10);
			tm.min = strtol(r+1, 0, 10);
			if(r = memchr(r+1, ':', q-(r+1)))
				tm.sec = strtol(r+1, 0, 10);
			else
				tm.sec = 0;
			continue;
		}

		/* look for month */
		for(j=0; j<12; j++)
			if(strcmplwr(p, months[j], 3)==0){
				tm.mon = j;
				break;
			}

		if(j!=12)
			continue;

		/* look for time zone [A-Z][A-Z]T */
		if(q-p==3 && 'A' <= p[0] && p[0] <= 'Z'
		&& 'A' <= p[1] && p[1] <= 'Z' && p[2] == 'T'){
			strecpy(tm.zone, tm.zone+4, p);
			continue;
		}

		if(p[0]=='+'||p[0]=='-')
		if(q-p==5 && strspn(p+1, "0123456789") == 4){
			delta = (((p[1]-'0')*10+p[2]-'0')*60+(p[3]-'0')*10+p[4]-'0')*60;
			if(p[0] == '-')
				delta = -delta;
			continue;
		}
		if(strspn(p, "0123456789") == q-p){
			j = strtol(p, nil, 10);
			if(1 <= j && j <= 31)
				tm.mday = j;
			if(j >= 1900)
				tm.year = j-1900;
		}
	}

	if(tm.mon<0 || tm.year<0
	|| tm.hour<0 || tm.min<0
	|| tm.mday<0)
		return -1;

	*tmp = *localtime(tm2sec(&tm)-delta);
	return 0;
}
