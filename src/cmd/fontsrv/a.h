typedef struct XFont XFont;
XFont *xfont;
int nxfont;

enum {
	SubfontSize = 32,
	MaxSubfont = (Runemax+1)/SubfontSize,
};

struct XFont
{
	char *name;
	int loaded;
	uchar range[MaxSubfont]; // range[i] = fontfile starting at i*SubfontSize exists
	ushort file[MaxSubfont];	// file[i] == fontfile i's lo rune / SubfontSize
	int nfile;
	int unit;
	double height;
	double originy;
	void (*loadheight)(XFont*, int, int*, int*);
	char *fonttext;
	int nfonttext;

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
