#include <u.h>
#include <libc.h>
#include <bio.h>
#include "../common/common.h"
#include "tr2post.h"

#undef isspace
#define isspace risspace

int
isspace(Rune r)
{
	return(r==' ' || r=='\t' || r=='\n' || r == '\r' || r=='\f');
}

int
Bskipws(Biobuf *bp) {
	int r;
	int sindex = 0;

	/* skip over initial white space */
	do {
		r = Bgetrune(bp);
		if (r == '\n') inputlineno++;
		sindex++;
	} while (r>=0 && isspace(r));
	if (r<0) {
		return(-1);
	} else if (!isspace(r)) {
		Bungetrune(bp);
		--sindex;
	}
	return(sindex);
}

int
asc2dig(char c, int base) {
	if (c >= '0' && c <= '9'){
		if (base == 8 && c > '7') return(-1);
		else return(c - '0');
	}

	if (base == 16){
		if (c >= 'a' && c <= 'f') return(10 + c - 'a');
		else if (c >= 'A' && c <= 'F') return(10 + c - 'A');
	}

	return(-1);
}

/* get a string of type: "d" for decimal integer, "u" for unsigned,
 * "s" for string", "c" for char,
 * return the number of characters gotten for the field.  If nothing
 * was gotten and the end of file was reached, a negative value
 * from the Bgetrune is returned.
 */

int
Bgetfield(Biobuf *bp, int type, void *thing, int size) {
	int r;
	Rune R;
	int sindex = 0, i, j, n = 0;
	int negate = 0;
	int base = 10;
	BOOLEAN bailout = FALSE;
	int dig;
	unsigned int u = 0;

	r = 0;

	/* skip over initial white space */
	if (Bskipws(bp) < 0)
		return(-1);

	switch (type) {
	case 'd':
		while (!bailout && (r = Bgetrune(bp))>=0) {
			switch (sindex++) {
			case 0:
				switch (r) {
				case '-':
					negate = 1;
					continue;
				case '+':
					continue;
				case '0':
					base = 8;
					continue;
				default:
					break;
				}
				break;
			case 1:
				if ((r == 'x' || r == 'X') && base == 8) {
					base = 16;
					continue;
				}
			}
			if ((dig = asc2dig(r, base)) == -1) bailout = TRUE;
			else n = dig + (n * base);
		}
		if (r < 0) return(-1);
		*(int *)thing = (negate)?-n:n;
		Bungetrune(bp);
		break;
	case 'u':
		while (!bailout && (r = Bgetrune(bp))>=0) {
			switch (sindex++) {
			case 0:
				if (r == '0') {
					base = 8;
					continue;
				}
				break;
			case 1:
				if ((r == 'x' || r == 'X') && base == 8) {
					base = 16;
					continue;
				}
			}
			if ((dig = asc2dig(r, base)) == -1) bailout = TRUE;
			else u = dig + (n * base);
		}
		*(int *)thing = u;
		if (r < 0) return(-1);
		Bungetrune(bp);
		break;
	case 's':
		j = 0;
		while ((size>j+UTFmax) && (r = Bgetrune(bp))>=0 && !isspace(r)) {
			R = r;
			i = runetochar(&(((char *)thing)[j]), &R);
			j += i;
			sindex++;
		}
		((char *)thing)[j++] = '\0';
		if (r < 0) return(-1);
		Bungetrune(bp);
		break;
	case 'r':
		if ((r = Bgetrune(bp))>=0) {
			*(Rune *)thing = r;
			sindex++;
			return(sindex);
		}
		if (r <= 0) return(-1);
		Bungetrune(bp);
		break;
	default:
		return(-2);
	}
	if (r < 0 && sindex == 0)
		return(r);
	else if (bailout && sindex == 1) {
		return(0);
	} else
		return(sindex);
}
