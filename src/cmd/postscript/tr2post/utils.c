#include <u.h>
#include <libc.h>
#include <bio.h>
#include "../common/common.h"
#include "tr2post.h"

int hpos = 0, vpos = 0;
int fontsize, fontpos;

#define MAXSTR	128
int trindex;			/* index into trofftab of current troff font */
static int expecthmot = 0;

void
initialize(void) {
}

void
hgoto(int x) {
	hpos = x;
	if (pageon()) {
		endstring();
/*		Bprint(Bstdout, "%d %d m\n", hpos, vpos); */
	}
}

void
vgoto(int y) {
	vpos = y;
	if (pageon()) {
		endstring();
/*		Bprint(Bstdout, "%d %d m\n", hpos, vpos); */
	}
}

void
hmot(int x) {
	int delta;

	if ((x<expecthmot-1) || (x>expecthmot+1)) {
		delta = x - expecthmot;
		if (curtrofffontid <0 || curtrofffontid >= troffontcnt) {
			Bprint(Bstderr, "troffontcnt=%d curtrofffontid=%d\n", troffontcnt, curtrofffontid);
			Bflush(Bstderr);
			exits("");
		}
		if (delta == troffontab[curtrofffontid].spacewidth*fontsize/10 && isinstring()) {
			if (pageon()) runeout(' ');
		} else {
			if (pageon()) {
				endstring();
				/* Bprint(Bstdout, " %d 0 rmoveto ", delta); */
/*				Bprint(Bstdout, " %d %d m ", hpos+x, vpos); */
				if (debug) Bprint(Bstderr, "x=%d expecthmot=%d\n", x, expecthmot);
			}
		}
	}
	hpos += x;
	expecthmot = 0;
}

void
vmot(int y) {
	endstring();
/*	Bprint(Bstdout, " 0 %d rmoveto ", -y); */
	vpos += y;
}

struct charent **
findglyph(int trfid, Rune rune, char *stoken) {
	struct charent **cp;

	for (cp = &(troffontab[trfid].charent[RUNEGETGROUP(rune)][RUNEGETCHAR(rune)]); *cp != 0; cp = &((*cp)->next)) {
		if ((*cp)->name) {
			if (debug) Bprint(Bstderr, "looking for <%s>, have <%s> in font %s\n", stoken, (*cp)->name, troffontab[trfid].trfontid);
			if (strcmp((*cp)->name, stoken) == 0)
				break;
		}
	}
	return(cp);
}

/* output glyph.  Use first rune to look up character (hash)
 * then use stoken UTF string to find correct glyph in linked
 * list of glyphs in bucket.
 */
