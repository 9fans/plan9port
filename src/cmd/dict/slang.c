#include <u.h>
#include <libc.h>
#include <bio.h>
#include "dict.h"

/* Possible tags */
enum {
	DF,		/* definition */
	DX,		/* definition/example */
	ET,		/* etymology */
	EX,		/* example */
	LA,		/* label */
	ME,		/* main entry */
	NU,		/* sense number */
	PR,		/* pronunciation */
	PS,		/* grammar part */
	XR,		/* cross reference */
	XX,		/* cross reference (whole entry) */
};

/* Assoc tables must be sorted on first field */

static Assoc tagtab[] = {
	{"df",	DF},
	{"dx",	DX},
	{"et",	ET},
	{"ex",	EX},
	{"la",	LA},
	{"me",	ME},
	{"nu",	NU},
	{"pr",	PR},
	{"ps",	PS},
	{"xr",	XR},
	{"xx",	XX},
};
static long	sget(char *, char *, char **, char **);
static void	soutpiece(char *, char *);

void
slangprintentry(Entry e, int cmd)
{
	char *p, *pe, *vs, *ve;
	long t;

	p = e.start;
	pe = e.end;
	if(cmd == 'h') {
		t = sget(p, pe, &vs, &ve);
		if(t == ME)
			soutpiece(vs, ve);
		outnl(0);
		return;
	}
	while(p < pe) {
		switch(sget(p, pe, &vs, &ve)) {
		case DF:
			soutpiece(vs, ve);
			outchars(".  ");
			break;
		case DX:
			soutpiece(vs, ve);
			outchars(".  ");
			break;
		case ET:
			outchars("[");
			soutpiece(vs, ve);
			outchars("] ");
			break;
		case EX:
			outchars("E.g., ");
			soutpiece(vs, ve);
			outchars(".  ");
			break;
		case LA:
			outchars("(");
			soutpiece(vs, ve);
			outchars(") ");
			break;
		case ME:
			outnl(0);
			soutpiece(vs, ve);
			outnl(0);
			break;
		case NU:
			outnl(2);
			soutpiece(vs, ve);
			outchars(".  ");
			break;
		case PR:
			outchars("[");
			soutpiece(vs, ve);
			outchars("] ");
			break;
		case PS:
			outnl(1);
			soutpiece(vs, ve);
			outchars(". ");
			break;
		case XR:
			outchars("See ");
			soutpiece(vs, ve);
			outchars(".  ");
			break;
		case XX:
			outchars("See ");
			soutpiece(vs, ve);
			outchars(".  ");
			break;
		default:
			ve = pe;	/* will end loop */
			break;
		}
		p = ve;
	}
	outnl(0);
}

long
slangnextoff(long fromoff)
{
	long a;
	char *p;

	a = Bseek(bdict, fromoff, 0);
	if(a < 0)
		return -1;
	for(;;) {
		p = Brdline(bdict, '\n');
		if(!p)
			break;
		if(p[0] == 'm' && p[1] == 'e' && p[2] == ' ')
			return (Boffset(bdict)-Blinelen(bdict));
	}
	return -1;
}

void
slangprintkey(void)
{
	Bprint(bout, "No key\n");
}

/*
 * Starting from b, find next line beginning with a tag.
 * Don't go past e, but assume *e==0.
 * Return tag value, or -1 if no more tags before e.
 * Set pvb to beginning of value (after tag).
 * Set pve to point at newline that ends the value.
 */
static long
sget(char *b, char *e, char **pvb, char **pve)
{
	char *p;
	char buf[3];
	long t, tans;

	buf[2] = 0;
	tans = -1;
	for(p = b;;) {
		if(p[2] == ' ') {
			buf[0] = p[0];
			buf[1] = p[1];
			t = lookassoc(tagtab, asize(tagtab), buf);
			if(t < 0) {
				if(debug)
					err("tag %s\n", buf);
				p += 3;
			} else {
				if(tans < 0) {
					p += 3;
					tans = t;
					*pvb = p;
				} else {
					*pve = p;
					break;
				}
			}
		}
		p = strchr(p, '\n');
		if(!p || ++p >= e) {
			if(tans >= 0)
				*pve = e-1;
			break;
		}
	}
	return tans;
}

static void
soutpiece(char *b, char *e)
{
	int c, lastc;

	lastc = 0;
	while(b < e) {
		c = *b++;
		if(c == '\n')
			c = ' ';
		if(!(c == ' ' && lastc == ' ') && c != '@')
			outchar(c);
		lastc = c;
	}
}
