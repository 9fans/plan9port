/*    Unicode   |     PostScript
 *  start  end  | offset  font name
 * 0x0000 0x00ff  0x00   LuxiSans00
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include "common.h"
#include "tr2post.h"
#include "comments.h"
#include "path.h"

/* Postscript font names, e.g., `LuxiSans00'
 * names may only be added because reference to the
 * names is made by indexing into this table.
 */
static struct pfnament *pfnafontmtab = 0;
static int pfnamcnt = 0;
int curpostfontid = -1;
int curfontsize = -1;
int curtrofffontid = -1;
static int curfontpos = -1;
static int fontheight = 0;
static int fontslant = 0;

/* This is troffs mounted font table.  It is an anachronism resulting
 * from the design of the APS typesetter.  fontmnt is the
 * number of positions available.  fontmnt is really 11, but
 * should not be limited.
 */
int fontmnt = 0;
char **fontmtab;

struct troffont *troffontab = 0;

int troffontcnt = 0;

void
mountfont(int pos, char *fontname) {
	int i;

	if (debug) Bprint(Bstderr, "mountfont(%d, %s)\n", pos, fontname);
	if (pos < 0 || pos >= fontmnt)
		error(FATAL, "cannot mount a font at position %d,\n  can only mount into postions 0-%d\n",
			pos, fontmnt-1);

	i = strlen(fontname);
	fontmtab[pos] = galloc(fontmtab[pos], i+1, "mountfont():fontmtab");
	strcpy(fontmtab[pos], fontname);
	if (curfontpos == pos)	curfontpos = -1;
}

void
settrfont(void) {
	if (curfontpos == fontpos) return;

	if (fontmtab[fontpos] == 0)
		error(FATAL, "Font at position %d was not initialized, botch!\n", fontpos);

	curtrofffontid = findtfn(fontmtab[fontpos], 1);
	if (debug) Bprint(Bstderr, "settrfont()-> curtrofffontid=%d\n", curtrofffontid);
	curfontpos = fontpos;
	if (curtrofffontid < 0) {
		int i;

		error(WARNING, "fontpos=%d\n", fontpos);
		for (i=0; i<fontmnt; i++)
			if (fontmtab[i] == 0)
				error(WARNING, "fontmtab[%d]=0x0\n", i);
			else
				error(WARNING, "fontmtab[%d]=%s\n", i, fontmtab[i]);
		exits("settrfont()");
	}
}

void
setpsfont(int psftid, int fontsize) {
	if (psftid == curpostfontid && fontsize == curfontsize) return;
	if (psftid >= pfnamcnt)
		error(FATAL, "Postscript font index=%d used but not defined, there are only %d fonts\n",
			psftid, pfnamcnt);

	endstring();
	if (pageon()) {
		Bprint(Bstdout, "%d /%s f\n", fontsize, pfnafontmtab[psftid].str);
		if ( fontheight != 0 || fontslant != 0 )
			Bprint(Bstdout, "%d %d changefont\n", fontslant, (fontheight != 0) ? fontheight : fontsize);
		pfnafontmtab[psftid].used = 1;
		curpostfontid = psftid;
		curfontsize = fontsize;
	}
}

/* find index of PostScript font name in table
 * returns -1 if name is not in table
 * If insflg is not zero
 * and the name is not found in the table, insert it.
 */
int
findpfn(char *fontname, int insflg) {
	char *tp;
	int i;

	for (i=0; i<pfnamcnt; i++) {
		if (strcmp(pfnafontmtab[i].str, fontname) == 0)
			return(i);
	}
	if (insflg) {
		tp = galloc(pfnafontmtab, sizeof(struct pfnament)*(pfnamcnt+1), "findpfn():pfnafontmtab");
		if (tp == 0)
			return(-2);
		pfnafontmtab = (struct pfnament *)tp;
		i = strlen(fontname);
		pfnafontmtab[pfnamcnt].str = galloc(0, i+1, "findpfn():pfnafontmtab[].str");
		strncpy(pfnafontmtab[pfnamcnt].str, fontname, i);
		pfnafontmtab[pfnamcnt].str[i] = '\0';
		pfnafontmtab[pfnamcnt].used = 0;
		return(pfnamcnt++);
	}
	return(-1);
}

char postroffdirname[] = "#9/postscript/troff";		/* "/sys/lib/postscript/troff/"; */
char troffmetricdirname[] = "#9/troff/font";	/* "/sys/lib/troff/font/devutf/"; */

