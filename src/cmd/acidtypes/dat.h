typedef struct Type Type;
typedef struct Typeref Typeref;
typedef struct TypeList TypeList;

enum
{
	None,
	Base,
	Enum,
	Aggr,
	Function,	
	Pointer,
	Array,
	Range,
	Defer,
	Typedef,
};

struct Type
{	/* Font Tab 4 */
	uint	ty;			/* None, Struct, ... */
	vlong lo;			/* for range */
	char sue;
	vlong hi;
	uint	gen;
	uint	n1;		/* type number (impl dependent) */
	uint	n2;		/* another type number */
	char	*name;		/* name of type */
	char	*suename;	/* name of struct, union, enumeration */
	uint	isunion;	/* is this Struct a union? */
	uint	printfmt;	/* describes base type */
	uint	xsizeof;	/* size of type */
	Type	*sub;		/* subtype */
	uint	n;			/* count for t, tname, val */
	Type **t;			/* members of sue, params of function */
	char	**tname;	/* associated names */
	long	*val;		/* associated offsets or values */
	uint	didtypedef;	/* internal flag */
	uint	didrange;		/* internal flag */
	uint 	printed;		/* internal flag */
	Type *equiv;		/* internal */
};

struct TypeList
{
	Type *hd;
	TypeList *tl;
};

void *erealloc(void*, uint);
void *emalloc(uint);
char *estrdup(char*);
void warn(char*, ...);

Type *typebynum(uint n1, uint n2);
Type *typebysue(char, char*);
void printtypes(Biobuf*);
void renumber(TypeList*, uint);
void denumber(void);
TypeList *mktl(Type*, TypeList*);

struct Dwarf;
struct Stab;
int dwarf2acid(struct Dwarf*, Biobuf*);
int stabs2acid(struct Stab*, Biobuf*);

Type *newtype(void);
char *nameof(Type*, int);
void freetypes(void);

extern char *prefix;
char *fixname(char*);
