#define	NONE	0
#define	WARNING	1
#define	FATAL	2

#define	RUNEGETGROUP(a)	((a>>8)&0xff)
#define	RUNEGETCHAR(a)	(a&0xff)

#define tempnam safe_tempnam

typedef	int	BOOLEAN;

#define	TRUE	1
#define	FALSE	0

#define NUMOFONTS 0x100
#define FONTSIZE 0x100

extern char *programname;
extern char *inputfilename;
extern int inputlineno;

extern int page_no;
extern int pages_printed;
extern int curpostfontid;
extern int hpos, vpos;

extern Biobuf *Bstdout, *Bstderr;

struct strtab {
	int size;
	char *str;
	int used;
};

extern struct strtab charcode[];
BOOLEAN pageon(void);
void startstring(void);
void endstring(void);
BOOLEAN isinstring(void);
void startpage(void);
void endpage(void);
int cat(char *);
int Bgetfield(Biobuf*, int, void *, int);
void *galloc(void *, int, char *);
void pagelist(char *);

int safe_tmpnam(char*);
