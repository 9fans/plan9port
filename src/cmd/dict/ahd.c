#include <u.h>
#include <libc.h>
#include <bio.h>
#include "dict.h"

/*
 * American Heritage Dictionary (encrypted)
 */

static Rune intab[256];

static void
initintab(void)
{
	intab[0x82] =  0xe9;
	intab[0x85] =  0xe0;
	intab[0x89] =  0xeb;
	intab[0x8a] =  0xe8;
	intab[0xa4] =  0xf1;
	intab[0xf8] =  0xb0;
	intab[0xf9] =  0xb7;
}

static char	tag[64];

enum{
	Run, Openper, Openat, Closeat
};

void
ahdprintentry(Entry e, int cmd)
{
	static int inited;
	long addr;
	char *p, *t = tag;
	int obreaklen;
	int c, state = Run;

	if(!inited){
		initintab();
		for(c=0; c<256; c++)
			if(intab[c] == 0)
				intab[c] = c;
		inited = 1;
	}
	obreaklen = breaklen;
	breaklen = 80;
	addr = e.doff;
	for(p=e.start; p<e.end; p++){
		c = intab[(*p ^ (addr++>>1))&0xff];
		switch(state){
		case Run:
			if(c == '%'){
				t = tag;
				state = Openper;
				break;
			}
		Putchar:
			if(c == '\n')
				outnl(0);
			else if(c < Runeself)
				outchar(c);
			else
				outrune(c);
			break;

		case Openper:
			if(c == '@')
				state = Openat;
			else{
				outchar('%');
				state = Run;
				goto Putchar;
			}
			break;

		case Openat:
			if(c == '@')
				state = Closeat;
			else if(t < &tag[sizeof tag-1])
				*t++ = c;
			break;

		case Closeat:
			if(c == '%'){
				*t = 0;
				switch(cmd){
				case 'h':
					if(strcmp("EH", tag) == 0)
						goto out;
					break;
				case 'r':
					outprint("%%@%s@%%", tag);
					break;
				}
				state = Run;
			}else{
				if(t < &tag[sizeof tag-1])
					*t++ = '@';
				if(t < &tag[sizeof tag-1])
					*t++ = c;
				state = Openat;
			}
			break;
		}
	}
out:
	outnl(0);
	breaklen = obreaklen;
}

long
ahdnextoff(long fromoff)
{
	static char *patterns[] = { "%@NL@%", "%@2@%", 0 };
	int c, k = 0, state = 0;
	char *pat = patterns[0];
	long defoff = -1;

	if(Bseek(bdict, fromoff, 0) < 0)
		return -1;
	while((c = Bgetc(bdict)) >= 0){
		c ^= (fromoff++>>1)&0xff;
		if(c != pat[state]){
			state = 0;
			continue;
		}
		if(pat[++state])
			continue;
		if(pat = patterns[++k]){	/* assign = */
			state = 0;
			defoff = fromoff-6;
			continue;
		}
		return fromoff-5;
	}
	return defoff;
}

void
ahdprintkey(void)
{
	Bprint(bout, "No pronunciations.\n");
}