int
readpsfontdesc(char *fontname, int trindex) {
	static char *filename = 0;
	Biobuf *bfd;
	Biobuf *Bfd;
	int errorflg = 0, line =1, rv;
	int start, end, offset;
	int startfont, endfont, startchar, endchar, pfid;
	char psfontnam[128];
	struct troffont *tp;
/*	struct charent *cp[]; */

	if (debug) Bprint(Bstderr, "readpsfontdesc(%s,%d)\n", fontname, trindex);
	filename=galloc(filename, strlen(postroffdirname)+1+strlen(fontname)+1, "readpsfontdesc: cannot allocate memory\n");
	sprint(filename, "%s/%s", postroffdirname, fontname);

	bfd = Bopen(unsharp(filename), OREAD);
	if (bfd == 0) {
		error(WARNING, "cannot open file %s\n", filename);
		return(0);
	}
	Bfd = bfd; /* &(bfd->Biobufhdr); */

	do {
		offset = 0;
		if ((rv=Bgetfield(Bfd, 'd', &start, 0)) == 0) {
			errorflg = 1;
			error(WARNING, "file %s:%d illegal start value\n", filename, line);
		} else if (rv < 0) break;
		if ((rv=Bgetfield(Bfd, 'd', &end, 0)) == 0) {
			errorflg = 1;
			error(WARNING, "file %s:%d illegal end value\n", filename, line);
		} else if (rv < 0) break;
		if ((rv=Bgetfield(Bfd, 'd', &offset, 0)) < 0) {
			errorflg = 1;
			error(WARNING, "file %s:%d illegal offset value\n", filename, line);
		}
		if ((rv=Bgetfield(Bfd, 's', psfontnam, 128)) == 0) {
			errorflg = 1;
			error(WARNING, "file %s:%d illegal fontname value\n", filename, line);
		} else if (rv < 0) break;
		Brdline(Bfd, '\n');
		if (!errorflg) {
			struct psfent *psfentp;
			startfont = RUNEGETGROUP(start);
			startchar = RUNEGETCHAR(start);
			endfont = RUNEGETGROUP(end);
			endchar = RUNEGETCHAR(end);
			USED(startchar);
			USED(endchar);
			pfid = findpfn(psfontnam, 1);
			if (startfont != endfont) {
				error(WARNING, "font descriptions must not cross 256 glyph block boundary\n");
				errorflg = 1;
				break;
			}
			tp = &(troffontab[trindex]);
			tp->psfmap = galloc(tp->psfmap, ++(tp->psfmapsize)*sizeof(struct psfent), "readpsfontdesc():psfmap");
			psfentp = &(tp->psfmap[tp->psfmapsize-1]);
			psfentp->start = start;
			psfentp->end = end;
			psfentp->offset = offset;
			psfentp->psftid = pfid;
			if (debug) {
				Bprint(Bstderr, "\tpsfmap->start=0x%x\n", start);
				Bprint(Bstderr, "\tpsfmap->end=0x%x\n", end);
				Bprint(Bstderr, "\tpsfmap->offset=0x%x\n", offset);
				Bprint(Bstderr, "\tpsfmap->pfid=0x%x\n", pfid);
			}
/*
			for (i=startchar; i<=endchar; i++) {
				tp->charent[startfont][i].postfontid = pfid;
				tp->charent[startfont][i].postcharid = i + offset - startchar;
			}
 */
			if (debug) {
				Bprint(Bstderr, "%x %x ", start, end);
				if (offset) Bprint(Bstderr, "%x ", offset);
				Bprint(Bstderr, "%s\n", psfontnam);
			}
			line++;
		}
	} while(errorflg != 1);
	Bterm(Bfd);
	return(1);
}

