extern int squawk;
extern int clean;
extern char *file;
extern int verbose;
extern long ninput, noutput, nrunes, nerrors;

enum { From = 1, Table = 2, Func = 4 };

typedef void (*Fnptr)(void);
struct convert{
	char *name;
	char *chatter;
	int flags;
	void *data;
	Fnptr fn;
};
extern struct convert convert[];
struct convert *conv(char *, int);
typedef void (*Infn)(int, long *, struct convert *);
typedef void (*Outfn)(Rune *, int, long *);
void outtable(Rune *, int, long *);

void utf_in(int, long *, struct convert *);
void utf_out(Rune *, int, long *);
void isoutf_in(int, long *, struct convert *);
void isoutf_out(Rune *, int, long *);

#define		N		10000		/* just blocking */
#define	OUT(out, r, n)	if(out->flags&Table) outtable(r, n, (long *)out->data);\
			else ((Outfn)(out->fn))(r, n, (long *)0)

extern Rune runes[N];
extern char obuf[UTFmax*N];	/* maximum bloat from N runes */

#define		BADMAP		(0xFFFD)
#define		BYTEBADMAP	('?')		/* badmap but has to fit in a byte */
#define		ESC		033

#ifdef	PLAN9
#define	EPR		fprint(2,
#define	EXIT(n,s)	exits(s)
#else
#define	EPR		fprintf(stderr,
#define	USED(x)		/* in plan 9, USED(x) tells the compiler to treat x as used */
#define	EXIT(n,s)	exit(n)
#endif
