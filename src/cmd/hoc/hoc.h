typedef void (*Inst)(void);
#define	STOP	(Inst) 0

typedef struct Symbol	Symbol;
typedef union Datum 	Datum;
typedef struct Formal	Formal;
typedef struct Saveval	Saveval;
typedef struct Fndefn	Fndefn;
typedef union Symval	Symval;

union Symval { /* value of a symbol */
	double	val;		/* VAR */
	double	(*ptr)(double);	/* BLTIN */
	Fndefn	*defn;		/* FUNCTION, PROCEDURE */
	char	*str;		/* STRING */
};

struct Symbol {	/* symbol table entry */
	char	*name;
	long	type;
	Symval u;
	struct Symbol	*next;	/* to link to another */
};
Symbol	*install(char*, int, double), *lookup(char*);

union Datum {	/* interpreter stack type */
	double	val;
	Symbol	*sym;
};

struct Saveval {	/* saved value of variable */
	Symval	val;
	long		type;
	Saveval	*next;
};

struct Formal {	/* formal parameter */
	Symbol	*sym;
	Saveval	*save;
	Formal	*next;
};

struct Fndefn {	/* formal parameter */
	Inst	*code;
	Formal	*formals;
	int	nargs;
};

extern	Formal *formallist(Symbol*, Formal*);
extern	double Fgetd(int);
extern	int moreinput(void);
extern	void restore(Symbol*);
extern	void restoreall(void);
extern	void execerror(char*, char*);
extern	void define(Symbol*, Formal*), verify(Symbol*);
extern	Datum pop(void);
extern	void initcode(void), push(Datum), xpop(void), constpush(void);
extern	void varpush(void);
#define div hocdiv
extern	void eval(void), add(void), sub(void), mul(void), div(void), mod(void);
extern	void negate(void), power(void);
extern	void addeq(void), subeq(void), muleq(void), diveq(void), modeq(void);

extern	Inst *progp, *progbase, prog[], *code(Inst);
extern	void assign(void), bltin(void), varread(void);
extern	void prexpr(void), prstr(void);
extern	void gt(void), lt(void), eq(void), ge(void), le(void), ne(void);
extern	void and(void), or(void), not(void);
extern	void ifcode(void), whilecode(void), forcode(void);
extern	void call(void), arg(void), argassign(void);
extern	void funcret(void), procret(void);
extern	void preinc(void), predec(void), postinc(void), postdec(void);
extern	void execute(Inst*);
extern	void printtop(void);

extern double	Log(double), Log10(double), Gamma(double), Sqrt(double), Exp(double);
extern double	Asin(double), Acos(double), Sinh(double), Cosh(double), integer(double);
extern double	Pow(double, double);

extern	void init(void);
extern	int yyparse(void);
extern	void execerror(char*, char*);
extern	void *emalloc(unsigned);
