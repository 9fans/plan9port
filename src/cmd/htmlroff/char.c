#include "a.h"

/*
 * Translate Unicode to HTML by asking tcs(1).
 * This way we don't have yet another table.
 */
Rune*
rune2html(Rune r)
{
	static Biobuf b;
	static int fd = -1;
	static Rune **tcscache[256];
	int p[2];
	char *q;

	if(r == '\n')
		return L("\n");

	if(tcscache[r>>8] && tcscache[r>>8][r&0xFF])
		return tcscache[r>>8][r&0xFF];

	if(fd < 0){
		if(pipe(p) < 0)
			sysfatal("pipe: %r");
		switch(fork()){
		case -1:
			sysfatal("fork: %r");
		case 0:
			dup(p[0], 0);
			dup(p[0], 1);
			close(p[1]);
			execl("tcs", "tcs", "-t", "html", nil);
			_exits(0);
		default:
			close(p[0]);
			fd = p[1];
			Binit(&b, fd, OREAD);
			break;
		}
	}
	fprint(fd, "%C\n", r);
	q = Brdline(&b, '\n');
	if(q == nil)
		sysfatal("tcs: early eof");
	q[Blinelen(&b)-1] = 0;
	if(tcscache[r>>8] == nil)
		tcscache[r>>8] = emalloc(256*sizeof tcscache[0][0]);
	tcscache[r>>8][r&0xFF] = erunesmprint("%s", q);
	return tcscache[r>>8][r&0xFF];
}

/*
 * Translate troff to Unicode by looking in troff's utfmap.
 * This way we don't have yet another hard-coded table.
 */
typedef struct Trtab Trtab;
struct Trtab
{
	char t[3];
	Rune r;
};

static Trtab trtab[200];
int ntrtab;

static Trtab trinit[] =
{
	"pl",		Upl,
	"eq",	Ueq,
	"em",	0x2014,
	"en",	0x2013,
	"mi",	Umi,
	"fm",	0x2032
};

Rune
troff2rune(Rune *rs)
{
	char *file, *f[10], *p, s[3];
	int i, nf;
	Biobuf *b;

	if(rs[0] >= Runeself || rs[1] >= Runeself)
		return Runeerror;
	s[0] = rs[0];
	s[1] = rs[1];
	s[2] = 0;
	if(ntrtab == 0){
		for(i=0; i<nelem(trinit) && ntrtab < nelem(trtab); i++){
			trtab[ntrtab] = trinit[i];
			ntrtab++;
		}
		file = unsharp("#9/troff/font/devutf/utfmap");
		if((b = Bopen(file, OREAD)) == nil)
			sysfatal("open %s: %r", file);
		while((p = Brdline(b, '\n')) != nil){
			p[Blinelen(b)-1] = 0;
			nf = getfields(p, f, nelem(f), 0, "\t");
			for(i=0; i+2<=nf && ntrtab<nelem(trtab); i+=2){
				chartorune(&trtab[ntrtab].r, f[i]);
				memmove(trtab[ntrtab].t, f[i+1], 2);
				ntrtab++;
			}
		}
		Bterm(b);

		if(ntrtab >= nelem(trtab))
			fprint(2, "%s: trtab too small\n", argv0);
	}

	for(i=0; i<ntrtab; i++)
		if(strcmp(s, trtab[i].t) == 0)
			return trtab[i].r;
	return Runeerror;
}
