#include "mplot.h"
void cfill(char *s){
	int k=bcolor(s);
	if(k>=0) e1->backgr=k;
}
