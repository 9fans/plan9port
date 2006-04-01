typedef struct AsciiInt AsciiInt;

struct AsciiInt {
	char*	key;
	int	val;
};

enum {
	Ltab2space,
	Lspace,
	Lempty,
	Lhash,
	Lplus,
	Lcommaspace,
	Lminus,
	Larrow,
	Lone,
	Llt,
	Lgt,
	Lquestion,
	Lindex,
	Lreset,
	Lsubmit,
	Lnot0to9,
	Lisindex,
	L_blank,
	Lfr,
	Lnoname,
	L_parent,
	L_self,
	L_top,
	Lappl_form,
	Lcircle,
	Lcm,
	Lcontent,
	Ldisc,
	Lem,
	Lin,
	Ljavascript,
	Ljscript,
	Ljscript1,
	Lmm,
	Lnone,
	Lpi,
	Lpt,
	Lrefresh,
	Lselect,
	Lsquare,
	Ltextarea
};

#define L(x)	runeconsttab[(x)]

extern	Rune	**runeconsttab;

/* XXX: for unix port only */
Rune		**_cvtstringtab(char**, int);
StringInt	*_cvtstringinttab(AsciiInt*, int);
void		_runetabinit(void);