void
glyphout(Rune rune, char *stoken, BOOLEAN specialflag) {
	struct charent **cp;
	struct troffont *tfp;
	struct psfent *psfp = (struct psfent*)0;
	int i, t;
	int fontid;	/* this is the troff font table index, not the mounted font table index */
	int mi, wid;
	Rune r;

	mi = 0;
	settrfont();

	/* check current font for the character, special or not */
	fontid = curtrofffontid;
if (debug) fprint(2, "	looking through current font: trying %s\n", troffontab[fontid].trfontid);
	cp = findglyph(fontid, rune, stoken);
	if (*cp != 0) goto foundit;

	if (specialflag) {
		if (expecthmot) hmot(0);

		/* check special fonts for the special character */
		/* cycle through the (troff) mounted fonts starting at the next font */
		for (mi=0; mi<fontmnt; mi++) {
			if (troffontab[fontid].trfontid==0) error(WARNING, "glyphout:troffontab[%d].trfontid=0x%x, botch!\n",
				fontid, troffontab[fontid].trfontid);
			if (fontmtab[mi]==0) {
				if (debug) fprint(2, "fontmtab[%d]=0x%x, fontmnt=%d\n", mi, fontmtab[mi], fontmnt);
				continue;
			}
			if (strcmp(troffontab[fontid].trfontid, fontmtab[mi])==0) break;
		}
		if (mi==fontmnt) error(FATAL, "current troff font is not mounted, botch!\n");
		for (i=(mi+1)%fontmnt; i!=mi; i=(i+1)%fontmnt) {
			if (fontmtab[i]==0) {
				if (debug) fprint(2, "fontmtab[%d]=0x%x, fontmnt=%d\n", i, fontmtab[i], fontmnt);
				continue;
			}
			fontid = findtfn(fontmtab[i], TRUE);
if (debug) fprint(2, "	looking through special fonts: trying %s\n", troffontab[fontid].trfontid);
			if (troffontab[fontid].special) {
				cp = findglyph(fontid, rune, stoken);
				if (*cp != 0) goto foundit;
			}
		}

		/* check font 1 (if current font is not font 1) for the special character */
		if (mi != 1) {
				fontid = findtfn(fontmtab[1], TRUE);;
if (debug) fprint(2, "	looking through font at position 1: trying %s\n", troffontab[fontid].trfontid);
				cp = findglyph(fontid, rune, stoken);
				if (*cp != 0) goto foundit;
		}
	}

	if (*cp == 0) {
		error(WARNING, "cannot find glyph, rune=0x%x stoken=<%s> troff font %s\n", rune, stoken,
			troffontab[curtrofffontid].trfontid);
		expecthmot = 0;
	}

	/* use the peter face in lieu of the character that we couldn't find */
	rune = 'p';	stoken = "pw";
	for (i=(mi+1)%fontmnt; i!=mi; i=(i+1)%fontmnt) {
		if (fontmtab[i]==0) {
			if (debug) fprint(2, "fontmtab[%d]=0x%x\n", i, fontmtab[i]);
			continue;
		}
		fontid = findtfn(fontmtab[i], TRUE);
if (debug) fprint(2, "	looking through special fonts: trying %s\n", troffontab[fontid].trfontid);
		if (troffontab[fontid].special) {
			cp = findglyph(fontid, rune, stoken);
			if (*cp != 0) goto foundit;
		}
	}

	if (*cp == 0) {
		error(WARNING, "cannot find glyph, rune=0x%x stoken=<%s> troff font %s\n", rune, stoken,
			troffontab[curtrofffontid].trfontid);
		expecthmot = 0;
		return;
	}

foundit:
	t = (((*cp)->postfontid&0xff)<<8) | ((*cp)->postcharid&0xff);
	if (debug) {
		Bprint(Bstderr, "runeout(0x%x)<%C> postfontid=0x%x postcharid=0x%x troffcharwidth=%d\n",
			rune, rune, (*cp)->postfontid, (*cp)->postcharid, (*cp)->troffcharwidth);
	}

	tfp = &(troffontab[fontid]);
	for (i=0; i<tfp->psfmapsize; i++) {
		psfp = &(tfp->psfmap[i]);
		if(t>=psfp->start && t<=psfp->end) break;
	}
	if (i >= tfp->psfmapsize)
		error(FATAL, "character <0x%x> does not have a Postscript font defined.\n", rune);

	setpsfont(psfp->psftid, fontsize);

	if (t == 0x0001) {	/* character is in charlib */
		endstring();
		if (pageon()) {
			Bprint(Bstdout, "%d %d m ", hpos, vpos);
			/* if char is unicode character rather than name, clean up for postscript */
			wid = chartorune(&r, (*cp)->name);
			if(' '<r && r<0x7F)
				Bprint(Bstdout, "%d build_%s\n", (*cp)->troffcharwidth, (*cp)->name);
			else{
				if((*cp)->name[wid] != 0)
					error(FATAL, "character <%s> badly named\n", (*cp)->name);
				Bprint(Bstdout, "%d build_X%.4x\n", (*cp)->troffcharwidth, r);
			}

			/* stash charent pointer in a list so that we can print these character definitions
			 * in the prologue.
			 */
			for (i=0; i<build_char_cnt; i++)
				if (*cp == build_char_list[i]) break;
			if (i == build_char_cnt) {
				build_char_list = galloc(build_char_list, sizeof(struct charent *) * ++build_char_cnt,
				"build_char_list");
				build_char_list[build_char_cnt-1] = *cp;
			}
		}
		expecthmot = (*cp)->troffcharwidth * fontsize / unitwidth;
	} else if (isinstring() || rune != ' ') {
		startstring();
		if (pageon()) {
			if (rune == ' ')
				Bprint(Bstdout, " ");
			else
				Bprint(Bstdout, "%s", charcode[RUNEGETCHAR(t)].str);
		}
		expecthmot = (*cp)->troffcharwidth * fontsize / unitwidth;
	}
}

/* runeout puts a symbol into a string (queue) to be output.
 * It also has to keep track of the current and last symbol
 * output to check that the spacing is correct by default
 * or needs to be adjusted with a spacing operation.
 */

void
runeout(Rune rune) {
	char stoken[UTFmax+1];
	int i;

	i = runetochar(stoken, &rune);
	stoken[i] = '\0';
	glyphout(rune, stoken, TRUE);
}

void
specialout(char *stoken) {
	Rune rune;

	chartorune(&rune, stoken);
	glyphout(rune, stoken, TRUE);
}

void
graphfunc(Biobuf *bp) {
}

long
nametorune(char *name) {
	return(0);
}

void
notavail(char *msg) {
	Bprint(Bstderr, "%s is not available at this time.\n", msg);
}
