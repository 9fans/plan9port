static
signed char *
newStrFromInt(int n) {
	int digits, m = n;
	for (digits = 1; ;) {
		m -= m % 10;
		if (m == 0) {
			break;
		}
		digits++;
		m /= 10;
	}
	signed char *ret = malloc(digits*sizeof(*ret) + sizeof(""));
	for (; m != digits; m++) {
		ret[m] = n % 10 + '0';
		n /= 10;
	}
	ret[digits] = '\0';
	return ret;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size == 0) {
		return 0;
	}

	static int initialized;
	if (!initialized) {
		initialized = 0 == 0;
		extern char *PARSER;
		extern char *parser;
		parser = unsharp(PARSER);
	}

	extern int    setup(int, char **);
	extern int    cpres(void);
	extern int    cempty(void);
	extern int    cpfir(void);
	extern int    stagen(void);
	extern int    output(void);
	extern int    go2out(void);
	extern int    callopt(void);
	extern void   cleantmp(void);

	         /* I/O descriptors */

	extern Biobuf* faction;        /* file for saving actions */
	extern Biobuf* fdefine;        /* file for #defines */
	extern Biobuf* fdebug;         /* y.debug for strings for debugging */
	extern Biobuf* ftable;         /* y.tab.c file */
	extern Biobuf* ftemp;          /* tempfile to pass 2 */
	extern Biobuf* finput;         /* input file */
	extern Biobuf* foutput;        /* y.output file */

	int fDs[2];
	readerFilDesFromBuf(data, size, fDs);
	signed char *fD = newStrFromInt(fDs[0]);
	char *argv[5] = {"fuzzedYacc", "-o", "/dev/null", "-f", fD};
	if (setup(sizeof(argv) / sizeof(argv[0]), argv) || cpres() || cempty() || cpfir() || stagen() || output() || go2out() || callopt()) {
		if (faction != nil) {
			Bterm(faction);
		}
		if (fdefine != nil) {
			Bterm(fdefine);
		}
		if (fdebug != nil) {
			Bterm(fdebug);
		}
		if (ftable != nil) {
			Bterm(ftable);
		}
		if (ftemp != nil) {
			Bterm(ftemp);
		}
		if (finput != nil) {
			Bterm(finput);
		}
		if (foutput != nil) {
			Bterm(foutput);
		}
		cleantmp();
	}
	free(fD);
	closeSecondFD(fDs);

	// Copied from yacc.c!
	enum {
	/*
	 * the following are adjustable
	 * according to memory size
	 */
	        ACTSIZE         = 40000,
	        MEMSIZE         = 40000,
	        NSTATES         = 2000,
	        NTERMS          = 511,
	        NPROD           = 1600,
	        NNONTERM        = 600,
	        TEMPSIZE        = 2000,
	        CNAMSZ          = 10000,
	        LSETSIZE        = 2400,
	        WSETSIZE        = 350,
	
	        NAMESIZE        = 50,
	        NTYPES          = 63,
		ISIZE		= 400,
	
	        /* relationships which must hold:
	                TBITSET ints must hold NTERMS+1 bits...
	                WSETSIZE >= NNONTERM
	                LSETSIZE >= NNONTERM
	                TEMPSIZE >= NTERMS + NNONTERM + 1
	                TEMPSIZE >= NSTATES
	        */
	};
