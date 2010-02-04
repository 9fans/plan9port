#undef pipe

typedef struct Document Document;

struct Document {
	char *docname;
	int npage;
	int fwdonly;
	char* (*pagename)(Document*, int);
	Image* (*drawpage)(Document*, int);
	int	(*addpage)(Document*, char*);
	int	(*rmpage)(Document*, int);
	Biobuf *b;
	void *extra;
	int type;
};

typedef struct Graphic	Graphic;

struct Graphic {
	int type;
	int fd;
	char *name;
};

enum {
	Ipic,
	Itiff,
	Ijpeg,
	Igif,
	Iinferno,
	Ifax,
	Icvt2pic,
	Iplan9bm,
	Ippm,
	Ipng,
	Iyuv,
	Ibmp,
};

enum {
	Tgfx,
	Tpdf,
	Tps,
}
;

void *emalloc(int);
void *erealloc(void*, int);
char *estrdup(char*);
int spawncmd(char*, char **, int, int, int);

int spooltodisk(uchar*, int, char**);
int stdinpipe(uchar*, int);
Document *initps(Biobuf*, int, char**, uchar*, int);
Document *initpdf(Biobuf*, int, char**, uchar*, int);
Document *initgfx(Biobuf*, int, char**, uchar*, int);
Document *inittroff(Biobuf*, int, char**, uchar*, int);
Document *initdvi(Biobuf*, int, char**, uchar*, int);
Document *initmsdoc(Biobuf*, int, char**, uchar*, int);

void viewer(Document*);
extern Cursor reading;
extern int chatty;
extern int goodps;
extern int textbits, gfxbits;
extern int reverse;
extern int clean;
extern int ppi;
extern int teegs;
extern int truetoboundingbox;
extern int wctlfd;
extern int resizing;
extern int mknewwindow;
extern int fitwin;

void rot180(Image*);
Image *rot90(Image*);
Image *rot270(Image*);
Image *resample(Image*, Image*);

/* ghostscript interface shared by ps, pdf */
typedef struct GSInfo	GSInfo;
typedef struct PDFInfo	PDFInfo;
typedef struct Page	Page;
typedef struct PSInfo	PSInfo;
struct GSInfo {
	Graphic g;
	int gsfd;
	Biobuf gsrd;
	int gspid;
	int ppi;
};
struct PDFInfo {
	GSInfo gs;
	Rectangle *pagebbox;
};
struct Page {
	char *name;
	int offset;			/* offset of page beginning within file */
};
struct PSInfo {
	GSInfo gs;
	Rectangle bbox;	/* default bounding box */
	Page *page;
	int npage;
	int clueless;	/* don't know where page boundaries are */
	long psoff;	/* location of %! in file */
	char ctm[256];
};

void	waitgs(GSInfo*);
int	gscmd(GSInfo*, char*, ...);
int	spawngs(GSInfo*, char*);
void	setdim(GSInfo*, Rectangle, int, int);
int	spawnwriter(GSInfo*, Biobuf*);
Rectangle	screenrect(void);
void	newwin(void);
void	zerox(void);
Rectangle winrect(void);
void	resize(int, int);
int	max(int, int);
int	min(int, int);
void	wexits(char*);
Image*	xallocimage(Display*, Rectangle, ulong, int, ulong);
int	bell(void*, char*);
Image*	convert(Graphic *g);
Image*	cachedpage(Document*, int, int);
void	cacheflush(void);
void   fit(void);

extern char tempfile[40];

extern int stdinfd;
extern int truecolor;


/* BUG BUG BUG BUG BUG: cannot use new draw operations in drawterm,
 * or in vncs, and there is a bug in the kernel for copying images
 * from cpu memory -> video memory (memmove is not being used).
 * until all that is settled, ignore the draw operators.
 */
#define drawop(a,b,c,d,e,f) draw(a,b,c,d,e)
#define gendrawop(a,b,c,d,e,f,g) gendraw(a,b,c,d,e,f)
