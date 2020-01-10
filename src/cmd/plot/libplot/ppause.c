#include "mplot.h"
void ppause(void){
	char	aa[4];
	fflush(stdout);
	read(0, aa, 4);
	erase();
}
