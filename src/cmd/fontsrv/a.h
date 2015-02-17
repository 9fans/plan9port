typedef struct XFont XFont;
XFont *xfont;
int nxfont;

struct XFont
{
	char *name;
	int loaded;
	uchar range[256];	// range[i] == whether to have subfont i<<8 to (i+1)<<8.
	int nrange;
	int unit;
	double height;
	double originy;
	void (*loadheight)(XFont*, int, int*, int*);

	// fontconfig workarround, as FC_FULLNAME does not work for matching fonts.
	char *fontfile;
	int  index;
};

void	loadfonts(void);
void	load(XFont*);
Memsubfont*	mksubfont(XFont*, char*, int, int, int, int);

extern XFont *xfont;
extern int nxfont;
void *emalloc9p(ulong);
extern Memsubfont *defont;

void	drawpjw(Memimage*, Fontchar*, int, int, int, int);
