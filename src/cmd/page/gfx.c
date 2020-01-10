/*
 * graphics file reading for page
 */

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <bio.h>
#include <cursor.h>
#include "page.h"

typedef struct Convert	Convert;
typedef struct GfxInfo	GfxInfo;

struct Convert {
	char *name;
	char *cmd;
	char *truecmd;	/* cmd for true color */
};

struct GfxInfo {
	Graphic *g;
};

/*
 * N.B. These commands need to read stdin if %a is replaced
 * with an empty string.
 */
Convert cvt[] = {
	{ "plan9",	"fb/3to1 rgbv %a |fb/pcp -tplan9" },
	{ "tiff",	"fb/tiff2pic %a | fb/3to1 rgbv | fb/pcp -tplan9" },
	{ "jpeg",	"jpg -9 %a", "jpg -t9 %a" },
	{ "gif",	"gif -9 %a", "gif -t9 %a" },
	{ "inferno",	nil },
	{ "fax",	"aux/g3p9bit -g %a" },
	{ "unknown",	"fb/cvt2pic %a |fb/3to1 rgbv" },
	{ "plan9bm",	nil },
	{ "ppm",	"ppm -9 %a", "ppm -t9 %a" },
	{ "png",	"png -9 %a", "png -t9 %a" },
	{ "yuv",	"yuv -9 %a", "yuv -t9 %a"  },
	{ "bmp",	"bmp -9 %a", "bmp -t9 %a"  },
};

static Image*	gfxdrawpage(Document *d, int page);
static char*	gfxpagename(Document*, int);
static int	spawnrc(char*, Graphic*);
//static void	waitrc(void);
//static int	spawnpost(int);
static int	addpage(Document*, char*);
static int	rmpage(Document*, int);
static int	genaddpage(Document*, char*, uchar*, int);

static char*
gfxpagename(Document *doc, int page)
{
	GfxInfo *gfx = doc->extra;
	return gfx->g[page].name;
}

static Image*
gfxdrawpage(Document *doc, int page)
{
	GfxInfo *gfx = doc->extra;
	return convert(gfx->g+page);
}

Document*
initgfx(Biobuf *b, int argc, char **argv, uchar *buf, int nbuf)
{
	GfxInfo *gfx;
	Document *doc;
	int i;

	USED(b);

	doc = emalloc(sizeof(*doc));
	gfx = emalloc(sizeof(*gfx));
	gfx->g = nil;

	doc->npage = 0;
	doc->drawpage = gfxdrawpage;
	doc->pagename = gfxpagename;
	doc->addpage = addpage;
	doc->rmpage = rmpage;
	doc->extra = gfx;
	doc->fwdonly = 0;
	doc->type = Tgfx;

	fprint(2, "reading through graphics...\n");
	if(argc==0 && buf)
		genaddpage(doc, nil, buf, nbuf);
	else{
		for(i=0; i<argc; i++)
			if(addpage(doc, argv[i]) < 0)
				fprint(2, "warning: not including %s: %r\n", argv[i]);
	}

	return doc;
}

