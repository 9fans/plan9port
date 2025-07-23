/* acid.h */
#undef OAPPEND

enum
{
	Eof		= -1,
	Strsize		= 65536,
	Hashsize	= 128,
	Maxarg		= 512,
	NFD		= 100,
	Maxproc		= 50,
	Maxval		= 10,
	Mempergc	= 1024*1024
};

/* #pragma varargck type "L"	void */

typedef struct Node	Node;
typedef struct String	String;
typedef struct Lsym	Lsym;
typedef struct List	List;
typedef struct Store	Store;
typedef struct Gc	Gc;
typedef struct Strc	Strc;
typedef struct Rplace	Rplace;
typedef struct Ptab	Ptab;
typedef struct Value	Value;
typedef struct Type	Type;
typedef struct Frtype	Frtype;

Extern int	kernel;
Extern int nlcount;
Extern int	remote;
Extern int	text;
Extern int cor;
Extern int	silent;
Extern Fhdr	*fhdr;
Extern Fhdr	*chdr;
Extern int	line;
Extern Biobuf*	bout;
Extern Biobuf*	io[32];
Extern int	iop;
Extern int pid;
Extern char	symbol[Strsize];
Extern int	interactive;
Extern Node*	code;
Extern int	na;
Extern int	wtflag;
Extern Regs*	acidregs;
extern Regs*	correg;
extern Map*	cormap;
extern Map*	symmap;
Extern Lsym*	hash[Hashsize];
Extern long	dogc;
Extern Rplace*	ret;
extern char*	symfil;
extern char*	corfil;
Extern int	gotint;
Extern long	flen;
Extern Gc*	gcl;
Extern int	stacked;
#define err aciderrjmp
Extern jmp_buf	err;
Extern Node*	prnt;
Extern Node*	fomt;
Extern List*	tracelist;
Extern int	initialising;
Extern int	quiet;
extern Fhdr*	corhdr;
extern Fhdr*	symhdr;

extern void	(*expop[])(Node*, Node*);
#define expr(n, r) (r)->store.comt=0; (*expop[(unsigned char)((n)->op)])(n, r);

enum
{
	TINT,
	TFLOAT,
	TSTRING,
	TLIST,
	TCODE,
	TREG,
	TCON,
	NUMT
};

struct Type
{
	Type*	next;
	int	offset;
	char	fmt;
	char	depth;
	Lsym*	type;
	Lsym*	tag;
	Lsym*	base;
};

struct Frtype
{
	Lsym*	var;
	Type*	type;
	Frtype*	next;
};

struct Ptab
{
	int	pid;
/*	int	ctl; */
};
Extern Ptab	ptab[Maxproc];

struct Rplace
{
	jmp_buf	rlab;
	Node*	stak;
	Node*	val;
	Lsym*	local;
	Lsym**	tail;
};

struct Gc
{
	char	gcmark;
	Gc*	gclink;
};

struct Store
{
	char	fmt;
	Type*	comt;
	union {
		vlong	ival;
		double	fval;
		String*	string;
		List*	l;
		Node*	cc;
		struct {
			char *name;
			uint	thread;
		} reg;
		Node*	con;
	} u;
};

struct List
{
	Gc gc;
	List*	next;
	char	type;
	Store store;
};

struct Value
{
	char	set;
	char	type;
	Store store;
	Value*	pop;
	Lsym*	scope;
	Rplace*	ret;
};

struct Lsym
{
	char*	name;
	int	lexval;
	Lsym*	hash;
	Value*	v;
	Type*	lt;
	Node*	proc;
	Frtype*	local;
	void	(*builtin)(Node*, Node*);
};

struct Node
{
	Gc gc;
	char	op;
	char	type;
	Node*	left;
	Node*	right;
	Lsym*	sym;
	int	builtin;
	Store store;
};
#define ZN	(Node*)0

struct String
{
	Gc gc;
	char	*string;
	int	len;
};

int	acidregsrw(Regs*, char*, u64int*, int);
List*	addlist(List*, List*);
void	addvarsym(Fhdr*);
List*	al(int);
Node*	an(int, Node*, Node*);
void	append(Node*, Node*, Node*);
int	bool(Node*);
void	build(Node*);
void	call(char*, Node*, Node*, Node*, Node*);
void	catcher(void*, char*);
void	checkqid(int, int);
void	cmd(void);
Node*	con(s64int);
List*	construct(Node*);
void	ctrace(int);
void	decl(Node*);
void	defcomplex(Node*, Node*);
void	deinstall(int);
void	delete(List*, int n, Node*);
void	delvarsym(char*);
void	dostop(int);
Lsym*	enter(char*, int);
void	error(char*, ...);
void	execute(Node*);
void	fatal(char*, ...);
u64int	findframe(u64int);
void	flatten(Node**, Node*);
void	gc(void);
char*	getstatus(int);
void*	gmalloc(long);
void	indir(Map*, u64int, char, Node*);
void	indirreg(Regs*, char*, char, Node*);
void	initexpr(void);
void	initprint(void);
void	installbuiltin(void);
void	kinit(void);
int	Zfmt(Fmt*);
int	listcmp(List*, List*);
int	listlen(List*);
List*	listvar(char*, long);
void	loadmodule(char*);
void	loadvars(void);
Lsym*	look(char*);
void	ltag(char*);
void	marklist(List*);
Lsym*	mkvar(char*);
void	msg(int, char*);
void	notes(int);
int	nproc(char**);
void	nthelem(List*, int, Node*);
int	numsym(char);
void	odot(Node*, Node*);
void	pcode(Node*, int);
void	pexpr(Node*);
int	popio(void);
void	pstr(String*);
void	pushfd(int);
void	pushfile(char*);
void	pushstr(Node*);
u64int	raddr(char*);
void	readtext(char*);
void	readcore(void);
void	restartio(void);
String	*runenode(Rune*);
int	scmp(String*, String*);
void	sproc(int);
String*	stradd(String*, String*);
String*	strnode(char*);
String*	strnodlen(char*, int);
#define system acidsystem
char*	system(void);
int	trlist(Map*, Regs*, u64int, u64int, Symbol*, int);
void	unwind(void);
void	userinit(void);
void	varreg(void);
void	varsym(void);
void	whatis(Lsym*);
void	windir(Map*, Node, Node*, Node*);
void	windirreg(Regs*, char*, Node*, Node*);
void	yyerror(char*, ...);
int	yylex(void);
int	yyparse(void);

enum
{
	ONAME,
	OCONST,
	OMUL,
	ODIV,
	OMOD,
	OADD,
	OSUB,
	ORSH,
	OLSH,
	OLT,
	OGT,
	OLEQ,
	OGEQ,
	OEQ,
	ONEQ,
	OLAND,
	OXOR,
	OLOR,
	OCAND,
	OCOR,
	OASGN,
	OINDM,
	OEDEC,
	OEINC,
	OPINC,
	OPDEC,
	ONOT,
	OIF,
	ODO,
	OLIST,
	OCALL,
	OCTRUCT,
	OWHILE,
	OELSE,
	OHEAD,
	OTAIL,
	OAPPEND,
	ORET,
	OINDEX,
	OINDC,
	ODOT,
	OLOCAL,
	OFRAME,
	OCOMPLEX,
	ODELETE,
	OCAST,
	OFMT,
	OEVAL,
	OWHAT,
	OUPLUS,
	NUMO
};