#define TBITSET		((32+NTERMS)/32)	/* BOTCH?? +31 */
	typedef struct {
		int	lset[TBITSET];
	} Lkset;
	
	typedef struct {
		int*	pitem;
		Lkset*	look;
	} Item;
	
	typedef struct {
		char*	name;
		int	value;
	} Symb;
	
	typedef struct {
		int*	pitem;
		int	flag;
		Lkset	ws;
	} Wset;

	// Re-initialize global variables.

	         /* I/O descriptors */

	faction = nil;
	fdefine = nil;
	fdebug = nil;
	ftable = nil;
	ftemp = nil;
	finput = nil;
	foutput = nil;

	        /* communication variables between various I/O routines */

	extern char*   infile;                 /* input file name */
	infile = nil;
	extern int     numbval;                /* value of an input number */
	numbval = 0;
	extern char    tokname[NAMESIZE+4];    /* input token name, slop for runes and 0 */
	memset(tokname, 0, sizeof(tokname));

	        /* storage of names */

	extern char    cnames[CNAMSZ];         /* place where token and nonterminal names are stored */
	memset(cnames, 0, sizeof(cnames));
	extern int     cnamsz;        /* size of cnames */
	cnamsz = CNAMSZ;
	extern char*   cnamp;         /* place where next name is to be put in */
	cnamp = cnames;
	extern int     ndefout;            /* number of defined symbols output */
	ndefout = 4;
	extern char*   tempname;
	tempname = nil;
	extern char*   actname;
	actname = nil;
#define TEMPNAME	"y.tmp.XXXXXX"
	extern char ttempname[sizeof(TEMPNAME)];
	memset(&ttempname[sizeof(TEMPNAME) - sizeof("") - 6], 'X', 6);
