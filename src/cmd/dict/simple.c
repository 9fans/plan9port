#include <u.h>
#include <libc.h>
#include <bio.h>
#include "dict.h"

/*
 * Routines for handling dictionaries in UTF, headword
 * separated from entry by tab, entries separated by newline.
 */

void
simpleprintentry(Entry e, int cmd)
{
	uchar *p, *pe;

	p = (uchar *)e.start;
	pe = (uchar *)e.end;
	while(p < pe){
		if(*p == '\t'){
			if(cmd == 'h')
				break;
			else
				outchar(' '), ++p;
		}else if(*p == '\n')
			break;
		else
			outchar(*p++);
	}
	outnl(0);
}

long
simplenextoff(long fromoff)
{
	if(Bseek(bdict, fromoff, 0) < 0)
		return -1;
	if(Brdline(bdict, '\n') == 0)
		return -1;
	return Boffset(bdict);
}

void
simpleprintkey(void)
{
	Bprint(bout, "No pronunciation key.\n");
}
