typedef struct Exec Exec;
typedef struct Rule Rule;
typedef struct Ruleset Ruleset;

/*
 * Object
 */
enum
{
	OArg,
	OAttr,
	OData,
	ODst,
	OPlumb,
	OSrc,
	OType,
	OWdir
};

/*
 * Verbs
 */
enum
{
	VAdd,	/* apply to OAttr only */
	VClient,
	VDelete,	/* apply to OAttr only */
	VIs,
	VIsdir,
	VIsfile,
	VMatches,
	VSet,
	VStart,
	VTo
};

struct Rule
{
	int	obj;
	int	verb;
	char	*arg;		/* unparsed string of all arguments */
	char	*qarg;	/* quote-processed arg string */
	Reprog	*regex;
};

struct Ruleset
{
	int	npat;
	int	nact;
	Rule	**pat;
	Rule	**act;
	char	*port;
};

enum
{
	NMATCHSUBEXP = 100	/* bounded by ../../libregexp/regcomp.h:/NSUBEXP */
};

struct Exec
{
	Plumbmsg	*msg;
	char			*match[NMATCHSUBEXP];
	int			p0;		/* begin and end of match */
	int			p1;
	int			clearclick;	/* click was expanded; remove attribute */
	int			setdata;	/* data should be set to $0 */
	int			holdforclient;	/* exec'ing client; keep message until port is opened */
	/* values of $variables */
	char			*file;
	char 			*dir;
};

void		parseerror(char*, ...);
void		error(char*, ...);
void*	emalloc(long);
void*	erealloc(void*, long);
char*	estrdup(char*);
Ruleset**	readrules(char*, int);
void		startfsys(int);
Exec*	matchruleset(Plumbmsg*, Ruleset*);
void		freeexec(Exec*);
char*	startup(Ruleset*, Exec*);
char*	printrules(void);
void		addport(char*);
char*	writerules(char*, int);
char*	expand(Exec*, char*, char**);
void		makeports(Ruleset*[]);
void		printinputstack(void);
int		popinput(void);

extern Ruleset	**rules;
extern char	*user;
extern char	*home;
extern jmp_buf	parsejmp;
extern char	*lasterror;
extern char	**ports;
extern int	nports;
extern int	debug;
