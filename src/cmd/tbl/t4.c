/* t4.c: read table specification */
# include "t.h"
int	oncol;

void
getspec(void)
{
	int	icol, i;

	qcol = findcol() + 1;/* must allow one extra for line at right */
	garray(qcol);
	sep[-1] = -1;
	for (icol = 0; icol < qcol; icol++) {
		sep[icol] = -1;
		evenup[icol] = 0;
		cll[icol][0] = 0;
		for (i = 0; i < MAXHEAD; i++) {
			csize[icol][i][0] = 0;
			vsize[icol][i][0] = 0;
			font[icol][i][0] = lefline[icol][i] = 0;
			flags[icol][i] = 0;
			style[icol][i] = 'l';
		}
	}
	for (i = 0; i < MAXHEAD; i++)
		lefline[qcol][i] = 0;	/* fixes sample55 looping */
	nclin = ncol = 0;
	oncol = 0;
	left1flg = rightl = 0;
	readspec();
	Bprint(&tabout, ".rm");
	for (i = 0; i < ncol; i++)
		Bprint(&tabout, " %2s", reg(i, CRIGHT));
	Bprint(&tabout, "\n");
}


void
readspec(void)
{
	int	icol, c, sawchar, stopc, i;
	char	sn[10], *snp, *temp;

	sawchar = icol = 0;
	while (c = get1char()) {
		switch (c) {
		default:
			if (c != tab) {
				char buf[64];
				sprint(buf, "bad table specification character %c", c);
				error(buf);
			}
		case ' ': /* note this is also case tab */
			continue;
		case '\n':
			if (sawchar == 0)
				continue;
		case ',':
		case '.': /* end of table specification */
			ncol = max(ncol, icol);
			if (lefline[ncol][nclin] > 0) {
				ncol++;
				rightl++;
			};
			if (sawchar)
				nclin++;
			if (nclin >= MAXHEAD)
				error("too many lines in specification");
			icol = 0;
			if (ncol == 0 || nclin == 0)
				error("no specification");
			if (c == '.') {
				while ((c = get1char()) && c != '\n')
					if (c != ' ' && c != '\t')
						error("dot not last character on format line");
				/* fix up sep - default is 3 except at edge */
				for (icol = 0; icol < ncol; icol++)
					if (sep[icol] < 0)
						sep[icol] =  icol + 1 < ncol ? 3 : 2;
				if (oncol == 0)
					oncol = ncol;
				else if (oncol + 2 < ncol)
					error("tried to widen table in T&, not allowed");
				return;
			}
			sawchar = 0;
			continue;
		case 'C':
		case 'S':
		case 'R':
		case 'N':
		case 'L':
		case 'A':
			c += ('a' - 'A');
		case '_':
			if (c == '_')
				c = '-';
		case '=':
		case '-':
		case '^':
		case 'c':
		case 's':
		case 'n':
		case 'r':
		case 'l':
		case 'a':
			style[icol][nclin] = c;
			if (c == 's' && icol <= 0)
				error("first column can not be S-type");
			if (c == 's' && style[icol-1][nclin] == 'a') {
				Bprint(&tabout, ".tm warning: can't span a-type cols, changed to l\n");
				style[icol-1][nclin] = 'l';
			}
			if (c == 's' && style[icol-1][nclin] == 'n') {
				Bprint(&tabout, ".tm warning: can't span n-type cols, changed to c\n");
				style[icol-1][nclin] = 'c';
			}
			icol++;
			if (c == '^' && nclin <= 0)
				error("first row can not contain vertical span");
			if (icol > qcol)
				error("too many columns in table");
			sawchar = 1;
			continue;
		case 'b':
		case 'i':
			c += 'A' - 'a';
		case 'B':
		case 'I':
			if (icol == 0)
				continue;
			snp = font[icol-1][nclin];
			snp[0] = (c == 'I' ? '2' : '3');
			snp[1] = 0;
			continue;
		case 't':
		case 'T':
			if (icol > 0)
				flags[icol-1][nclin] |= CTOP;
			continue;
		case 'd':
		case 'D':
			if (icol > 0)
				flags[icol-1][nclin] |= CDOWN;
			continue;
		case 'f':
		case 'F':
			if (icol == 0)
				continue;
			snp = font[icol-1][nclin];
			snp[0] = snp[1] = stopc = 0;
			for (i = 0; i < 2; i++) {
				c = get1char();
				if (i == 0 && c == '(') {
					stopc = ')';
					c = get1char();
				}
				if (c == 0)
					break;
				if (c == stopc) {
					stopc = 0;
					break;
				}
				if (stopc == 0)
					if (c == ' ' || c == tab )
						break;
				if (c == '\n' || c == '|') {
					un1getc(c);
					break;
				}
				snp[i] = c;
				if (c >= '0' && c <= '9')
					break;
			}
			if (stopc)
				if (get1char() != stopc)
					error("Nonterminated font name");
			continue;
		case 'P':
		case 'p':
			if (icol <= 0)
				continue;
			temp = snp = csize[icol-1][nclin];
			while (c = get1char()) {
				if (c == ' ' || c == tab || c == '\n')
					break;
				if (c == '-' || c == '+')
					if (snp > temp)
						break;
					else
						*snp++ = c;
				else if (digit(c))
					*snp++ = c;
				else
					break;
				if (snp - temp > 4)
					error("point size too large");
			}
			*snp = 0;
			if (atoi(temp) > 36)
				error("point size unreasonable");
			un1getc (c);
			continue;
		case 'V':
		case 'v':
			if (icol <= 0)
				continue;
			temp = snp = vsize[icol-1][nclin];
			while (c = get1char()) {
				if (c == ' ' || c == tab || c == '\n')
					break;
				if (c == '-' || c == '+')
					if (snp > temp)
						break;
					else
						*snp++ = c;
				else if (digit(c))
					*snp++ = c;
				else
					break;
				if (snp - temp > 4)
					error("vertical spacing value too large");
			}
			*snp = 0;
			un1getc(c);
			continue;
		case 'w':
		case 'W':
			snp = cll [icol-1];
			/* Dale Smith didn't like this check - possible to have two text blocks
		   of different widths now ....
			if (*snp)
				{
				Bprint(&tabout, "Ignored second width specification");
				continue;
				}
		/* end commented out code ... */
			stopc = 0;
			while (c = get1char()) {
				if (snp == cll[icol-1] && c == '(') {
					stopc = ')';
					continue;
				}
				if ( !stopc && (c > '9' || c < '0'))
					break;
				if (stopc && c == stopc)
					break;
				*snp++ = c;
			}
			*snp = 0;
			if (snp - cll[icol-1] > CLLEN)
				error ("column width too long");
			if (!stopc)
				un1getc(c);
			continue;
		case 'e':
		case 'E':
			if (icol < 1)
				continue;
			evenup[icol-1] = 1;
			evenflg = 1;
			continue;
		case 'z':
		case 'Z': /* zero width-ignre width this item */
			if (icol < 1)
				continue;
			flags[icol-1][nclin] |= ZEROW;
			continue;
		case 'u':
		case 'U': /* half line up */
			if (icol < 1)
				continue;
			flags[icol-1][nclin] |= HALFUP;
			continue;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			sn[0] = c;
			snp = sn + 1;
			while (digit(*snp++ = c = get1char()))
				;
			un1getc(c);
			sep[icol-1] = max(sep[icol-1], numb(sn));
			continue;
		case '|':
			lefline[icol][nclin]++;
			if (icol == 0)
				left1flg = 1;
			continue;
		}
	}
	error("EOF reading table specification");
}


