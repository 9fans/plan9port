/*
 * wc -- count things in utf-encoded text files
 * Bugs:
 *	The only white space characters recognized are ' ', '\t' and '\n', even though
 *	ISO 10646 has many more blanks scattered through it.
 *	Should count characters that cannot occur in any rune (hex f0-ff) separately.
 *	Should count non-canonical runes (e.g. hex c1,80 instead of hex 40).
 */
#include <u.h>
#include <libc.h>
#define	NBUF	(8*1024)
uvlong nline, tnline, pline;
uvlong nword, tnword, pword;
uvlong nrune, tnrune, prune;
uvlong nbadr, tnbadr, pbadr;
uvlong nchar, tnchar, pchar;
void count(int, char *);
void report(uvlong, uvlong, uvlong, uvlong, uvlong, char *);
void
main(int argc, char *argv[])
{
	char *status="";
	int i, f;
	ARGBEGIN {
	case 'l': pline++; break;
	case 'w': pword++; break;
	case 'r': prune++; break;
	case 'b': pbadr++; break;
	case 'c': pchar++; break;
	default:
		fprint(2, "Usage: %s [-lwrbc] [file ...]\n", argv0);
		exits("usage");
	} ARGEND
	if(pline+pword+prune+pbadr+pchar == 0) {
		pline = 1;
		pword = 1;
		pchar = 1;
	}
	if(argc==0)
		count(0, 0);
	else{
		for(i=0;i<argc;i++){
			f=open(argv[i], OREAD);
			if(f<0){
				perror(argv[i]);
				status="can't open";
			}
			else{
				count(f, argv[i]);
				tnline+=nline;
				tnword+=nword;
				tnrune+=nrune;
				tnbadr+=nbadr;
				tnchar+=nchar;
				close(f);
			}
		}
		if(argc>1)
			report(tnline, tnword, tnrune, tnbadr, tnchar, "total");
	}
	exits(status);
}
void
report(uvlong nline, uvlong nword, uvlong nrune, uvlong nbadr, uvlong nchar, char *fname)
{
	char line[1024], word[128];
	line[0] = '\0';
	if(pline){
		sprint(word, " %7llud", nline);
		strcat(line, word);
	}
	if(pword){
		sprint(word, " %7llud", nword);
		strcat(line, word);
	}
	if(prune){
		sprint(word, " %7llud", nrune);
		strcat(line, word);
	}
	if(pbadr){
		sprint(word, " %7llud", nbadr);
		strcat(line, word);
	}
	if(pchar){
		sprint(word, " %7llud", nchar);
		strcat(line, word);
	}
	if(fname){
		sprint(word, " %s",   fname);
		strcat(line, word);
	}
	print("%s\n", line+1);
}
/*
 * How it works.  Start in statesp.  Each time we read a character,
 * increment various counts, and do state transitions according to the
 * following table.  If we're not in statesp or statewd when done, the
 * file ends with a partial rune.
 *        |                character
 *  state |09,20| 0a  |00-7f|80-bf|c0-df|e0-ef|f0-ff
 * -------+-----+-----+-----+-----+-----+-----+-----
 * statesp|ASP  |ASPN |AWDW |AWDWX|AC2W |AC3W |AWDWX
 * statewd|ASP  |ASPN |AWD  |AWDX |AC2  |AC3  |AWDX
 * statec2|ASPX |ASPNX|AWDX |AWDR |AC2X |AC3X |AWDX
 * statec3|ASPX |ASPNX|AWDX |AC2R |AC2X |AC3X |AWDX
 */
