#include <u.h>
#include <libc.h>
#include <bio.h>
#include "hdr.h"
#include "conv.h"

typedef struct Hchar Hchar;
struct Hchar
{
	char *s;
	Rune r;
};

/* &lt;, &gt;, &quot;, &amp; intentionally omitted */

static Hchar byname[] =
{
	{"AElig", 198},
	{"Aacute", 193},
	{"Acirc", 194},
	{"Agrave", 192},
	{"Aring", 197},
	{"Atilde", 195},
	{"Auml", 196},
	{"Ccedil", 199},
	{"ETH", 208},
	{"Eacute", 201},
	{"Ecirc", 202},
	{"Egrave", 200},
	{"Euml", 203},
	{"Iacute", 205},
	{"Icirc", 206},
	{"Igrave", 204},
	{"Iuml", 207},
	{"Ntilde", 209},
	{"Oacute", 211},
	{"Ocirc", 212},
	{"Ograve", 210},
	{"Oslash", 216},
	{"Otilde", 213},
	{"Ouml", 214},
	{"THORN", 222},
	{"Uacute", 218},
	{"Ucirc", 219},
	{"Ugrave", 217},
	{"Uuml", 220},
	{"Yacute", 221},
	{"aacute", 225},
	{"acirc", 226},
	{"acute", 180},
	{"aelig", 230},
	{"agrave", 224},
	{"alpha", 945},
	{"aring", 229},
	{"atilde", 227},
	{"auml", 228},
	{"beta", 946},
	{"brvbar", 166},
	{"ccedil", 231},
	{"cdots", 8943},
	{"cedil", 184},
	{"cent", 162},
	{"chi", 967},
	{"copy", 169},
	{"curren", 164},
	{"ddots", 8945},
	{"deg", 176},
	{"delta", 948},
	{"divide", 247},
	{"eacute", 233},
	{"ecirc", 234},
	{"egrave", 232},
	{"emdash", 8212},	/* non-standard but commonly used */
	{"emsp", 8195},
	{"endash", 8211},	/* non-standard but commonly used */
	{"ensp", 8194},
	{"epsilon", 949},
	{"eta", 951},
	{"eth", 240},
	{"euml", 235},
	{"frac12", 189},
	{"frac14", 188},
	{"frac34", 190},
	{"gamma", 947},
	{"iacute", 237},
	{"icirc", 238},
	{"iexcl", 161},
	{"igrave", 236},
	{"iota", 953},
	{"iquest", 191},
	{"iuml", 239},
	{"kappa", 954},
	{"lambda", 955},
	{"laquo", 171},
	{"ldquo", 8220},
	{"ldots", 8230},
	{"lsquo", 8216},
	{"macr", 175},
	{"mdash", 8212},
	{"micro", 181},
	{"middot", 183},
	{"mu", 956},
	{"nbsp", 160},
	{"ndash", 8211},
	{"not", 172},
	{"ntilde", 241},
	{"nu", 957},
	{"oacute", 243},
	{"ocirc", 244},
	{"ograve", 242},
	{"omega", 969},
	{"omicron", 959},
	{"ordf", 170},
	{"ordm", 186},
	{"oslash", 248},
	{"otilde", 245},
	{"ouml", 246},
	{"para", 182},
	{"phi", 966},
	{"pi", 960},
	{"plusmn", 177},
	{"pound", 163},
	{"psi", 968},
	{"quad", 8193},
	{"raquo", 187},
	{"rdquo", 8221},
	{"reg", 174},
	{"rho", 961},
	{"rsquo", 8217},
	{"sect", 167},
	{"shy", 173},
	{"sigma", 963},
	{"sp", 8194},
	{"sup1", 185},
	{"sup2", 178},
	{"sup3", 179},
	{"szlig", 223},
	{"tau", 964},
	{"theta", 952},
	{"thinsp", 8201},
	{"thorn", 254},
	{"times", 215},
	{"trade", 8482},
	{"uacute", 250},
	{"ucirc", 251},
	{"ugrave", 249},
	{"uml", 168},
	{"upsilon", 965},
	{"uuml", 252},
	{"varepsilon", 8712},
	{"varphi", 981},
	{"varpi", 982},
	{"varrho", 1009},
	{"vdots", 8942},
	{"vsigma", 962},
	{"vtheta", 977},
	{"xi", 958},
	{"yacute", 253},
	{"yen", 165},
	{"yuml", 255},
	{"zeta", 950}
};

