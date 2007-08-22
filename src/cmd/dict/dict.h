/* Runes for special purposes (0xe800-0xfdff is Private Use Area) */
enum {	NONE=0xe800,	/* Emit nothing */
	TAGS,		/* Start of tag */
	TAGE,		/* End of tag */
	SPCS,		/* Start of special character name */
	PAR,		/* Newline, indent */
	LIGS,		/* Start of ligature codes */

/* need to keep in sync with utils.c:/ligtab */
	LACU=LIGS,	/* Acute (´) ligatures */
	LGRV,		/* Grave (ˋ) ligatures */
	LUML,		/* Umlaut (¨) ligatures */
	LCED,		/* Cedilla (¸) ligatures */
	LTIL,		/* Tilde (˜) ligatures */
	LBRV,		/* Breve (˘) ligatures */
	LRNG,		/* Ring (˚) ligatures */
	LDOT,		/* Dot (˙) ligatures */
	LDTB,		/* Dot below (.) ligatures */
	LFRN,		/* Frown (⌢) ligatures */
	LFRB,		/* Frown below (̯) ligatures */
	LOGO,		/* Ogonek (˛) ligatures */
	LMAC,		/* Macron (¯) ligatures */
	LHCK,		/* Hacek (ˇ) ligatures */
	LASP,		/* Asper (ʽ) ligatures */
	LLEN,		/* Lenis (ʼ) ligatures */
	LBRB,		/* Breve below (̮) ligatures */
	LIGE,		/* End of ligature codes */

/* need to keep in sync with utils.c:/multitab */
	MULTI,		/* Start of multi-rune codes */
	MAAS=MULTI,	/* ʽα */
	MALN,		/* ʼα */
	MAND,		/* and */
	MAOQ,		/* a/q */
	MBRA,		/* <| */
	MDD,		/* .. */
	MDDD,		/* ... */
	MEAS,		/* ʽε */
	MELN,		/* ʼε */
	MEMM,		/* —— */
	MHAS,		/* ʽη */
	MHLN,		/* ʼη */
	MIAS,		/* ʽι */
	MILN,		/* ʼι */
	MLCT,		/* ct */
	MLFF,		/* ff */
	MLFFI,		/* ffi */
	MLFFL,		/* ffl */
	MLFL,		/* fl */
	MLFI,		/* fi */
	MLLS,		/* ll with swing */
	MLST,		/* st */
	MOAS,		/* ʽο */
	MOLN,		/* ʼο */
	MOR,		/* or */
	MRAS,		/* ʽρ */
	MRLN,		/* ʼρ */
	MTT,		/* ~~ */
	MUAS,		/* ʽυ */
	MULN,		/* ʼυ */
	MWAS,		/* ʽω */
	MWLN,		/* ʼω */
	MOE,		/* oe */
	MES,		/* em space */
	MULTIE		/* End of multi-rune codes */
};
#define Nligs (LIGE-LIGS)
#define Nmulti (MULTIE-MULTI)

typedef struct Entry Entry;
typedef struct Assoc Assoc;
typedef struct Nassoc Nassoc;
typedef struct Dict Dict;

struct Entry {
	char	*start;		/* entry starts at start */
	char	*end;		/* and finishes just before end */
	long	doff;		/* dictionary offset (for debugging) */
};

struct Assoc {
	char	*key;
	long	val;
};

struct Nassoc {
	long	key;
	long	val;
};

struct Dict {
	char	*name;			/* dictionary name */
	char	*desc;			/* description */
	char	*path;			/* path to dictionary data */
	char	*indexpath;		/* path to index data */
	long	(*nextoff)(long);	/* function to find next entry offset from arg */
	void	(*printentry)(Entry, int); /* function to print entry */
	void	(*printkey)(void);	/* function to print pronunciation key */
};

int	acomp(Rune*, Rune*);
Rune	*changett(Rune *, Rune *, int);
void	err(char*, ...);
void	fold(Rune *);
void	foldre(char*, char*);
Rune	liglookup(Rune, Rune);
long	lookassoc(Assoc*, int, char*);
long	looknassoc(Nassoc*, int, long);
void	outprint(char*, ...);
void	outrune(long);
void	outrunes(Rune *);
void	outchar(int);
void	outchars(char *);
void	outnl(int);
void	outpiece(char *, char *);
void	runescpy(Rune*, Rune*);
long	runetol(Rune*);
char	*dictfile(char*);

long	oednextoff(long);
void	oedprintentry(Entry, int);
void	oedprintkey(void);
long	ahdnextoff(long);
void	ahdprintentry(Entry, int);
void	ahdprintkey(void);
long	pcollnextoff(long);
void	pcollprintentry(Entry, int);
void	pcollprintkey(void);
long	pcollgnextoff(long);
void	pcollgprintentry(Entry, int);
void	pcollgprintkey(void);
long	movienextoff(long);
void	movieprintentry(Entry, int);
void	movieprintkey(void);
long	pgwnextoff(long);
void	pgwprintentry(Entry,int);
void	pgwprintkey(void);
void	rogetprintentry(Entry, int);
long	rogetnextoff(long);
void	rogetprintkey(void);
long	slangnextoff(long);
void	slangprintentry(Entry, int);
void	slangprintkey(void);
long	robertnextoff(long);
void	robertindexentry(Entry, int);
void	robertprintkey(void);
long	robertnextflex(long);
void	robertflexentry(Entry, int);
long	simplenextoff(long);
void	simpleprintentry(Entry, int);
void	simpleprintkey(void);
long	thesnextoff(long);
void	thesprintentry(Entry, int);
void	thesprintkey(void);
long	worldnextoff(long);
void	worldprintentry(Entry, int);
void	worldprintkey(void);

extern Biobuf	*bdict;
extern Biobuf	*bout;
extern int	linelen;
extern int	breaklen;
extern int	outinhibit;
extern int	debug;
extern Rune	multitab[][5];
extern Dict	dicts[];

#define asize(a) (sizeof (a)/sizeof(a[0]))
