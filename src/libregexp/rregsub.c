#include "lib9.h"
#include "regexp9.h"

/* substitute into one string using the matches from the last regexec() */
extern	void
rregsub(Rune *sp,	/* source string */
	Rune *dp,	/* destination string */
	int dlen,
	Resub *mp,	/* subexpression elements */
	int ms)		/* number of elements pointed to by mp */
{
	Rune *ssp, *ep;
	int i;

	ep = dp+(dlen/sizeof(Rune))-1;
	while(*sp != '\0'){
		if(*sp == '\\'){
			switch(*++sp){
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				i = *sp-'0';
				if(mp[i].s.rsp != 0 && mp!=0 && ms>i)
					for(ssp = mp[i].s.rsp;
					     ssp < mp[i].e.rep;
					     ssp++)
						if(dp < ep)
							*dp++ = *ssp;
				break;
			case '\\':
				if(dp < ep)
					*dp++ = '\\';
				break;
			case '\0':
				sp--;
				break;
			default:
				if(dp < ep)
					*dp++ = *sp;
				break;
			}
		}else if(*sp == '&'){				
			if(mp[0].s.rsp != 0 && mp!=0 && ms>0)
			if(mp[0].s.rsp != 0)
				for(ssp = mp[0].s.rsp;
				     ssp < mp[0].e.rep; ssp++)
					if(dp < ep)
						*dp++ = *ssp;
		}else{
			if(dp < ep)
				*dp++ = *sp;
		}
		sp++;
	}
	*dp = '\0';
}
