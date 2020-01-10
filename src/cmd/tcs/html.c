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

/*
 * Names beginning with _ are names we recognize
 * (without the underscore) but will not generate,
 * because they are nonstandard.
 */
static Hchar byname[] =
{
	{"AElig", 198},
	{"Aacute", 193},
	{"Acirc", 194},
	{"Agrave", 192},
	{"Alpha", 913},
	{"Aring", 197},
	{"Atilde", 195},
	{"Auml", 196},
	{"Beta", 914},
	{"Ccedil", 199},
	{"Chi", 935},
	{"Dagger", 8225},
	{"Delta", 916},
	{"ETH", 208},
	{"Eacute", 201},
	{"Ecirc", 202},
	{"Egrave", 200},
	{"Epsilon", 917},
	{"Eta", 919},
	{"Euml", 203},
	{"Gamma", 915},
	{"Iacute", 205},
	{"Icirc", 206},
	{"Igrave", 204},
	{"Iota", 921},
	{"Iuml", 207},
	{"Kappa", 922},
	{"Lambda", 923},
	{"Mu", 924},
	{"Ntilde", 209},
	{"Nu", 925},
	{"OElig", 338},
	{"Oacute", 211},
	{"Ocirc", 212},
	{"Ograve", 210},
	{"Omega", 937},
	{"Omicron", 927},
	{"Oslash", 216},
	{"Otilde", 213},
	{"Ouml", 214},
	{"Phi", 934},
	{"Pi", 928},
	{"Prime", 8243},
	{"Psi", 936},
	{"Rho", 929},
	{"Scaron", 352},
	{"Sigma", 931},
	{"THORN", 222},
	{"Tau", 932},
	{"Theta", 920},
	{"Uacute", 218},
	{"Ucirc", 219},
	{"Ugrave", 217},
	{"Upsilon", 933},
	{"Uuml", 220},
	{"Xi", 926},
	{"Yacute", 221},
	{"Yuml", 376},
	{"Zeta", 918},
	{"aacute", 225},
	{"acirc", 226},
	{"acute", 180},
	{"aelig", 230},
	{"agrave", 224},
	{"alefsym", 8501},
	{"alpha", 945},
	{"amp", 38},
	{"and", 8743},
	{"ang", 8736},
	{"aring", 229},
	{"asymp", 8776},
	{"atilde", 227},
	{"auml", 228},
	{"bdquo", 8222},
	{"beta", 946},
	{"brvbar", 166},
	{"bull", 8226},
	{"cap", 8745},
	{"ccedil", 231},
	{"cdots", 8943},
	{"cedil", 184},
	{"cent", 162},
	{"chi", 967},
	{"circ", 710},
	{"clubs", 9827},
	{"cong", 8773},
	{"copy", 169},
	{"crarr", 8629},
	{"cup", 8746},
	{"curren", 164},
	{"dArr", 8659},
	{"dagger", 8224},
	{"darr", 8595},
	{"ddots", 8945},
	{"deg", 176},
	{"delta", 948},
	{"diams", 9830},
	{"divide", 247},
	{"eacute", 233},
	{"ecirc", 234},
	{"egrave", 232},
	{"_emdash", 8212},	/* non-standard but commonly used */
	{"empty", 8709},
	{"emsp", 8195},
	{"_endash", 8211},	/* non-standard but commonly used */
	{"ensp", 8194},
	{"epsilon", 949},
	{"equiv", 8801},
	{"eta", 951},
	{"eth", 240},
	{"euml", 235},
	{"euro", 8364},
	{"exist", 8707},
	{"fnof", 402},
	{"forall", 8704},
	{"frac12", 189},
	{"frac14", 188},
	{"frac34", 190},
	{"frasl", 8260},
	{"gamma", 947},
	{"ge", 8805},
	{"gt", 62},
	{"hArr", 8660},
	{"harr", 8596},
	{"hearts", 9829},
	{"hellip", 8230},
	{"iacute", 237},
	{"icirc", 238},
	{"iexcl", 161},
	{"igrave", 236},
	{"image", 8465},
	{"infin", 8734},
	{"int", 8747},
	{"iota", 953},
	{"iquest", 191},
	{"isin", 8712},
	{"iuml", 239},
	{"kappa", 954},
	{"lArr", 8656},
	{"lambda", 955},
	{"lang", 9001},
	{"laquo", 171},
	{"larr", 8592},
	{"lceil", 8968},
	{"_ldots", 8230},
	{"ldquo", 8220},
	{"le", 8804},
	{"lfloor", 8970},
	{"lowast", 8727},
	{"loz", 9674},
	{"lrm", 8206},
	{"lsaquo", 8249},
	{"lsquo", 8216},
	{"lt", 60},
	{"macr", 175},
	{"mdash", 8212},
	{"micro", 181},
	{"middot", 183},
	{"minus", 8722},
	{"mu", 956},
	{"nabla", 8711},
	{"nbsp", 160},
	{"ndash", 8211},
	{"ne", 8800},
	{"ni", 8715},
	{"not", 172},
	{"notin", 8713},
	{"nsub", 8836},
	{"ntilde", 241},
	{"nu", 957},
	{"oacute", 243},
	{"ocirc", 244},
	{"oelig", 339},
	{"ograve", 242},
	{"oline", 8254},
	{"omega", 969},
	{"omicron", 959},
	{"oplus", 8853},
	{"or", 8744},
	{"ordf", 170},
	{"ordm", 186},
	{"oslash", 248},
	{"otilde", 245},
	{"otimes", 8855},
	{"ouml", 246},
	{"para", 182},
	{"part", 8706},
	{"permil", 8240},
	{"perp", 8869},
	{"phi", 966},
	{"pi", 960},
	{"piv", 982},
	{"plusmn", 177},
	{"pound", 163},
	{"prime", 8242},
	{"prod", 8719},
	{"prop", 8733},
	{"psi", 968},
	{"quad", 8193},
	{"quot", 34},
	{"rArr", 8658},
	{"radic", 8730},
	{"rang", 9002},
	{"raquo", 187},
	{"rarr", 8594},
	{"rceil", 8969},
	{"rdquo", 8221},
	{"real", 8476},
	{"reg", 174},
	{"rfloor", 8971},
	{"rho", 961},
	{"rlm", 8207},
	{"rsaquo", 8250},
	{"rsquo", 8217},
	{"sbquo", 8218},
	{"scaron", 353},
	{"sdot", 8901},
	{"sect", 167},
	{"shy", 173},
	{"sigma", 963},
	{"sigmaf", 962},
	{"sim", 8764},
	{"_sp", 8194},
	{"spades", 9824},
	{"sub", 8834},
	{"sube", 8838},
	{"sum", 8721},
	{"sup", 8835},
	{"sup1", 185},
	{"sup2", 178},
	{"sup3", 179},
	{"supe", 8839},
	{"szlig", 223},
	{"tau", 964},
	{"there4", 8756},
	{"theta", 952},
	{"thetasym", 977},
	{"thinsp", 8201},
	{"thorn", 254},
	{"tilde", 732},
	{"times", 215},
	{"trade", 8482},
	{"uArr", 8657},
	{"uacute", 250},
	{"uarr", 8593},
	{"ucirc", 251},
	{"ugrave", 249},
	{"uml", 168},
	{"upsih", 978},
	{"upsilon", 965},
	{"uuml", 252},
	{"_varepsilon", 8712},
	{"varphi", 981},
	{"_varpi", 982},
	{"varrho", 1009},
	{"vdots", 8942},
	{"_vsigma", 962},
	{"_vtheta", 977},
	{"weierp", 8472},
	{"xi", 958},
	{"yacute", 253},
	{"yen", 165},
	{"yuml", 255},
	{"zeta", 950},
	{"zwj", 8205},
	{"zwnj", 8204}
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
	int i;

	if(init)
		return;
	init = 1;
	memmove(byrune, byname, sizeof byrune);

	/* Eliminate names we aren't allowed to generate. */
	for(i=0; i<nelem(byrune); i++){
		if(byrune[i].s[0] == '_'){
			byrune[i].r = Runeerror;
			byname[i].s++;
		}
	}

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

	if(r == Runeerror)
		return nil;
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
	OUT(out, rbuf, 0);
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

	USED(x);
	html_init();
	Binit(&b, 1, OWRITE);
	er = r+n;
	for(; r<er; r++){
		if(*r < Runeself)
			Bputrune(&b, *r);
		else if((s = findbyrune(*r)) != nil)
			Bprint(&b, "&%s;", s);
		else
			Bprint(&b, "&#%d;", *r);
	}
	Bflush(&b);
}
