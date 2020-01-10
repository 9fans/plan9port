#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include "dict.h"

/* Roget's Thesaurus from project Gutenberg */

/* static long Last = 0; */

void
rogetprintentry(Entry e, int cmd)
{
	int spc;
	char c, *p;

	spc = 0;
	p = e.start;

	if(cmd == 'h'){
		while(!isspace((uchar)*p) && p < e.end)
			p++;
		while(strncmp(p, " -- ", 4) != 0 && p < e.end){
			while(isspace((uchar)*p) && p < e.end)
				p++;
			if (*p == '[' || *p == '{'){
				c = (*p == '[')? ']': '}';
				while(*p != c && p < e.end)
					p++;
				p++;
				continue;
			}
			if (isdigit((uchar)*p) || ispunct((uchar)*p)){
				while(!isspace((uchar)*p) && p < e.end)
					p++;
				continue;
			}


			if (isspace((uchar)*p))
				spc = 1;
			else
			if (spc){
				outchar(' ');
				spc = 0;
			}

			while(!isspace((uchar)*p) && p < e.end)
				outchar(*p++);
		}
		return;
	}

	while(p < e.end && !isspace((uchar)*p))
		p++;
	while(p < e.end && isspace((uchar)*p))
		p++;

	while (p < e.end){
		if (p < e.end -4 && strncmp(p, " -- ", 4) == 0){	/* first line */
			outnl(2);
			p += 4;
			spc = 0;
		}

		if (p < e.end -2 && strncmp(p, "[ ", 4) == 0){		/* twiddle layout */
			outchars(" [");
			continue;
		}

		if (p < e.end -4 && strncmp(p, "&c (", 4) == 0){	/* usefull xref */
			if (spc)
				outchar(' ');
			outchar('/');
			while(p < e.end && *p != '(')
				p++;
			p++;
			while(p < e.end && *p != ')')
				outchar(*p++);
			p++;
			while(p < e.end && isspace((uchar)*p))
				p++;
			while(p < e.end && isdigit((uchar)*p))
				p++;
			outchar('/');
			continue;
		}

		if (p < e.end -3 && strncmp(p, "&c ", 3) == 0){		/* less usefull xref */
			while(p < e.end && !isdigit((uchar)*p))
				p++;
			while(p < e.end && isdigit((uchar)*p))
				p++;
			continue;
		}

		if (*p == '\n' && p < (e.end -1)){			/* their newlines */
			spc = 0;
			p++;
			if (isspace((uchar)*p)){				/* their continuation line */
				while (isspace((uchar)*p))
					p++;
				p--;
			}
			else{
				outnl(2);
			}
		}
		if (spc && *p != ';' && *p != '.' &&
		    *p != ',' && !isspace((uchar)*p)){				/* drop spaces before punct */
			spc = 0;
			outchar(' ');
		}
		if (isspace((uchar)*p))
			spc = 1;
		else
			outchar(*p);
		p++;
	}
	outnl(0);
}

long
rogetnextoff(long fromoff)
{
	int i;
	vlong l;
	char *p;

	Bseek(bdict, fromoff, 0);
	Brdline(bdict, '\n');
	while ((p = Brdline(bdict, '\n')) != nil){
		l = Blinelen(bdict);
		if (!isdigit((uchar)*p))
			continue;
		for (i = 0; i < l-4; i++)
			if (strncmp(p+i, " -- ", 4) == 0)
				return Boffset(bdict)-l;
	}
	return Boffset(bdict);
}

void
rogetprintkey(void)
{
	Bprint(bout, "No pronunciation key.\n");
}
