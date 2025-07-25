#define	NFLAG	128

extern char **flag[NFLAG];
extern char *cmdname;
extern char *flagset[];

int getflags(int, char*[], char*, int);
