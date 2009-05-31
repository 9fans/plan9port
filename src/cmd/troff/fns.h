#define getline p9getline

/*
 * other
 */
#ifdef NOTDEF
int	pclose(FILE*);
long	filesize(int fd);
int	open(char *, int);
int	read(int, char *, int);
int	lseek(int, long, int);
int	close(int);
int	getpid(void);
#endif
char	*unsharp(char*);

/*
 * c1.c
 */
void	init0(void);
void	init2(void);
void	cvtime(void);
void	errprint(void);
int	control(int a, int b);
void	casept(void);
int	getrq(void);
Tchar	getch(void);
void	setxon(void);
Tchar	getch0(void);
Tchar	get1ch(FILE *);
void	pushback(Tchar *b);
void	cpushback(char *b);
int	nextfile(void);
int	popf(void);
void	flushi(void);
int	getach(void);
void	casenx(void);
int	getname(void);
void	caseso(void);
void	caself(void);
void	casecf(void);
void	getline(char *s, int n);
void	casesy(void);
void	getpn(char *a);
void	setrpt(void);

/*
 * n2.c
 */
int	pchar(Tchar i);
void	pchar1(Tchar i);
int	pchar2(Tchar i);
int	flusho(void);
void	casedone(void);
void	caseex(void);
void	done(int x);
void	done1(int x);
void	done2(int x);
void	done3(int x);
void	edone(int x);
void	casepi(void);

/*
 * c3.c
 */
void	blockinit(void);
char*	grow(char *, int, int);
void	mnspace(void);
void	caseig(void);
void	casern(void);
void	maddhash(Contab *rp);
void	munhash(Contab *mp);
void	mrehash(void);
void	caserm(void);
void	caseas(void);
void	caseds(void);
void	caseam(void);
void	casede(void);
int	findmn(int i);
void	clrmn(int i);
Offset	finds(int mn);
int	skip(void);
int	copyb(void);
void	copys(void);
Offset	alloc(void);
void	ffree(Offset i);
void	wbf(Tchar i);
Tchar	rbf(void);
Tchar	popi(void);
Offset	pushi(Offset newip, int mname);
void*	setbrk(int x);
int	getsn(void);
Offset	setstr(void);
void	collect(void);
void	seta(void);
void	caseda(void);
void	casegd(void);
void	casedi(void);
void	casedt(void);
void	casetl(void);
void	casepc(void);
void	casepm(void);
void	stackdump(void);

/*
 * c4.c
 */
void	setn(void);
int	wrc(Tchar i);
void	setn1(int i, int form, Tchar bits);
void	nnspace(void);
void	nrehash(void);
void	nunhash(Numtab *rp);
int	findr(int i);
int	usedr(int i);
int	fnumb(int i, int (*f)(Tchar));
int	decml(int i, int (*f)(Tchar));
int	roman(int i, int (*f)(Tchar));
int	roman0(int i, int (*f)(Tchar), char *onesp, char *fivesp);
int	abc(int i, int (*f)(Tchar));
int	abc0(int i, int (*f)(Tchar));
long	atoi0(void);
long	ckph(void);
long	atoi1(Tchar ii);
void	caserr(void);
void	casenr(void);
void	caseaf(void);
void	setaf(void);
int	vnumb(int *i);
int	hnumb(int *i);
int	inumb(int *n);
int	quant(int n, int m);

/*
 * c5.c
 */
void	casead(void);
void	casena(void);
void	casefi(void);
void	casenf(void);
void	casers(void);
void	casens(void);
int	chget(int c);
void	casecc(void);
void	casec2(void);
void	casehc(void);
void	casetc(void);
void	caselc(void);
void	casehy(void);
int	max(int aa, int bb);
void	casenh(void);
void	casece(void);
void	casein(void);
void	casell(void);
void	caselt(void);
void	caseti(void);
void	casels(void);
void	casepo(void);
void	casepl(void);
void	casewh(void);
void	casech(void);
int	findn(int i);
void	casepn(void);
void	casebp(void);
void	casextm(void);
void	casetm(void);
void	casefm(void);
void	casetm1(int ab, FILE *out);
void	casesp(void);
void	casesp1(int a);
void	casert(void);
void	caseem(void);
void	casefl(void);
void	caseev(void);
void	envcopy(Env *e1, Env *e2);
void	caseel(void);
void	caseie(void);
void	casexif(void);
void	caseif(void);
void	caseif1(int);
void	eatblk(int inblk);
int	cmpstr(Tchar c);
void	caserd(void);
int	rdtty(void);
void	caseec(void);
void	caseeo(void);
void	caseta(void);
void	casene(void);
void	casetr(void);
void	casecu(void);
void	caseul(void);
void	caseuf(void);
void	caseit(void);
void	casemc(void);
void	casemk(void);
void	casesv(void);
void	caseos(void);
void	casenm(void);
void	getnm(int *p, int min);
void	casenn(void);
void	caseab(void);
void	save_tty(void);
void	restore_tty(void);
void	set_tty(void);
void	echo_off(void);
void	echo_on(void);

