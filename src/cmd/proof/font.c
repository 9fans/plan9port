#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <bio.h>
#include "proof.h"

char	fname[NFONT][20];		/* font names */
char lastload[NFONT][20];	/* last file name prefix loaded for this font */
Font	*fonttab[NFONT][NSIZE];	/* pointers to fonts */
int	fmap[NFONT];		/* what map to use with this font */

static void	loadfont(int, int);
static void	fontlookup(int, char *);
static void	buildxheight(Biobuf*);
static void	buildmap(Biobuf*);
static void	buildtroff(char *);
static void	addmap(int, char *, int);
static char	*map(Rune*, int);
static void	scanstr(char *, char *, char **);

int	specfont;	/* somehow, number of special font */

#define	NMAP	5
#define	QUICK	2048	/* char values less than this are quick to look up */
#define	eq(s,t)	strcmp((char *) s, (char *) t) == 0

int	curmap	= -1;	/* what map are we working on */

typedef struct Link Link;
struct Link	/* link names together */
{
	uchar	*name;
	int	val;
	Link	*next;
};

typedef struct Map Map;
struct Map	/* holds a mapping from uchar name to index */
{
	double	xheight;
	Rune	quick[QUICK];	/* low values get special treatment */
	Link	*slow;	/* other stuff goes into a link list */
};

Map	charmap[5];

typedef struct Fontmap Fontmap;
struct Fontmap	/* mapping from troff name to filename */
{
	char	*troffname;
	char	*prefix;
	int	map;		/* which charmap to use for this font */
	char	*fallback;	/* font to look in if can't find char here */
};

Fontmap	fontmap[100];
int	pos2fontmap[NFONT];	/* indexed by troff font position, gives Fontmap */
int	nfontmap	= 0;	/* how many are there */


void
dochar(Rune r[])
{
	char *s, *fb;
	Font *f;
	Point p;
	int fontno, fm, i;
	char buf[10];

	fontno = curfont;
	if((s = map(r, curfont)) == 0){		/* not on current font */
		if ((s = map(r, specfont)) != 0)	/* on special font */
			fontno = specfont;
		else{
			/* look for fallback */
			fm = pos2fontmap[curfont];
			fb = fontmap[fm].fallback;
			if(fb){
				/* see if fallback is mounted */
				for(i = 0; i < NFONT; i++){
					if(eq(fb, fontmap[pos2fontmap[i]].troffname)){
						s = map(r, i);
						if(s){
							fontno = i;
							goto found;
						}
					}
				}
			}
			/* no such char; use name itself on defont */
			/* this is not a general solution */
			p.x = hpos/DIV + xyoffset.x + offset.x;
			p.y = vpos/DIV + xyoffset.y + offset.y;
			p.y -= font->ascent;
			sprint(buf, "%S", r);
			string(screen, p, display->black, ZP, font, buf);
			return;
		}
	}
    found:
	p.x = hpos/DIV + xyoffset.x + offset.x;
	p.y = vpos/DIV + xyoffset.y + offset.y;
	while ((f = fonttab[fontno][cursize]) == 0)
		loadfont(fontno, cursize);
	p.y -= f->ascent;
	dprint(2, "putting %S at %d,%d font %d, size %d\n", r, p.x, p.y, fontno, cursize);
	string(screen, p, display->black, ZP, f, s);
}

static int drawlog2[] = {
	0, 0,
	1, 1,
	2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	5
};

static void
loadfont(int n, int s)
{
	char file[100];
	int i, fd, t, deep;
	static char *try[3] = {"", "times/R.", "pelm/"};
	Subfont *f;
	Font *ff;

	try[0] = fname[n];
	dprint(2, "loadfont %d %d\n", n, s);
	for (t = 0; t < 3; t++){
		i = s * mag * charmap[fmap[n]].xheight/0.72;	/* a pixel is 0.72 points */
		if (i < MINSIZE)
			i = MINSIZE;
		dprint(2, "size %d, i %d, mag %g\n", s, i, mag);
		for(; i >= MINSIZE; i--){
			/* if .font file exists, take that */
			sprint(file, "%s/%s%d.font", libfont, try[t], i);
			ff = openfont(display, file);
			if(ff != 0){
				fonttab[n][s] = ff;
				dprint(2, "using %s for font %d %d\n", file, n, s);
				return;
			}
			/* else look for a subfont file */
			for (deep = drawlog2[screen->depth]; deep >= 0; deep--){
				sprint(file, "%s/%s%d.%d", libfont, try[t], i, deep);
				dprint(2, "trying %s for %d\n", file, i);
				if ((fd = open(file, 0)) >= 0){
					f = readsubfont(display, file, fd, 0);
					if (f == 0) {
						fprint(2, "can't rdsubfontfile %s: %r\n", file);
						exits("rdsubfont");
					}
					close(fd);
					ff = mkfont(f, 0);
					if(ff == 0){
						fprint(2, "can't mkfont %s: %r\n", file);
						exits("rdsubfont");
					}
					fonttab[n][s] = ff;
					dprint(2, "using %s for font %d %d\n", file, n, s);
					return;
				}
			}
		}
	}
	fprint(2, "can't find font %s.%d or substitute, quitting\n", fname[n], s);
	exits("no font");
}

void
loadfontname(int n, char *s)
{
	int i;
	Font *f, *g = 0;

	if (strcmp(s, fname[n]) == 0)
		return;
	if(fname[n] && fname[n][0]){
		if(lastload[n] && strcmp(lastload[n], fname[n]) == 0)
			return;
		strcpy(lastload[n], fname[n]);
	}
	fontlookup(n, s);
	for (i = 0; i < NSIZE; i++)
		if (f = fonttab[n][i]){
			if (f != g) {
				freefont(f);
				g = f;
			}
			fonttab[n][i] = 0;
		}
}

