/* t..c : external declarations */

#include <u.h>
#include <libc.h>
#include <bio.h>
# include <ctype.h>

# define MAXLIN 250
# define MAXHEAD 44
# define MAXCOL 30
 /* Do NOT make MAXCOL bigger with adjusting nregs[] in tr.c */
# define MAXCHS 2000
#define MAXLINLEN 300
# define MAXRPT 100
# define CLLEN 10
# define SHORTLINE 4
extern int nlin, ncol, iline, nclin, nslin;

extern int (*style)[MAXHEAD];
extern char (*font)[MAXHEAD][2];
extern char (*csize)[MAXHEAD][4];
extern char (*vsize)[MAXHEAD][4];
extern char (*cll)[CLLEN];
extern int (*flags)[MAXHEAD];
# define ZEROW 001
# define HALFUP 002
# define CTOP 004
# define CDOWN 010
extern int stynum[];
extern int qcol;
extern int *doubled, *acase, *topat;
extern int F1, F2;
extern int (*lefline)[MAXHEAD];
extern int fullbot[];
extern char *instead[];
extern int expflg;
extern int ctrflg;
extern int evenflg;
extern int *evenup;
extern int boxflg;
extern int dboxflg;
extern int linsize;
extern int tab;
extern int pr1403;
extern int linsize, delim1, delim2;
extern int allflg;
extern int textflg;
extern int left1flg;
extern int rightl;
struct colstr {char *col, *rcol;};
extern struct colstr *table[];
extern char *cspace, *cstore;
extern char *exstore, *exlim, *exspace;
extern int *sep;
extern int *used, *lused, *rused;
extern int linestop[];
extern char *leftover;
extern char *last, *ifile;
extern int texname;
extern int texct, texmax;
extern char texstr[];
extern int linstart;


extern Biobuf *tabin, tabout;
# define CRIGHT 2
# define CLEFT 0
# define CMID 1
# define S1 31
# define S2 32
# define S3 33
# define TMP 38
#define S9 39
# define SF 35
# define SL 34
# define LSIZE 33
# define SIND 37
# define SVS 36
/* this refers to the relative position of lines */
# define LEFT 1
# define RIGHT 2
# define THRU 3
# define TOP 1
# define BOT 2

int tbl(int argc,char *argv[]);		/*t1.c*/
void setinp(int, char **);
int swapin(void);

void tableput(void);			/*t2.c*/

void getcomm(void);			/*t3.c*/
void backrest(char *);

void getspec(void);			/*t4.c*/
void readspec(void);
int findcol(void);
void garray(int);
char *getcore(int, int);
void freearr(void);

void gettbl(void);			/*t5.c*/
int nodata(int);
int oneh(int);
int vspand(int, int, int);
int vspen(char *);
void permute(void);

void maktab(void);			/*t6.c*/
void wide(char *, char *, char *);
int filler(char *);

void runout(void);			/*t7.c*/
void runtabs(int, int);
int ifline(char *);
void need(void);
void deftail(void);

void putline(int, int);			/*t8.c*/
void puttext(char *, char *, char *);
void funnies(int, int);
void putfont(char *);
void putsize(char *);

void yetmore(void);			/*t9.c*/
int domore(char *);

void checkuse(void);			/*tb.c*/
int real(char *);
char *chspace(void);
int *alocv(int);
void release(void);

void choochar(void);			/*tc.c*/
int point(char *);

void error(char *);			/*te.c*/
char *gets1(char *, int);
void un1getc(int);
int get1char(void);

void savefill(void);			/*tf.c*/
void rstofill(void);
void endoff(void);
void freearr(void);
void saveline(void);
void ifdivert(void);
void restline(void);
void cleanfc(void);

int gettext(char *, int, int, char *, char *);		/*tg.c*/
void untext(void);

int interv(int, int);			/*ti.c*/
int interh(int, int);
int up1(int);

char *maknew(char *);			/*tm.c*/
int ineqn (char *, char *);

char *reg(int, int);			/*tr.c*/

int match (char *, char *);		/*ts.c*/
int prefix(char *, char *);
int letter (int);
int numb(char *);
int digit(int);
int max(int, int);
void tcopy (char *, char *);

int ctype(int, int);			/*tt.c*/
int min(int, int);
int fspan(int, int);
int lspan(int, int);
int ctspan(int, int);
void tohcol(int);
int allh(int);
int thish(int, int);

void makeline(int, int, int);		/*tu.c*/
void fullwide(int, int);
void drawline(int, int, int, int, int, int);
void getstop(void);
int left(int, int, int *);
int lefdata(int, int);
int next(int);
int prev(int);

void drawvert(int, int, int, int);			/*tv.c*/
int midbar(int, int);
int midbcol(int, int);
int barent(char *);
