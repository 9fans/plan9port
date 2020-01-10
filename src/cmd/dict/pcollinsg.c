#include <u.h>
#include <libc.h>
#include <bio.h>
#include "dict.h"

/*
 * Routines for handling dictionaries in the "Paperback Collins"
 * `German' format (with tags surrounded by \5⋯\6 and \xba⋯\xba)
 */

/*
 *	\5...\6 escapes (fonts, mostly)
 *
 *	h	headword (helvetica 7 pt)
 *	c	clause (helvetica 7 pt)
 *	3	helvetica 7 pt
 *	4	helvetica 6.5 pt
 *	s	helvetica 8 pt
 *	x	helvetica 8 pt
 *	y	helvetica 5 pt
 *	m	helvetica 30 pt
 *	1	roman 6 pt
 *	9	roman 4.5 pt
 *	p	roman 7 pt
 *	q	roman 4.5 pt
 *	2	italic 6 pt
 *	7	italic 4.5 pt
 *	b	bold 6 pt
 *	a	`indent 0:4 left'
 *	k	`keep 9'
 *	l	`size 12'
 */

enum {
	IBASE=0x69,	/* dotless i */
	Taglen=32
};

static Rune intab[256] = {
	/*0*/	/*1*/	/*2*/	/*3*/	/*4*/	/*5*/	/*6*/	/*7*/
/*00*/	NONE,	NONE,	NONE,	NONE,	NONE,	TAGS,	TAGE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	0x20,	NONE,	NONE,
/*10*/	NONE,	0x2d,	0x20,	0x20,	NONE,	NONE,	NONE,	NONE,
	0x20,	NONE,	NONE,	NONE,	0x20,	NONE,	NONE,	0x2d,
/*20*/	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	0x26,	'\'',
	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
/*30*/  0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,
	0x38,	0x39,	0x3a,	0x3b,	0x3c,	0x3d,	0x3e,	0x3f,
/*40*/  0x40,	0x41,	0x42,	0x43,	0x44,	0x45,	0x46,	0x47,
	0x48,	0x49,	0x4a,	0x4b,'L',	0x4d,	0x4e,	0x4f,
/*50*/	0x50,	0x51,	0x52,	0x53,	0x54,	0x55,	0x56,	0x57,
	0x58,	0x59,	0x5a,	0x5b,'\\',	0x5d,	0x5e,	0x5f,
/*60*/	0x60,	0x61,	0x62,	0x63,	0x64,	0x65,	0x66,	0x67,
	0x68,	0x69,	0x6a,	0x6b,	0x6c,	0x6d,	0x6e,	0x6f,
/*70*/	0x70,	0x71,	0x72,	0x73,	0x74,	0x75,	0x76,	0x77,
	0x78,	0x79,	0x7a,	0x7b,	0x7c,	0x7d,	0x7e,	NONE,
/*80*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	0x20,	NONE,	NONE,	NONE,	NONE,	NONE,
/*90*/	0xdf,	0xe6,	NONE,	MOE,	NONE,	NONE,	NONE,	0xf8,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*A0*/	NONE,	NONE,	0x22,	0xa3,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*B0*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	0x7e,
	NONE,	IBASE,	SPCS,	NONE,	NONE,	NONE,	NONE,	NONE,
/*C0*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*D0*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*E0*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*F0*/	0x20,	0x20,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE
};

static Nassoc numtab[] = {
	{1,	0x2b},
	{4,	0x3d},
	{7,	0xb0},
	{11,	0x2248},
	{69,	0x2666},
	{114,	0xae},
	{340,	0x25b},
	{341,	0x254},
	{342,	0x28c},
	{343,	0x259},
	{345,	0x292},
	{346,	0x283},
	{347,	0x275},
	{348,	0x28a},
	{349,	0x2c8},
	{351,	0x26a},
	{352,	0x25c},
	{354,	0x251},
	{355,	0x7e},
	{356,	0x252},
	{384,	0x273},
	{445,	0xf0},	/* BUG -- should be script eth */
};

static Nassoc overtab[] = {
	{0x2c,	LCED},
	{0x2f,	LACU},
	{0x3a,	LUML},
	{'\\',	LGRV},
	{0x5e,	LFRN},
	{0x7e,	LTIL}
};

static uchar *reach(uchar*, int);

static Entry	curentry;
static char	tag[Taglen];

void
pcollgprintentry(Entry e, int cmd)
{
	uchar *p, *pe;
	int r, rprev = NONE, rx, over = 0, font;
	char buf[16];

	p = (uchar *)e.start;
	pe = (uchar *)e.end;
	curentry = e;
	if(cmd == 'h')
		outinhibit = 1;
	while(p < pe){
		if(cmd == 'r'){
			outchar(*p++);
			continue;
		}
		switch(r = intab[*p++]){	/* assign = */
		case TAGS:
			if(rprev != NONE){
				outrune(rprev);
				rprev = NONE;
			}
			p = reach(p, 0x06);
			font = tag[0];
			if(cmd == 'h')
				outinhibit = (font != 'h');
			break;

		case TAGE:	/* an extra one */
			break;

		case SPCS:
			p = reach(p, 0xba);
			r = looknassoc(numtab, asize(numtab), strtol(tag,0,0));
			if(r < 0){
				if(rprev != NONE){
					outrune(rprev);
					rprev = NONE;
				}
				sprint(buf, "\\N'%s'", tag);
				outchars(buf);
				break;
			}
			/* else fall through */

		default:
			if(over){
				rx = looknassoc(overtab, asize(overtab), r);
				if(rx > 0)
					rx = liglookup(rx, rprev);
				if(rx > 0 && rx != NONE)
					outrune(rx);
				else{
					outrune(rprev);
					if(r == ':')
						outrune(0xa8);
					else{
						outrune(0x5e);
						outrune(r);
					}
				}
				over = 0;
				rprev = NONE;
			}else if(r == '^'){
				over = 1;
			}else{
				if(rprev != NONE)
					outrune(rprev);
				rprev = r;
			}
		}

	}
	if(rprev != NONE)
		outrune(rprev);
	if(cmd == 'h')
		outinhibit = 0;
	outnl(0);
}

long
pcollgnextoff(long fromoff)
{
	int c, state = 0, defoff = -1;

	if(Bseek(bdict, fromoff, 0) < 0)
		return -1;
	while((c = Bgetc(bdict)) >= 0){
		if(c == '\r')
			defoff = Boffset(bdict);
		switch(state){
		case 0:
			if(c == 0x05)
				state = 1;
			break;
		case 1:
			if(c == 'h')
				state = 2;
			else
				state = 0;
			break;
		case 2:
			if(c == 0x06)
				return (Boffset(bdict)-3);
			else
				state = 0;
			break;
		}
	}
	return defoff;
}

void
pcollgprintkey(void)
{
	Bprint(bout, "No pronunciation key yet\n");
}

static uchar *
reach(uchar *p, int tagchar)
{
	int c; char *q=tag;

	while(p < (uchar *)curentry.end){
		c = *p++;
		if(c == tagchar)
			break;
		*q++ = c;
		if(q >= &tag[sizeof tag-1])
			break;
	}
	*q = 0;
	return p;
}