void
allfree(void)
{
	int i;

	for (i=0; i<NFONT; i++)
		loadfontname(i, "??");
}


void
readmapfile(char *file)
{
	Biobuf *fp;
	char *p, cmd[100];

	if ((fp=Bopen(file, OREAD)) == 0){
		fprint(2, "proof: can't open map file %s\n", file);
		exits("urk");
	}
	while((p=Brdline(fp, '\n')) != 0) {
		p[Blinelen(fp)-1] = 0;
		scanstr(p, cmd, 0);
		if(p[0]=='\0' || eq(cmd, "#"))	/* skip comments, empty */
			continue;
		else if(eq(cmd, "xheight"))
			buildxheight(fp);
		else if(eq(cmd, "map"))
			buildmap(fp);
		else if(eq(cmd, "special"))
			buildtroff(p);
		else if(eq(cmd, "troff"))
			buildtroff(p);
		else
			fprint(2, "weird map line %s\n", p);
	}
	Bterm(fp);
}

static void
buildxheight(Biobuf *fp)	/* map goes from char name to value to print via *string() */
{
	char *line;

	line = Brdline(fp, '\n');
	if(line == 0){
		fprint(2, "proof: bad map file\n");
		exits("map");
	}
	charmap[curmap].xheight = atof(line);
}

static void
buildmap(Biobuf *fp)	/* map goes from char name to value to print via *string() */
{
	uchar *p, *line, ch[100];
	int val;
	Rune r;

	curmap++;
	if(curmap >= NMAP){
		fprint(2, "proof: out of char maps; recompile\n");
		exits("charmap");
	}
	while ((line = Brdline(fp, '\n'))!= 0){
		if (line[0] == '\n')
			return;
		line[Blinelen(fp)-1] = 0;
		scanstr((char *) line, (char *) ch, (char **)(void*)&p);
		if (ch[0] == '\0') {
			fprint(2, "bad map file line '%s'\n", (char*)line);
			continue;
		}
		val = strtol((char *) p, 0, 10);
dprint(2, "buildmap %s (%x %x) %s %d\n", (char*)ch, ch[0], ch[1], (char*)p, val);
		chartorune(&r, (char*)ch);
		if(utflen((char*)ch)==1 && r<QUICK)
			charmap[curmap].quick[r] = val;
		else
			addmap(curmap, strdup((char *) ch), val);	/* put somewhere else */
	}
}

static void
addmap(int n, char *s, int val)	/* stick a new link on */
{
	Link *p = (Link *) malloc(sizeof(Link));
	Link *prev = charmap[n].slow;

	if(p == 0)
		exits("out of memory in addmap");
	p->name = (uchar *) s;
	p->val = val;
	p->next = prev;
	charmap[n].slow = p;
}

static void
buildtroff(char *buf)	/* map troff names into bitmap filenames */
{				/* e.g., R -> times/R., I -> times/I., etc. */
	char *p, cmd[100], name[200], prefix[400], fallback[100];

	scanstr(buf, cmd, &p);
	scanstr(p, name, &p);
	scanstr(p, prefix, &p);
	while(*p!=0 && isspace(*p))
		p++;
	if(*p != 0){
		scanstr(p, fallback, &p);
		fontmap[nfontmap].fallback = strdup(fallback);
	}else
		fontmap[nfontmap].fallback = 0;
	fontmap[nfontmap].troffname = strdup(name);
	fontmap[nfontmap].prefix = strdup(prefix);
	fontmap[nfontmap].map = curmap;
	dprint(2, "troff name %s is bitmap %s map %d in slot %d fallback %s\n", name, prefix, curmap, nfontmap, fontmap[nfontmap].fallback? fontmap[nfontmap].fallback : "<null>");
	nfontmap++;
}

static void
fontlookup(int n, char *s)	/* map troff name of s into position n */
{
	int i;

	for(i = 0; i < nfontmap; i++)
		if (eq(s, fontmap[i].troffname)) {
			strcpy(fname[n], fontmap[i].prefix);
			fmap[n] = fontmap[i].map;
			pos2fontmap[n] = i;
			if (eq(s, "S"))
				specfont = n;
			dprint(2, "font %d %s is %s\n", n, s, fname[n]);
			return;
		}
	/* god help us if this font isn't there */
}


static char *
map(Rune rp[], int font)	/* figure out mapping for char in this font */
{
	static char s[100];
	char c[10];
	Link *p;
	Rune r;

	if(rp[1]==0 &&  rp[0]<QUICK)	/* fast lookup */
		r = charmap[fmap[font]].quick[rp[0]];
	else {	/* high-valued or compound character name */
		sprint(c, "%S", rp);
		r = 0;
		for (p = charmap[fmap[font]].slow; p; p = p->next)
			if(eq(c, p->name)){
				r = p->val;
				break;
			}
	}
	if(r == 0){	/* not there */
		dprint(2, "didn't find %S font# %d\n", rp, font);
		return 0;
	}
	dprint(2, "map %S to %s font# %d\n", rp, s, font);
	s[runetochar(s, &r)] = 0;
	return s;
}

static void
scanstr(char *s, char *ans, char **ep)
{
	for (; isspace((uchar) *s); s++)
		;
	for (; *s!=0 && !isspace((uchar) *s); )
		*ans++ = *s++;
	*ans = 0;
	if (ep)
		*ep = s;
}
