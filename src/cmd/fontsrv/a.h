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
};

void	loadfonts(void);
void	load(XFont*);
Memsubfont*	mksubfont(char*, int, int, int, int);

extern XFont *xfont;
extern int nxfont;
void *emalloc9p(ulong);
extern Memsubfont *defont;
