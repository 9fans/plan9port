#define arc(_x,_y,_X,_Y,_u,_v,_r)  printf("a %d %d %d %d %d %d %d\n",_x,_y,_X,_Y,_u,_v,_r)
#define box(_x,_y,_X,_Y)  printf("bo %d %d %d %d\n", _x,_y,_X,_Y)
#define bspline(_num,_ff) {printf("bs {\n"); putnum(_num,_ff); printf("}\n");}
#define call(_sname,_x)  printf("ca %s %d\n", _sname,_x)
#define cfill(_s)  printf("cf %s\n", _s)
#define circle(_x,_y,_r)  printf("ci %d %d %d\n", _x,_y,_r)
#define closepl()  printf("cl\n")
#define color(_s)  printf("co %s\n", _s)
#define cspline(_num,_ff) {printf("cs {\n"); putnum(_num,_ff); printf("}\n");}
#define pdefine(_sname, _str)  printf("de %s {\n%s\n",_sname, _str)
#define dspline(_num,_ff) {printf("ds {\n");putnum(_num,_ff);printf("}\n");}
#define erase() printf("e\n")
#define ffill(_s)  printf("ff %s\n", _s)
#define fill(_num,_ff)  {printf("fi {\n"); putnum(_num,_ff); printf("}\n");}
#define fpoly(_s)  printf("fp %s\n", _s)
#define frame(_x,_y,_X,_Y)  printf("fr %g %g %g %g\n", _x,_y,_X,_Y)
#define fspline(_s)  printf("fs %s\n", _s)
#define grade(_x)  printf("g %d\n", _x)
#define idle()
#define line(_x1,_y1,_x2,_y2)  printf("li %d %d %d %d\n", _x1,_y1,_x2,_y2)
#define lspline(_num,_ff) {printf("ls {\n"); putnum(_num,_ff); printf("}\n");}
#define move(_x, _y)  printf("m %d %d\n", _x, _y)
#define openpl()  printf("o\n")
#define parabola(_x,_y,_X,_Y,_u,_v)  printf("\npa %d %d %d %d %d %d\n", _x,_y,_X,_Y,_u,_v)
#define pen(_s)  printf("pe %s\n", _s)
#define point(_x,_y)  printf("poi %d %d\n", _x,_y)
#define poly(_num,_ff)  {printf("pol {\n"); putnum(_num,_ff); printf("}\n");}
#define ppause() printf("pp\n")
#define range(_x,_y,_X,_Y)  printf("ra %d %d %d %d\n", _x,_y,_X,_Y)
#define restore() printf("re\n")
#define rmove(_x,_y)  printf("rm %d %d\n", _x,_y)
#define rvec(_x,_y)  printf("rv %d %d\n", _x,_y)
#define save() printf( "sa\n")
#define sbox(_x,_y,_X,_Y)  printf("sb %d %d %d %d\n", _x,_y,_X,_Y)
#define spline(_num,_ff)  {printf("sp {\n"); putnum(_num,_ff); printf("}\n");}
#define text(_s)  {if(*(_s) == ' ')printf("t \"%s\"\n",_s); else printf("t %s\n", _s); }
#define vec(_x,_y)  printf("v %d %d\n", _x,_y)
void putnum(int [], double *[]);
char *whoami(void);