int
readtroffmetric(char *fontname, int trindex) {
	static char *filename = 0;
	Biobuf *bfd;
	Biobuf *Bfd;
	int errorflg = 0, line =1, rv;
	struct charent **cp;
	char stoken[128], *str;
	int ntoken;
	Rune troffchar, quote;
	int width, flag, charnum, thisfont, thischar;
	BOOLEAN specharflag;

	if (debug) Bprint(Bstderr, "readtroffmetric(%s,%d)\n", fontname, trindex);
	filename=galloc(filename, strlen(troffmetricdirname)+4+strlen(devname)+1+strlen(fontname)+1, "readtroffmetric():filename");
	sprint(filename, "%s/dev%s/%s", troffmetricdirname, devname, fontname);

	bfd = Bopen(unsharp(filename), OREAD);
	if (bfd == 0) {
		error(WARNING, "cannot open file %s\n", filename);
		return(0);
	}
	Bfd = bfd; /* &(bfd->Biobufhdr); */
	do {
		/* deal with the few lines at the beginning of the
		 * troff font metric files.
		 */
		if ((rv=Bgetfield(Bfd, 's', stoken, 128)) == 0) {
			errorflg = 1;
			error(WARNING, "file %s:%d illegal token\n", filename, line);
		} else if (rv < 0) break;
		if (debug) {
			Bprint(Bstderr, "%s\n", stoken);
		}

		if (strcmp(stoken, "name") == 0) {
			if ((rv=Bgetfield(Bfd, 's', stoken, 128)) == 0) {
				errorflg = 1;
				error(WARNING, "file %s:%d illegal token\n", filename, line);
			} else if (rv < 0) break;
		} else if (strcmp(stoken, "named") == 0) {
			Brdline(Bfd, '\n');
		} else if (strcmp(stoken, "fontname") == 0) {
			if ((rv=Bgetfield(Bfd, 's', stoken, 128)) == 0) {
				errorflg = 1;
				error(WARNING, "file %s:%d illegal token\n", filename, line);
			} else if (rv < 0) break;
		} else if (strcmp(stoken, "spacewidth") == 0) {
			if ((rv=Bgetfield(Bfd, 'd', &ntoken, 0)) == 0) {
				errorflg = 1;
				error(WARNING, "file %s:%d illegal token\n", filename, line);
			} else if (rv < 0) break;
			troffontab[trindex].spacewidth = ntoken;
			thisfont = RUNEGETGROUP(' ');
			thischar = RUNEGETCHAR(' ');
			for (cp = &(troffontab[trindex].charent[thisfont][thischar]); *cp != 0; cp = &((*cp)->next))
				if ((*cp)->name)
					if  (strcmp((*cp)->name, " ") == 0)
						break;

			if (*cp == 0) *cp = galloc(0, sizeof(struct charent), "readtroffmetric:charent");
			(*cp)->postfontid = thisfont;
			(*cp)->postcharid = thischar;
			(*cp)->troffcharwidth = ntoken;
			(*cp)->name = galloc(0, 2, "readtroffmetric: char name");
			(*cp)->next = 0;
			strcpy((*cp)->name, " ");
		} else if (strcmp(stoken, "special") == 0) {
			troffontab[trindex].special = TRUE;
		} else if (strcmp(stoken, "charset") == 0) {
			line++;
			break;
		}
		if (!errorflg) {
			line++;
		}
	} while(!errorflg && rv>=0);
	while(!errorflg && rv>=0) {
		if ((rv=Bgetfield(Bfd, 's', stoken, 128)) == 0) {
			errorflg = 1;
			error(WARNING, "file %s:%d illegal rune token <0x%x> rv=%d\n", filename, line, troffchar, rv);
		} else if (rv < 0) break;
		if (utflen(stoken) > 1) specharflag = TRUE;
		else specharflag = FALSE;
		/* if this character is a quote we have to use the previous characters info */
		if ((rv=Bgetfield(Bfd, 'r', &quote, 0)) == 0) {
			errorflg = 1;
			error(WARNING, "file %s:%d illegal width or quote token <0x%x> rv=%d\n", filename, line, quote, rv);
		} else if (rv < 0) break;
		if (quote == '"') {
			/* need some code here */

			goto flush;
		} else {
			Bungetrune(Bfd);
		}

		if ((rv=Bgetfield(Bfd, 'd', &width, 0)) == 0) {
			errorflg = 1;
			error(WARNING, "file %s:%d illegal width token <0x%x> rv=%d\n", filename, line, troffchar, rv);
		} else if (rv < 0) break;
		if ((rv=Bgetfield(Bfd, 'd', &flag, 0)) == 0) {
			errorflg = 1;
			error(WARNING, "file %s:%d illegal flag token <0x%x> rv=%d\n", filename, line, troffchar, rv);
		} else if (rv < 0) break;
		if ((rv=Bgetfield(Bfd, 'd', &charnum, 0)) == 0) {
			errorflg = 1;
			error(WARNING, "file %s:%d illegal character number token <0x%x> rv=%d\n", filename, line, troffchar, rv);
		} else if (rv < 0) break;
flush:
		str = Brdline(Bfd, '\n');
		/* stash the crap from the end of the line for debugging */
		if (debug) {
			if (str == 0) {
				Bprint(Bstderr, "premature EOF\n");
				return(0);
			}
			str[Blinelen(Bfd)-1] = '\0';
		}
		line++;
		chartorune(&troffchar, stoken);
		if (specharflag) {
			if (debug)
				Bprint(Bstderr, "%s %d  %d 0x%x %s # special\n",stoken, width, flag, charnum, str);
		}
		if (strcmp(stoken, "---") == 0) {
			thisfont = RUNEGETGROUP(charnum);
			thischar = RUNEGETCHAR(charnum);
			stoken[0] = '\0';
		} else {
			thisfont = RUNEGETGROUP(troffchar);
			thischar = RUNEGETCHAR(troffchar);
		}
		for (cp = &(troffontab[trindex].charent[thisfont][thischar]); *cp != 0; cp = &((*cp)->next))
			if ((*cp)->name) {
				if (debug) Bprint(Bstderr, "installing <%s>, found <%s>\n", stoken, (*cp)->name);
				if  (strcmp((*cp)->name, stoken) == 0)
					break;
			}
		if (*cp == 0) *cp = galloc(0, sizeof(struct charent), "readtroffmetric:charent");
		(*cp)->postfontid = RUNEGETGROUP(charnum);
		(*cp)->postcharid = RUNEGETCHAR(charnum);
		(*cp)->troffcharwidth = width;
		(*cp)->name = galloc(0, strlen(stoken)+1, "readtroffmetric: char name");
		(*cp)->next = 0;
		strcpy((*cp)->name, stoken);
		if (debug) {
			if (specharflag)
				Bprint(Bstderr, "%s", stoken);
			else
				Bputrune(Bstderr, troffchar);
			Bprint(Bstderr, " %d  %d 0x%x %s # psfontid=0x%x pscharid=0x%x thisfont=0x%x thischar=0x%x\n",
				width, flag, charnum, str,
				(*cp)->postfontid,
				(*cp)->postcharid,
				thisfont, thischar);
		}
	}
	Bterm(Bfd);
	Bflush(Bstderr);
	return(1);
}

