/*
          t string  Place the string so that its first character is
                    centered on the current point (default).  If
                    string begins with `\C' (`\R'), it is centered
                    (right-adjusted) on the current point.  A
                    backslash at the beginning of the string may be
                    escaped with another backslash.
 */
#include "mplot.h"
void text(char *s){
	register int	kx, ky;
	int centered, right, more;
	char *ss;
	ss=s;
	for(;;){
		centered=right=more=0;
		if(*ss=='\\'){
			ss++;
			switch(*ss){
			case 'C': centered++; ss++; break;
			case 'R': right++; ss++; break;
			case 'L': ss++; break;
			case 'n': --ss; break;
			}
		}
		for(s=ss;*ss!='\0';ss++)
			if(ss[0]=='\\' && ss[1]=='n'){
				more++;
				break;
			}
		kx = SCX(e1->copyx);
		ky = SCY(e1->copyy);
		ky=m_text(kx, ky, s, ss, e1->foregr, centered, right);
		if(!more)break;
		e1->copyy = ( (double)(ky) - e1->bottom)/e1->scaley + e1->ymin + .5;
		move(e1->copyx, e1->copyy);
		ss+=2;
	}
}
