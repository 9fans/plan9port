#ifndef BUFSIZE
#include <stdio.h>
#endif
#define SCX(A) (int)((A)*e1->scalex+0.5)
#define SCY(A) (int)((A)*e1->scaley+0.5)
#define TRX(A) (int)(((A) - e1->xmin)*e1->scalex  + e1->left)
#define TRY(A) (int)(((A) - e1->ymin)*e1->scaley + e1->bottom)
#define DTRX(A) (((A) - e1->xmin)*e1->scalex  + e1->left)
#define DTRY(A) (((A) - e1->ymin)*e1->scaley + e1->bottom)
#define INCHES(A) ((A)/1000.)
extern struct penvir {
	double left, bottom;
	double xmin, ymin;
	double scalex, scaley;
	double sidex, sidey;
	double copyx, copyy;
	char *font;
	int psize;
	int pen;
	int pdiam;
	double dashlen;
	} *e0, *e1, *e2, *esave;
enum {
	SOLIDPEN, DASHPEN, DOTPEN
};
extern FILE *TEXFILE;

#define round texround

extern int round();

void		box(double x0, double y0, double x1, double y1) ;
void		circle(double xc, double yc, double r);
void		closepl(void);
void		devarc(double x1, double y1, double x2, double y2, double xc, double yc, int r);
void		disc(double xc, double yc, double r);
void		erase(void);
void		fill(int num[], double *ff[]);
void		frame(double xs, double ys, double xf, double yf);
void		line(double x0, double y0, double x1, double y1) ;
void		move(double xx, double yy) ;
void		openpl(void);
void		pen(char *s) ;
void		poly(int num[], double *ff[]);
void		range(double x0, double y0, double x1, double y1) ;
void		rmove(double xx, double yy) ;
void		rvec(double xx, double yy)  ;
void		sbox(double x0, double y0, double x1, double y1) ;
void		vec(double xx, double yy) ;
void	space(double x0, double y0, double x1, double y1);
