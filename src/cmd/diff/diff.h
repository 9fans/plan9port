#define stdout bstdout

char mode;			/* '\0', 'e', 'f', 'h' */
char bflag;			/* ignore multiple and trailing blanks */
char rflag;			/* recurse down directory trees */
char mflag;			/* pseudo flag: doing multiple files, one dir */
int anychange;
extern Biobuf	stdout;
extern int	binary;

#define MALLOC(t, n)		((t *)emalloc((n)*sizeof(t)))
#define REALLOC(p, t, n)	((t *)erealloc((void *)(p), (n)*sizeof(t)))
#define FREE(p)			free((void *)(p))

#define MAXPATHLEN	1024

int mkpathname(char *, char *, char *);
void *emalloc(unsigned);
void *erealloc(void *, unsigned);
void diff(char *, char *, int);
void diffdir(char *, char *, int);
void diffreg(char *, char *);
Biobuf *prepare(int, char *);
void panic(int, char *, ...);
void check(Biobuf *, Biobuf *);
void change(int, int, int, int);
void flushchanges(void);