#define ACTNAME		"y.acts.XXXXXX"
	extern char tactname[sizeof(ACTNAME)];
	memset(&tactname[sizeof(ACTNAME) - sizeof("") - 6], 'X', 6);

	        /* storage of types */
	extern int     ntypes;                 /* number of types defined */
	ntypes = 0;
	extern char*   typeset[NTYPES];        /* pointers to type tags */
	memset(typeset, 0, sizeof(typeset));

	        /* token information */

	extern int     ntokens;           /* number of tokens */
	ntokens = 0;
	extern Symb    tokset[NTERMS];
	memset(tokset, 0, sizeof(tokset));
	extern int     toklev[NTERMS];         /* vector with the precedence of the terminals */
	memset(toklev, 0, sizeof(toklev));

	        /* nonterminal information */

	extern int     nnonter;           /* the number of nonterminals */
	nnonter = -1;
	extern Symb    nontrst[NNONTERM];
	memset(nontrst, 0, sizeof(nontrst));
	extern int     start;                  /* start symbol */
	start = 0;

	        /* assigned token type values */
	extern int     extval;
	extval = 0;

	        /* grammar rule information */

	extern int     mem0[MEMSIZE] ;         /* production storage */
	memset(mem0, 0, sizeof(mem0));
	extern int*    mem;
	mem = mem0;
	extern int     nprod;              /* number of productions */
	nprod = 1;
	extern int*    prdptr[NPROD];          /* pointers to descriptions of productions */
	memset(prdptr, 0, sizeof(prdptr));
	extern int     levprd[NPROD];          /* precedence levels for the productions */
	memset(levprd, 0, sizeof(levprd));
	extern int     rlines[NPROD];          /* line number for this rule */
	memset(rlines, 0, sizeof(rlines));

	        /* state information */

	extern int     nstate;             /* number of states */
	nstate = 0;
	extern Item*   pstate[NSTATES+2];      /* pointers to the descriptions of the states */
	memset(pstate, 0, sizeof(pstate));
	extern int     tystate[NSTATES];       /* contains type information about the states */
	memset(tystate, 0, sizeof(tystate));
	extern int     defact[NSTATES];        /* the default actions of states */
	memset(defact, 0, sizeof(defact));
	extern int     tstates[NTERMS];        /* states generated by terminal gotos */
	memset(tstates, 0, sizeof(tstates));
	extern int     ntstates[NNONTERM];     /* states generated by nonterminal gotos */
	memset(ntstates, 0, sizeof(ntstates));
	extern int     mstates[NSTATES];       /* chain of overflows of term/nonterm generation lists  */
	memset(mstates, 0, sizeof(mstates));
	extern int     lastred;                /* the number of the last reduction of a state */
	lastred = 0;

	        /* lookahead set information */

	extern Lkset   lkst[LSETSIZE];
	memset(lkst, 0, sizeof(lkst));
	extern int     nolook;                 /* flag to turn off lookahead computations */
	nolook = 0;
	extern int     nlset;              /* next lookahead set index */
	nlset = 0;
	extern int     nolook;             /* flag to suppress lookahead computations */
	nolook = 0;
	extern Lkset   clset;                  /* temporary storage for lookahead computations */
	memset(&clset, 0, sizeof(clset));

	        /* working set information */

	extern Wset    wsets[WSETSIZE];
	memset(wsets, 0, sizeof(wsets));
	extern Wset*   cwp;
	cwp = nil;

	        /* storage for action table */

	extern int     amem[ACTSIZE];          /* action table storage */
	memset(amem, 0, sizeof(amem));
	extern int*    memp;            /* next free action table position */
	memp = amem;
	extern int     indgo[NSTATES];         /* index to the stored goto table */
	memset(indgo, 0, sizeof(indgo));

	        /* temporary vector, indexable by states, terms, or ntokens */

	extern int     temp1[TEMPSIZE];        /* temporary storage, indexed by terms + ntokens or states */
	memset(temp1, 0, sizeof(temp1));
	extern int     lineno;             /* current input line number */
	lineno = 1;
	extern int     nerrors;            /* number of errors */
	nerrors = 0;

	        /* statistics collection variables */

	extern int     zzgoent;
	extern int     zzgobest;
	extern int     zzacent;
	extern int     zzexcp;
	extern int     zzclose;
	extern int     zzrrconf;
	extern int     zzsrconf;
	zzgoent = zzgobest = zzacent = zzexcp = zzclose = zzrrconf = zzsrconf = 0;

	extern int*    ggreed;
	ggreed = lkst[0].lset;
	extern int*    pgo;
	pgo = wsets[0].ws.lset;
	extern int*    yypgo;
	yypgo = &nontrst[0].value;

	extern int     maxspr;             /* maximum spread of any entry */
	extern int     maxoff;             /* maximum offset into a array */
	extern int*    pmem;
	extern int*    maxa;
	extern int     nxdb;
	extern int     adb;
	maxspr = maxoff = nxdb = adb = 0;
	pmem = mem0;
	maxa = nil;


	        /* storage for information about the nonterminals */

	extern int**   pres[NNONTERM+2];       /* vector of pointers to productions yielding each nonterminal */
	memset(pres, 0, sizeof(pres));
	extern Lkset*  pfirst[NNONTERM+2];     /* vector of pointers to first sets for each nonterminal */
	memset(pfirst, 0, sizeof(pfirst));
	extern int     pempty[NNONTERM+1];     /* vector of nonterminals nontrivially deriving e */
	memset(pempty, 0, sizeof(pempty));

	        /* random stuff picked out from between functions */

	extern int     indebug;
	extern Wset*   zzcwp;
	extern int     zzgoent;
	extern int     zzgobest;
	extern int     zzacent;
	extern int     zzexcp;
	extern int     zzclose;
	extern int     zzsrconf;
	extern int*    zzmemsz;
	extern int     zzrrconf;
	extern int     pidebug;            /* debugging flag for putitem */
	extern int     gsdebug;
	extern int     cldebug;            /* debugging flag for closure */
	extern int     pkdebug;
	extern int     g2debug;
	indebug = zzgoent = zzgobest = zzacent = zzexcp = zzclose = zzsrconf = zzrrconf = pidebug = gsdebug = cldebug = pkdebug = g2debug = 0;
	zzcwp = wsets;
	zzmemsz = mem0;

	extern char sarr[ISIZE];
	memset(sarr, 0, sizeof(sarr));
	extern int *pyield[NPROD];
	memset(pyield, 0, sizeof(pyield));
	extern int peekline;
	peekline = 0;

	return 0;
}