enum{			/* actions */
	AC2,		/* enter statec2 */
	AC2R,		/* enter statec2, don't count a rune */
	AC2W,		/* enter statec2, count a word */
	AC2X,		/* enter statec2, count a bad rune */
	AC3,		/* enter statec3 */
	AC3W,		/* enter statec3, count a word */
	AC3X,		/* enter statec3, count a bad rune */
	ASP,		/* enter statesp */
	ASPN,		/* enter statesp, count a newline */
	ASPNX,		/* enter statesp, count a newline, count a bad rune */
	ASPX,		/* enter statesp, count a bad rune */
	AWD,		/* enter statewd */
	AWDR,		/* enter statewd, don't count a rune */
	AWDW,		/* enter statewd, count a word */
	AWDWX,		/* enter statewd, count a word, count a bad rune */
	AWDX,		/* enter statewd, count a bad rune */
};
uchar statesp[256]={	/* looking for the start of a word */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 00-07 */
AWDW, ASP,  ASPN, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 08-0f */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 10-17 */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 18-1f */
ASP,  AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 20-27 */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 28-2f */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 30-37 */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 38-3f */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 40-47 */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 48-4f */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 50-57 */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 58-5f */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 60-67 */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 68-6f */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 70-77 */
AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW, AWDW,	/* 78-7f */
AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,/* 80-87 */
AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,/* 88-8f */
AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,/* 90-97 */
AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,/* 98-9f */
AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,/* a0-a7 */
AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,/* a8-af */
AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,/* b0-b7 */
AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,/* b8-bf */
AC2W, AC2W, AC2W, AC2W, AC2W, AC2W, AC2W, AC2W,	/* c0-c7 */
AC2W, AC2W, AC2W, AC2W, AC2W, AC2W, AC2W, AC2W,	/* c8-cf */
AC2W, AC2W, AC2W, AC2W, AC2W, AC2W, AC2W, AC2W,	/* d0-d7 */
AC2W, AC2W, AC2W, AC2W, AC2W, AC2W, AC2W, AC2W,	/* d8-df */
AC3W, AC3W, AC3W, AC3W, AC3W, AC3W, AC3W, AC3W,	/* e0-e7 */
AC3W, AC3W, AC3W, AC3W, AC3W, AC3W, AC3W, AC3W,	/* e8-ef */
AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,/* f0-f7 */
AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,AWDWX,/* f8-ff */
};
uchar statewd[256]={	/* looking for the next character in a word */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 00-07 */
AWD,  ASP,  ASPN, AWD,  AWD,  AWD,  AWD,  AWD,	/* 08-0f */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 10-17 */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 18-1f */
ASP,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 20-27 */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 28-2f */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 30-37 */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 38-3f */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 40-47 */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 48-4f */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 50-57 */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 58-5f */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 60-67 */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 68-6f */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 70-77 */
AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,  AWD,	/* 78-7f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 80-87 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 88-8f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 90-97 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 98-9f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* a0-a7 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* a8-af */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* b0-b7 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* b8-bf */
AC2,  AC2,  AC2,  AC2,  AC2,  AC2,  AC2,  AC2,	/* c0-c7 */
AC2,  AC2,  AC2,  AC2,  AC2,  AC2,  AC2,  AC2,	/* c8-cf */
AC2,  AC2,  AC2,  AC2,  AC2,  AC2,  AC2,  AC2,	/* d0-d7 */
AC2,  AC2,  AC2,  AC2,  AC2,  AC2,  AC2,  AC2,	/* d8-df */
AC3,  AC3,  AC3,  AC3,  AC3,  AC3,  AC3,  AC3,	/* e0-e7 */
AC3,  AC3,  AC3,  AC3,  AC3,  AC3,  AC3,  AC3,	/* e8-ef */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* f0-f7 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* f8-ff */
};
uchar statec2[256]={	/* looking for 10xxxxxx to complete a rune */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 00-07 */
AWDX, ASPX, ASPNX,AWDX, AWDX, AWDX, AWDX, AWDX,	/* 08-0f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 10-17 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 18-1f */
ASPX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 20-27 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 28-2f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 30-37 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 38-3f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 40-47 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 48-4f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 50-57 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 58-5f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 60-67 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 68-6f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 70-77 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 78-7f */
AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR,	/* 80-87 */
AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR,	/* 88-8f */
AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR,	/* 90-97 */
AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR,	/* 98-9f */
AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR,	/* a0-a7 */
AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR,	/* a8-af */
AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR,	/* b0-b7 */
AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR, AWDR,	/* b8-bf */
AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X,	/* c0-c7 */
AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X,	/* c8-cf */
AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X,	/* d0-d7 */
AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X,	/* d8-df */
AC3X, AC3X, AC3X, AC3X, AC3X, AC3X, AC3X, AC3X,	/* e0-e7 */
AC3X, AC3X, AC3X, AC3X, AC3X, AC3X, AC3X, AC3X,	/* e8-ef */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* f0-f7 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* f8-ff */
};
uchar statec3[256]={	/* looking for 10xxxxxx,10xxxxxx to complete a rune */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 00-07 */
AWDX, ASPX, ASPNX,AWDX, AWDX, AWDX, AWDX, AWDX,	/* 08-0f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 10-17 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 18-1f */
ASPX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 20-27 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 28-2f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 30-37 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 38-3f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 40-47 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 48-4f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 50-57 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 58-5f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 60-67 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 68-6f */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 70-77 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* 78-7f */
AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R,	/* 80-87 */
AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R,	/* 88-8f */
AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R,	/* 90-97 */
AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R,	/* 98-9f */
AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R,	/* a0-a7 */
AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R,	/* a8-af */
AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R,	/* b0-b7 */
AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R, AC2R,	/* b8-bf */
AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X,	/* c0-c7 */
AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X,	/* c8-cf */
AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X,	/* d0-d7 */
AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X, AC2X,	/* d8-df */
AC3X, AC3X, AC3X, AC3X, AC3X, AC3X, AC3X, AC3X,	/* e0-e7 */
AC3X, AC3X, AC3X, AC3X, AC3X, AC3X, AC3X, AC3X,	/* e8-ef */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* f0-f7 */
AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX, AWDX,	/* f8-ff */
};
void
count(int f, char *name)
{
	int n;
	uchar buf[NBUF];
	uchar *bufp, *ebuf;
	uchar *state=statesp;

	nline = 0;
	nword = 0;
	nrune = 0;
	nbadr = 0;
	nchar = 0;

	for(;;){
		n=read(f, buf, NBUF);
		if(n<=0)
			break;
		nchar+=n;
		nrune+=n;	/* might be too large, gets decreased later */
		bufp=buf;
		ebuf=buf+n;
		do{
			switch(state[*bufp]){
			case AC2:   state=statec2;                   break;
			case AC2R:  state=statec2; --nrune;          break;
			case AC2W:  state=statec2; nword++;          break;
			case AC2X:  state=statec2;          nbadr++; break;
			case AC3:   state=statec3;                   break;
			case AC3W:  state=statec3; nword++;          break;
			case AC3X:  state=statec3;          nbadr++; break;
			case ASP:   state=statesp;                   break;
			case ASPN:  state=statesp; nline++;          break;
			case ASPNX: state=statesp; nline++; nbadr++; break;
			case ASPX:  state=statesp;          nbadr++; break;
			case AWD:   state=statewd;                   break;
			case AWDR:  state=statewd; --nrune;          break;
			case AWDW:  state=statewd; nword++;          break;
			case AWDWX: state=statewd; nword++; nbadr++; break;
			case AWDX:  state=statewd;          nbadr++; break;
			}
		}while(++bufp!=ebuf);
	}
	if(state!=statesp && state!=statewd)
		nbadr++;
	if(n<0)
		perror(name);
	report(nline, nword, nrune, nbadr, nchar, name);
}
