/*
n10.c

Device interfaces
*/

#include <u.h>
#include "tdef.h"
#include "ext.h"
#include "fns.h"
#include <ctype.h>

Term	t;	/* terminal characteristics */

int	dtab;
int	plotmode;
int	esct;

enum	{ Notype = 0, Type = 1 };

static char *parse(char *s, int typeit)	/* convert \0, etc to nroff driving table format */
{		/* typeit => add a type id to the front for later use */
	static char buf[100], *t, *obuf;
	int quote = 0;
	wchar_t wc;

	obuf = typeit == Type ? buf : buf+1;
#ifdef UNICODE
	if (mbtowc(&wc, s, strlen(s)) > 1) {	/* it's multibyte, */
		buf[0] = MBchar;
		strcpy(buf+1, s);
		return obuf;
	}			/* so just hand it back */
#endif	/*UNICODE*/
	buf[0] = Troffchar;
	t = buf + 1;
	if (*s == '"') {
		s++;
		quote = 1;
	}
	for (;;) {
		if (quote && *s == '"') {
			s++;
			break;
		}
		if (!quote && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\0'))
			break;
		if (*s != '\\')
			*t++ = *s++;
		else {
			s++;	/* skip \\ */
			if (isdigit((uchar)s[0]) && isdigit((uchar)s[1]) && isdigit((uchar)s[2])) {
				*t++ = (s[0]-'0')<<6 | (s[1]-'0')<<3 | s[2]-'0';
				s += 2;
			} else if (isdigit((uchar)s[0])) {
				*t++ = *s - '0';
			} else if (*s == 'b') {
				*t++ = '\b';
			} else if (*s == 'n') {
				*t++ = '\n';
			} else if (*s == 'r') {
				*t++ = '\r';
			} else if (*s == 't') {
				*t++ = '\t';
			} else {
				*t++ = *s;
			}
			s++;
		}
	}
	*t = '\0';
	return obuf;
}


static int getnrfont(FILE *fp)	/* read the nroff description file */
{
	Chwid chtemp[NCHARS];
	static Chwid chinit;
	int i, nw, n, wid, code, type;
	char buf[100], ch[100], s1[100], s2[100];
	wchar_t wc;

	code = 0;
	chinit.wid = 1;
	chinit.str = "";
	for (i = 0; i < ALPHABET; i++) {
		chtemp[i] = chinit;	/* zero out to begin with */
		chtemp[i].num = chtemp[i].code = i;	/* every alphabetic character is itself */
		chtemp[i].wid = 1;	/* default ascii widths */
	}
	skipline(fp);
	nw = ALPHABET;
	while (fgets(buf, sizeof buf, fp) != NULL) {
		sscanf(buf, "%s %s %[^\n]", ch, s1, s2);
		if (!eq(s1, "\"")) {	/* genuine new character */
			sscanf(s1, "%d", &wid);
		} /* else it's a synonym for prev character, */
			/* so leave previous values intact */

		/* decide what kind of alphabet it might come from */

		if (strlen(ch) == 1) {	/* it's ascii */
			n = ch[0];	/* origin includes non-graphics */
			chtemp[n].num = ch[0];
		} else if (ch[0] == '\\' && ch[1] == '0') {
			n = strtol(ch+1, 0, 0);	/* \0octal or \0xhex */
			chtemp[n].num = n;
#ifdef UNICODE
		} else if (mbtowc(&wc, ch, strlen(ch)) > 1) {
			chtemp[nw].num = chadd(ch, MBchar, Install);
			n = nw;
			nw++;
#endif	/*UNICODE*/
		} else {
			if (strcmp(ch, "---") == 0) { /* no name */
				sprintf(ch, "%d", code);
				type = Number;
			} else
				type = Troffchar;
/* BUG in here somewhere when same character occurs twice in table */
			chtemp[nw].num = chadd(ch, type, Install);
			n = nw;
			nw++;
		}
		chtemp[n].wid = wid;
		chtemp[n].str = strdupl(parse(s2, Type));
	}
	t.tfont.nchars = nw;
	t.tfont.wp = (Chwid *) malloc(nw * sizeof(Chwid));
	if (t.tfont.wp == NULL)
		return -1;
	for (i = 0; i < nw; i++)
		t.tfont.wp[i] = chtemp[i];
	return 1;
}


void n_ptinit(void)
{
	int i;
	char *p;
	char opt[50], cmd[100];
	FILE *fp;

	hmot = n_hmot;
	makem = n_makem;
	setabs = n_setabs;
	setch = n_setch;
	sethl = n_sethl;
	setht = n_setht;
	setslant = n_setslant;
	vmot = n_vmot;
	xlss = n_xlss;
	findft = n_findft;
	width = n_width;
	mchbits = n_mchbits;
	ptlead = n_ptlead;
	ptout = n_ptout;
	ptpause = n_ptpause;
	setfont = n_setfont;
	setps = n_setps;
	setwd = n_setwd;

	if ((p = getenv("NROFFTERM")) != 0)
		strcpy(devname, p);
	if (termtab[0] == 0)
		strcpy(termtab,DWBntermdir);
	if (fontdir[0] == 0)
		strcpy(fontdir, "");
	if (devname[0] == 0)
		strcpy(devname, NDEVNAME);
	pl = 11*INCH;
	po = PO;
	hyf = 0;
	ascii = 1;
	lg = 0;
	fontlab[1] = 'R';
	fontlab[2] = 'I';
	fontlab[3] = 'B';
	fontlab[4] = PAIR('B','I');
	fontlab[5] = 'D';
	bdtab[3] = 3;
	bdtab[4] = 3;

	/* hyphalg = 0;	/* for testing */

	strcat(termtab, devname);
	if ((fp = fopen(unsharp(termtab), "r")) == NULL) {
		ERROR "cannot open %s", termtab WARN;
		exit(-1);
	}


/* this loop isn't robust about input format errors. */
/* it assumes  name, name-value pairs..., charset */
/* god help us if we get out of sync. */

	fscanf(fp, "%s", cmd);	/* should be device name... */
	if (!is(devname) && trace)
		ERROR "wrong terminal name: saw %s, wanted %s", cmd, devname WARN;
	for (;;) {
		fscanf(fp, "%s", cmd);
		if (is("charset"))
			break;
		fscanf(fp, " %[^\n]", opt);
		if (is("bset")) t.bset = atoi(opt);
		else if (is("breset")) t.breset = atoi(opt);
		else if (is("Hor")) t.Hor = atoi(opt);
		else if (is("Vert")) t.Vert = atoi(opt);
		else if (is("Newline")) t.Newline = atoi(opt);
		else if (is("Char")) t.Char = atoi(opt);
		else if (is("Em")) t.Em = atoi(opt);
		else if (is("Halfline")) t.Halfline = atoi(opt);
		else if (is("Adj")) t.Adj = atoi(opt);
		else if (is("twinit")) t.twinit = strdupl(parse(opt, Notype));
		else if (is("twrest")) t.twrest = strdupl(parse(opt, Notype));
		else if (is("twnl")) t.twnl = strdupl(parse(opt, Notype));
		else if (is("hlr")) t.hlr = strdupl(parse(opt, Notype));
		else if (is("hlf")) t.hlf = strdupl(parse(opt, Notype));
		else if (is("flr")) t.flr = strdupl(parse(opt, Notype));
		else if (is("bdon")) t.bdon = strdupl(parse(opt, Notype));
		else if (is("bdoff")) t.bdoff = strdupl(parse(opt, Notype));
		else if (is("iton")) t.iton = strdupl(parse(opt, Notype));
		else if (is("itoff")) t.itoff = strdupl(parse(opt, Notype));
		else if (is("ploton")) t.ploton = strdupl(parse(opt, Notype));
		else if (is("plotoff")) t.plotoff = strdupl(parse(opt, Notype));
		else if (is("up")) t.up = strdupl(parse(opt, Notype));
		else if (is("down")) t.down = strdupl(parse(opt, Notype));
		else if (is("right")) t.right = strdupl(parse(opt, Notype));
		else if (is("left")) t.left = strdupl(parse(opt, Notype));
		else
			ERROR "bad tab.%s file, %s %s", devname, cmd, opt WARN;
	}

	getnrfont(fp);
	fclose(fp);

	sps = EM;
	ics = EM * 2;
	dtab = 8 * t.Em;
	for (i = 0; i < 16; i++)
		tabtab[i] = dtab * (i + 1);
	pl = 11 * INCH;
	po = PO;
	spacesz = SS;
	lss = lss1 = VS;
	ll = ll1 = lt = lt1 = LL;
	smnt = nfonts = 5;	/* R I B BI S */
	n_specnames();	/* install names like "hyphen", etc. */
	if (eqflg)
		t.Adj = t.Hor;
}


void n_specnames(void)
{

	int	i;

	for (i = 0; spnames[i].n; i++)
		*spnames[i].n = chadd(spnames[i].v, Troffchar, Install);
	if (c_isalnum == 0)
		c_isalnum = NROFFCHARS;
}

void twdone(void)
{
	if (!TROFF && t.twrest) {
		obufp = obuf;
		oputs(t.twrest);
		flusho();
		if (pipeflg) {
			pclose(ptid);
		}
		restore_tty();
	}
}


void n_ptout(Tchar i)
{
	*olinep++ = i;
	if (olinep >= &oline[LNSIZE])
		olinep--;
	if (cbits(i) != '\n')
		return;
	olinep--;
	lead += dip->blss + lss - t.Newline;
	dip->blss = 0;
	esct = esc = 0;
	if (olinep > oline) {
		move();
		ptout1();
		oputs(t.twnl);
	} else {
		lead += t.Newline;
		move();
	}
	lead += dip->alss;
	dip->alss = 0;
	olinep = oline;
}


void ptout1(void)
{
	int k;
	char *codep;
	int w, j, phyw;
	Tchar *q, i;
	static int oxfont = FT;	/* start off in roman */

	for (q = oline; q < olinep; q++) {
		i = *q;
		if (ismot(i)) {
			j = absmot(i);
			if (isnmot(i))
				j = -j;
			if (isvmot(i))
				lead += j;
			else
				esc += j;
			continue;
		}
		if ((k = cbits(i)) <= ' ') {
			switch (k) {
			case ' ': /*space*/
				esc += t.Char;
				break;
			case '\033':
			case '\007':
			case '\016':
			case '\017':
				oput(k);
				break;
			}
			continue;
		}
		phyw = w = t.Char * t.tfont.wp[k].wid;
		if (iszbit(i))
			w = 0;
		if (esc || lead)
			move();
		esct += w;
		xfont = fbits(i);
		if (xfont != oxfont) {
			switch (oxfont) {
			case ULFONT:	oputs(t.itoff); break;
			case BDFONT:	oputs(t.bdoff); break;
			case BIFONT:	oputs(t.itoff); oputs(t.bdoff); break;
			}
			switch (xfont) {
			case ULFONT:
				if (*t.iton & 0377) oputs(t.iton); break;
			case BDFONT:
				if (*t.bdon & 0377) oputs(t.bdon); break;
			case BIFONT:
				if (*t.bdon & 0377) oputs(t.bdon);
				if (*t.iton & 0377) oputs(t.iton);
				break;
			}
			oxfont = xfont;
		}
		if ((xfont == ulfont || xfont == BIFONT) && !(*t.iton & 0377)) {
			for (j = w / t.Char; j > 0; j--)
				oput('_');
			for (j = w / t.Char; j > 0; j--)
				oput('\b');
		}
		if (!(*t.bdon & 0377) && ((j = bdtab[xfont]) || xfont == BDFONT || xfont == BIFONT))
			j++;
		else
			j = 1;	/* number of overstrikes for bold */
		if (k < ALPHABET) {	/* ordinary ascii */
			oput(k);
			while (--j > 0) {
				oput('\b');
				oput(k);
			}
		} else if (k >= t.tfont.nchars) {	/* BUG -- not really understood */
/* fprintf(stderr, "big char %d, name %s\n", k, chname(k)); /* */
			oputs(chname(k)+1);	/* BUG: should separate Troffchar and MBchar... */
		} else if (t.tfont.wp[k].str == 0) {
/* fprintf(stderr, "nostr char %d, name %s\n", k, chname(k)); /* */
			oputs(chname(k)+1);	/* BUG: should separate Troffchar and MBchar... */
		} else if (t.tfont.wp[k].str[0] == MBchar) {	/* parse() puts this on */
/* fprintf(stderr, "MBstr char %d, name %s\n", k, chname(k)); /* */
			oputs(t.tfont.wp[k].str+1);
		} else {
			int oj = j;
/* fprintf(stderr, "str char %d, name %s\n", k, chname(k)); /* */
			codep = t.tfont.wp[k].str+1;	/* Troffchar by default */
			while (*codep != 0) {
				if (*codep & 0200) {
					codep = plot(codep);
					oput(' ');
				} else {
					if (*codep == '%')	/* escape */
						codep++;
					oput(*codep);
					if (*codep == '\033')
						oput(*++codep);
					else if (*codep != '\b')
						for (j = oj; --j > 0; ) {
							oput('\b');
							oput(*codep);
						}
					codep++;
				}
			}
		}
		if (!w)
			for (j = phyw / t.Char; j > 0; j--)
				oput('\b');
	}
}


char *plot(char *x)
{
	int	i;
	char	*j, *k;

	oputs(t.ploton);
	k = x;
	if ((*k & 0377) == 0200)
		k++;
	for (; *k; k++) {
		if (*k == '%') {	/* quote char within plot mode */
			oput(*++k);
		} else if (*k & 0200) {
			if (*k & 0100) {
				if (*k & 040)
					j = t.up;
				else
					j = t.down;
			} else {
				if (*k & 040)
					j = t.left;
				else
					j = t.right;
			}
			if ((i = *k & 037) == 0) {	/* 2nd 0200 turns it off */
				++k;
				break;
			}
			while (i--)
				oputs(j);
		} else
			oput(*k);
	}
	oputs(t.plotoff);
	return(k);
}


void move(void)
{
	int k;
	char *i, *j;
	char *p, *q;
	int iesct, dt;

	iesct = esct;
	if (esct += esc)
		i = "\0";
	else
		i = "\n\0";
	j = t.hlf;
	p = t.right;
	q = t.down;
	if (lead) {
		if (lead < 0) {
			lead = -lead;
			i = t.flr;
			/*	if(!esct)i = t.flr; else i = "\0";*/
			j = t.hlr;
			q = t.up;
		}
		if (*i & 0377) {
			k = lead / t.Newline;
			lead = lead % t.Newline;
			while (k--)
				oputs(i);
		}
		if (*j & 0377) {
			k = lead / t.Halfline;
			lead = lead % t.Halfline;
			while (k--)
				oputs(j);
		} else { /* no half-line forward, not at line begining */
			k = lead / t.Newline;
			lead = lead % t.Newline;
			if (k > 0)
				esc = esct;
			i = "\n";
			while (k--)
				oputs(i);
		}
	}
	if (esc) {
		if (esc < 0) {
			esc = -esc;
			j = "\b";
			p = t.left;
		} else {
			j = " ";
			if (hflg)
				while ((dt = dtab - (iesct % dtab)) <= esc) {
					if (dt % t.Em)
						break;
					oput(TAB);
					esc -= dt;
					iesct += dt;
				}
		}
		k = esc / t.Em;
		esc = esc % t.Em;
		while (k--)
			oputs(j);
	}
	if ((*t.ploton & 0377) && (esc || lead)) {
		oputs(t.ploton);
		esc /= t.Hor;
		lead /= t.Vert;
		while (esc--)
			oputs(p);
		while (lead--)
			oputs(q);
		oputs(t.plotoff);
	}
	esc = lead = 0;
}


void n_ptlead(void)
{
	move();
}


void n_ptpause(void )
{
	char	junk;

	flusho();
	read(2, &junk, 1);
}