int
findcol(void)
{
# define FLNLIM 200
	/* this counts the number of columns and then puts the line back*/
	char	*s, line[FLNLIM+2], *p;
	int	c, n = 0, inpar = 0;

	while ((c = get1char()) != 0 && c == ' ')
		;
	if (c != '\n')
		un1getc(c);
	for (s = line; *s = c = get1char(); s++) {
		if (c == ')')
			inpar = 0;
		if (inpar)
			continue;
		if (c == '\n' || c == 0 || c == '.' || c == ',')
			break;
		else if (c == '(')
			inpar = 1;
		else if (s >= line + FLNLIM)
			error("too long spec line");
	}
	for (p = line; p < s; p++)
		switch (*p) {
		case 'l':
		case 'r':
		case 'c':
		case 'n':
		case 'a':
		case 's':
		case 'L':
		case 'R':
		case 'C':
		case 'N':
		case 'A':
		case 'S':
		case '-':
		case '=':
		case '_':
			n++;
		}
	while (p >= line)
		un1getc(*p--);
	return(n);
}


void
garray(int qcol)
{
	style =  (int (*)[]) getcore(MAXHEAD * qcol, sizeof(int));
	evenup = (int *) getcore(qcol, sizeof(int));
	lefline = (int (*)[]) getcore(MAXHEAD * (qcol + 1), sizeof (int)); /*+1 for sample55 loop - others may need it too*/
	font = (char (*)[][2]) getcore(MAXHEAD * qcol, 2);
	csize = (char (*)[MAXHEAD][4]) getcore(MAXHEAD * qcol, 4);
	vsize = (char (*)[MAXHEAD][4]) getcore(MAXHEAD * qcol, 4);
	flags =  (int (*)[]) getcore(MAXHEAD * qcol, sizeof(int));
	cll = (char (*)[])getcore(qcol, CLLEN);
	sep = (int *) getcore(qcol + 1, sizeof(int));
	sep++; /* sep[-1] must be legal */
	used = (int *) getcore(qcol + 1, sizeof(int));
	lused = (int *) getcore(qcol + 1, sizeof(int));
	rused = (int *) getcore(qcol + 1, sizeof(int));
	doubled = (int *) getcore(qcol + 1, sizeof(int));
	acase = (int *) getcore(qcol + 1, sizeof(int));
	topat = (int *) getcore(qcol + 1, sizeof(int));
}


char	*
getcore(int a, int b)
{
	char	*x;
	x = calloc(a, b);
	if (x == 0)
		error("Couldn't get memory");
	return(x);
}


void
freearr(void)
{
	free(style);
	free(evenup);
	free(lefline);
	free(flags);
	free(font);
	free(csize);
	free(vsize);
	free(cll);
	free(--sep);	/* netnews says this should be --sep because incremented earlier! */
	free(used);
	free(lused);
	free(rused);
	free(doubled);
	free(acase);
	free(topat);
}