static int
genaddpage(Document *doc, char *name, uchar *buf, int nbuf)
{
	Graphic *g;
	GfxInfo *gfx;
	Biobuf *b;
	uchar xbuf[32];
	int i, l;

	l = 0;
	gfx = doc->extra;

	assert((name == nil) ^ (buf == nil));
	assert(name != nil || doc->npage == 0);

	for(i=0; i<doc->npage; i++)
		if(strcmp(gfx->g[i].name, name) == 0)
			return i;

	if(name){
		l = strlen(name);
		if((b = Bopen(name, OREAD)) == nil) {
			werrstr("Bopen: %r");
			return -1;
		}

		if(Bread(b, xbuf, sizeof xbuf) != sizeof xbuf) {
			werrstr("short read: %r");
			return -1;
		}
		Bterm(b);
		buf = xbuf;
		nbuf = sizeof xbuf;
	}


	gfx->g = erealloc(gfx->g, (doc->npage+1)*(sizeof(*gfx->g)));
	g = &gfx->g[doc->npage];

	memset(g, 0, sizeof *g);
	if(memcmp(buf, "GIF", 3) == 0)
		g->type = Igif;
	else if(memcmp(buf, "\111\111\052\000", 4) == 0)
		g->type = Itiff;
	else if(memcmp(buf, "\115\115\000\052", 4) == 0)
		g->type = Itiff;
	else if(memcmp(buf, "\377\330\377", 3) == 0)
		g->type = Ijpeg;
	else if(memcmp(buf, "\211PNG\r\n\032\n", 3) == 0)
		g->type = Ipng;
	else if(memcmp(buf, "compressed\n", 11) == 0)
		g->type = Iinferno;
	else if(memcmp(buf, "\0PC Research, Inc", 17) == 0)
		g->type = Ifax;
	else if(memcmp(buf, "TYPE=ccitt-g31", 14) == 0)
		g->type = Ifax;
	else if(memcmp(buf, "II*", 3) == 0)
		g->type = Ifax;
	else if(memcmp(buf, "TYPE=", 5) == 0)
		g->type = Ipic;
	else if(buf[0] == 'P' && '0' <= buf[1] && buf[1] <= '9')
		g->type = Ippm;
	else if(memcmp(buf, "BM", 2) == 0)
		g->type = Ibmp;
	else if(memcmp(buf, "          ", 10) == 0 &&
		'0' <= buf[10] && buf[10] <= '9' &&
		buf[11] == ' ')
		g->type = Iplan9bm;
	else if(strtochan((char*)buf) != 0)
		g->type = Iplan9bm;
	else if (l > 4 && strcmp(name + l -4, ".yuv") == 0)
		g->type = Iyuv;
	else
		g->type = Icvt2pic;

	if(name){
		g->name = estrdup(name);
		g->fd = -1;
	}else{
		g->name = estrdup("stdin");	/* so it can be freed */
		g->fd = stdinpipe(buf, nbuf);
	}

	if(chatty) fprint(2, "classified \"%s\" as \"%s\"\n", g->name, cvt[g->type].name);
	return doc->npage++;
}

static int
addpage(Document *doc, char *name)
{
	return genaddpage(doc, name, nil, 0);
}

static int
rmpage(Document *doc, int n)
{
	int i;
	GfxInfo *gfx;

	if(n < 0 || n >= doc->npage)
		return -1;

	gfx = doc->extra;
	doc->npage--;
	free(gfx->g[n].name);

	for(i=n; i<doc->npage; i++)
		gfx->g[i] = gfx->g[i+1];

	if(n < doc->npage)
		return n;
	if(n == 0)
		return 0;
	return n-1;
}


Image*
convert(Graphic *g)
{
	int fd;
	Convert c;
	char *cmd;
	char *name, buf[1000];
	Image *im;
	int rcspawned = 0;

	c = cvt[g->type];
	if(c.cmd == nil) {
		if(chatty) fprint(2, "no conversion for bitmap \"%s\"...\n", g->name);
		if(g->fd < 0){	/* not stdin */
			fd = open(g->name, OREAD);
			if(fd < 0) {
				fprint(2, "cannot open file: %r\n");
				wexits("open");
			}
		}else
			fd = g->fd;
	} else {
		cmd = c.cmd;
		if(truecolor && c.truecmd)
			cmd = c.truecmd;

		if(g->fd >= 0)	/* is pipe */
			name = "";
		else
			name = g->name;
		if(strlen(cmd)+strlen(name) > sizeof buf) {
			fprint(2, "command too long\n");
			wexits("convert");
		}
		snprint(buf, sizeof buf, cmd, name);
		if(chatty) fprint(2, "using \"%s\" to convert \"%s\"...\n", buf, g->name);
		fd = spawnrc(buf, g);
		rcspawned++;
		if(fd < 0) {
			fprint(2, "cannot spawn converter: %r\n");
			wexits("convert");
		}
	}

	im = readimage(display, fd, 0);
	if(im == nil) {
		fprint(2, "warning: couldn't read image: %r\n");
	}

	close(fd);
	return im;
}

static int
spawnrc(char *cmd, Graphic *g)
{
	int pfd[2];
	int fd[3];

	if(chatty) fprint(2, "spawning(%s)...", cmd);

	if(pipe(pfd) < 0)
		return -1;

	if(g->fd > 0)
		fd[0] = dup(g->fd, -1);
	else
		fd[0] = open("/dev/null", OREAD);
	fd[1] = pfd[1];
	fd[2] = dup(2, -1);

	if(threadspawnl(fd, "rc", "rc", "-c", cmd, nil) == -1)
		return -1;

	return pfd[0];
}
