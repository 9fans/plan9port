#include "mplot.h"
void erase(void){
	m_swapbuf();
	m_clrwin(clipminx, clipminy, clipmaxx, clipmaxy, e1->backgr);
}
