#define	NFLAG	128
#define	NCMDLINE	512
#define	NGETFLAGSARGV	256
extern char **flag[NFLAG];
extern char cmdline[NCMDLINE+1];
extern char *cmdname;
extern char *flagset[];
extern char *getflagsargv[NGETFLAGSARGV+2];
int getflags(int, char *[], char *);
void usage(char *);