static Hchar byrune[nelem(byname)];

static int
hnamecmp(const void *va, const void *vb)
{
	Hchar *a, *b;
	
	a = (Hchar*)va;
	b = (Hchar*)vb;
	return strcmp(a->s, b->s);
}

static int
hrunecmp(const void *va, const void *vb)
{
	Hchar *a, *b;
	
	a = (Hchar*)va;
	b = (Hchar*)vb;
	return a->r - b->r;
}

static void
html_init(void)
{
	static int init;
	
	if(init)
		return;
	init = 1;
	memmove(byrune, byname, sizeof byrune);
	qsort(byname, nelem(byname), sizeof byname[0], hnamecmp);
	qsort(byrune, nelem(byrune), sizeof byrune[0], hrunecmp);
}

static Rune
findbyname(char *s)
{
	Hchar *h;
	int n, m, x;
	
	h = byname;
	n = nelem(byname);
	while(n > 0){
		m = n/2;
		x = strcmp(h[m].s, s);
		if(x == 0)
			return h[m].r;
		if(x < 0){
			h += m+1;
			n -= m+1;
		}else
			n = m;
	}
	return Runeerror;
}

static char*
findbyrune(Rune r)
{
	Hchar *h;
	int n, m;

	h = byrune;
	n = nelem(byrune);
	while(n > 0){
		m = n/2;
		if(h[m].r == r)
			return h[m].s;
		if(h[m].r < r){
			h += m+1;
			n -= m+1;
		}else
			n = m;
	}
	return nil;
}

void
html_in(int fd, long *x, struct convert *out)
{
	char buf[100], *p;
	Biobuf b;
	Rune rbuf[N];
	Rune *r, *er;
	int c, i;
	
	USED(x);
	
	html_init();
	r = rbuf;
	er = rbuf+N;
	Binit(&b, fd, OREAD);
	while((c = Bgetrune(&b)) != Beof){
		if(r >= er){
			OUT(out, rbuf, r-rbuf);
			r = rbuf;
		}
		if(c == '&'){
			buf[0] = c;
			for(i=1; i<nelem(buf)-1;){
				c = Bgetc(&b);
				if(c == Beof)
					break;
				buf[i++] = c;
				if(strchr("; \t\r\n", c))
					break;
			}
			buf[i] = 0;
			if(buf[i-1] == ';'){
				buf[i-1] = 0;
				if((c = findbyname(buf+1)) != Runeerror){
					*r++ = c;
					continue;
				}
				buf[i-1] = ';';
				if(buf[1] == '#'){
					if(buf[2] == 'x')
						c = strtol(buf+3, &p, 16);
					else
						c = strtol(buf+2, &p, 10);
					if(*p != ';' || c >= NRUNE || c < 0)
						goto bad;
					*r++ = c;
					continue;
				}
			}
		bad:
			for(p=buf; p<buf+i; ){
				p += chartorune(r++, p);
				if(r >= er){
					OUT(out, rbuf, r-rbuf);
					r = rbuf;
				}
			}
			continue;
		}
		*r++ = c;
	}
	if(r > rbuf)
		OUT(out, rbuf, r-rbuf);
}

/*
 * use biobuf because can use more than UTFmax bytes per rune
 */
void
html_out(Rune *r, int n, long *x)
{
	char *s;
	Biobuf b;
	Rune *er;
	
	html_init();
	Binit(&b, 1, OWRITE);
	er = r+n;
	for(; r<er; r++){
		if(*r < Runeself)
			Bputrune(&b, *r);
		else if((s = findbyrune(*r)) != nil)
			Bprint(&b, "&%s;", s);
		else
			Bprint(&b, "&#x%04x;", *r);
	}
	Bflush(&b);
}

