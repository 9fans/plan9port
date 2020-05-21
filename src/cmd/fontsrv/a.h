typedef struct XFont XFont;
XFont *xfont;
int nxfont;

enum {
	SubfontSize = 128,
	MaxSubfont = (Runemax+1)/SubfontSize-1,
};

struct XFont
{
	char *name;
	int loaded;
	uchar range[(Runemax+1)/SubfontSize];	// range[i] == whether to have subfont i*SubfontSize to (i+1)*SubfontSize - 1.
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

void	drawpjw(Memimage*, Fontchar*, int, int, int, int);
