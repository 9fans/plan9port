typedef struct Node Node;
typedef struct Field Field;
typedef Node *Nodeptr;
#define YYSTYPE Nodeptr

struct Node {
	Node	*next;
	int	c;	/* token type */
	char	addr;	/* true if this is an address */
	String	*s;	/* string representing token */
	String	*white;	/* white space following token */
	char	*start;	/* first byte for this token */
	char	*end;	/* next byte in input */
};

struct Field {
	Field	*next;
	Node	*node;
	int	source;
};

typedef struct DS	DS;
struct DS {
	/* dist string */
	char	buf[128];
	char	expand[128];
	char	*netdir;
	char	*proto;
	char	*host;
	char	*service;
};

extern Field	*firstfield;
extern Field	*lastfield;
extern Node	*usender;
extern Node	*usys;
extern Node	*udate;
extern int	originator;
extern int	destination;
extern int	date;
extern int	messageid;

Node*	anonymous(Node*);
Node*	address(Node*);
int	badfieldname(Node*);
Node*	bang(Node*, Node*);
Node*	colon(Node*, Node*);
int	cistrcmp(char*, char*);
Node*	link2(Node*, Node*);
Node*	link3(Node*, Node*, Node*);
void	freenode(Node*);
void	newfield(Node*, int);
void	freefield(Field*);
void	yyinit(char*, int);
int	yyparse(void);
int	yylex(void);
String*	yywhite(void);
Node*	whiten(Node*);
void	yycleanup(void);
int	mxdial(char*, char*, char*);
void	dial_string_parse(char*, DS*);