/*
 * t6.c
 */
int	t_width(Tchar j);
void	zapwcache(int s);
int	onfont(int n, int f);
int	getcw(int i);
void	xbits(Tchar i, int bitf);
Tchar	t_setch(int c);
Tchar	t_setabs(void);
int	t_findft(int i);
void	caseps(void);
void	casps1(int i);
int	findps(int i);
void	t_mchbits(void);
void	t_setps(void);
Tchar	t_setht(void);
Tchar	t_setslant(void);
void	caseft(void);
void	t_setfont(int a);
void	t_setwd(void);
Tchar	t_vmot(void);
Tchar	t_hmot(void);
Tchar	t_mot(void);
Tchar	t_sethl(int k);
Tchar	t_makem(int i);
Tchar	getlg(Tchar i);
void	caselg(void);
void	casefp(void);
char	*strdupl(const char *);
int	setfp(int pos, int f, char *truename, int print);
void	casecs(void);
void	casebd(void);
void	casevs(void);
void	casess(void);
Tchar	t_xlss(void);
Uchar*	unpair(int i);
void	outascii(Tchar i);

/*
 * c7.c
 */
void	tbreak(void);
void	donum(void);
void	text(void);
void	nofill(void);
void	callsp(void);
void	ckul(void);
void	storeline(Tchar c, int w);
void	newline(int a);
int	findn1(int a);
void	chkpn(void);
int	findt(int a);
int	findt1(void);
void	eject(Stack *a);
int	movword(void);
void	horiz(int i);
void	setnel(void);
int	getword(int x);
void	storeword(Tchar c, int w);
Tchar	gettch(void);

/*
 * c8.c
 */
void	hyphen(Tchar *wp);
int	punct(Tchar i);
int	alph(int i);
void	caseha(void);
void	caseht(void);
void	casehw(void);
int	exword(void);
int	suffix(void);
int	maplow(int i);
int	vowel(int i);
Tchar*	chkvow(Tchar *w);
void	digram(void);
int	dilook(int a, int b, char t[26][13]);

/*
 * c9.c
 */
Tchar	setz(void);
void	setline(void);
int	eat(int c);
void	setov(void);
void	setbra(void);
void	setvline(void);
void	setdraw(void);
void	casefc(void);
Tchar	setfield(int x);

/*
 * t10.c
 */
void	t_ptinit(void);
void	t_specnames(void);
void	t_ptout(Tchar i);
int	ptout0(Tchar *pi);
void	ptchname(int);
void	ptflush(void);
void	ptps(void);
void	ptfont(void);
void	ptfpcmd(int f, char *s, char *fn);
void	t_ptlead(void);
void	ptesc(void);
void	ptpage(int n);
void	pttrailer(void);
void	ptstop(void);
void	t_ptpause(void);

/*
 * t11.c
 */
int	getdesc(char *name);
int	getfont(char *name, int pos);
int	chadd(char *s, int, int);
char*	chname(int n);
int	getlig(FILE *fin);

/*
 * n6.c
 */
int	n_width(Tchar j);
Tchar	n_setch(int c);
Tchar	n_setabs(void);
int	n_findft(int i);
void	n_mchbits(void);
void	n_setps(void);
Tchar	n_setht(void);
Tchar	n_setslant(void);
void	n_caseft(void);
void	n_setfont(int a);
void	n_setwd(void);
Tchar	n_vmot(void);
Tchar	n_hmot(void);
Tchar	n_mot(void);
Tchar	n_sethl(int k);
Tchar	n_makem(int i);
void	n_casefp(void);
void	n_casebd(void);
void	n_casevs(void);
Tchar	n_xlss(void);

/*
 * n10.c
 */
void	n_ptinit(void);
char*	skipstr(char *s);
char*	getstr(char *s, char *t);
char*	getint(char *s, int *pn);
void	twdone(void);
void	n_specnames(void);
int	findch(char *s);
void	n_ptout(Tchar i);
void	ptout1(void);
char*	plot(char *x);
void	move(void);
void	n_ptlead(void);
void	n_ptpause(void);

/*
 * indirect calls on TROFF/!TROFF.  these are variables!
 */
extern Tchar	(*hmot)(void);
extern Tchar	(*makem)(int i);
extern Tchar	(*setabs)(void);
extern Tchar	(*setch)(int c);
extern Tchar	(*sethl)(int k);
extern Tchar	(*setht)(void);
extern Tchar	(*setslant)(void);
extern Tchar	(*vmot)(void);
extern Tchar	(*xlss)(void);
extern int	(*findft)(int i);
extern int	(*width)(Tchar j);
extern void	(*mchbits)(void);
extern void	(*ptlead)(void);
extern void	(*ptout)(Tchar i);
extern void	(*ptpause)(void);
extern void	(*setfont)(int a);
extern void	(*setps)(void);
extern void	(*setwd)(void);
