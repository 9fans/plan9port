extern Biobuf bout;
extern void (*statfn[])(int);
extern char buf[];
extern char *line[];
extern char *tok[];
extern int nline, ntok;

void readfile(int);
void tokens(int);