/* find index of troff font name in table
 * returns -1 if name is not in table
 * returns -2 if it cannot allocate memory
 * returns -3 if there is a font mapping problem
 * If insflg is not zero
 * and the name is not found in the table, insert it.
 */
int
findtfn(char *fontname, BOOLEAN insflg) {
	struct troffont *tp;
	int i, j;

	if (debug) {
		if (fontname==0) fprint(2, "findtfn(0x%x,%d)\n", fontname, insflg);
		else fprint(2, "findtfn(%s,%d)\n", fontname, insflg);
	}
	for (i=0; i<troffontcnt; i++) {
		if (troffontab[i].trfontid==0) {
			error(WARNING, "findtfn:troffontab[%d].trfontid=0x%x, botch!\n",
				i, troffontab[i].trfontid);
			continue;
		}
		if (strcmp(troffontab[i].trfontid, fontname) == 0)
			return(i);
	}
	if (insflg) {
		tp = (struct troffont *)galloc(troffontab, sizeof(struct troffont)*(troffontcnt+1), "findtfn: struct troffont:");
		if (tp == 0)
			return(-2);
		troffontab = tp;
		tp = &(troffontab[troffontcnt]);
		i = strlen(fontname);
		tp->trfontid = galloc(0, i+1, "findtfn: trfontid:");

		/* initialize new troff font entry with name and numeric fields to 0 */
		strncpy(tp->trfontid, fontname, i);
		tp->trfontid[i] = '\0';
		tp->special = FALSE;
		tp->spacewidth = 0;
		tp->psfmapsize = 0;
		tp->psfmap = 0;
		for (i=0; i<NUMOFONTS; i++)
			for (j=0; j<FONTSIZE; j++)
				tp->charent[i][j] = 0;
		troffontcnt++;
		if (!readtroffmetric(fontname, troffontcnt-1))
			return(-3);
		if (!readpsfontdesc(fontname, troffontcnt-1))
			return(-3);
		return(troffontcnt-1);
	}
	return(-1);
}

void
finish(void) {
	int i;

	Bprint(Bstdout, "%s", TRAILER);
	Bprint(Bstdout, "done\n");
	Bprint(Bstdout, "%s", DOCUMENTFONTS);

	for (i=0; i<pfnamcnt; i++)
		if (pfnafontmtab[i].used)
			Bprint(Bstdout, " %s", pfnafontmtab[i].str);
	Bprint(Bstdout, "\n");

	Bprint(Bstdout, "%s %d\n", PAGES, pages_printed);

}

/* Set slant to n degrees. Disable slanting if n is 0. */
void
t_slant(int n) {
	fontslant = n;
	curpostfontid = -1;
}

/* Set character height to n points. Disabled if n is 0 or the current size. */

void
t_charht(int n) {
	fontheight = (n == fontsize) ? 0 : n;
	curpostfontid = -1;
}
