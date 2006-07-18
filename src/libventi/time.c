#include <u.h>
#include <libc.h>
#include <venti.h>

int
vttimefmt(Fmt *fmt)
{
	vlong ns;
	Tm tm;

	if(fmt->flags&FmtLong){
		ns = nsec();
		tm = *localtime(ns/1000000000);
		return fmtprint(fmt, "%04d/%02d%02d %02d:%02d:%02d.%03d", 
			tm.year+1900, tm.mon+1, tm.mday, 
			tm.hour, tm.min, tm.sec,
			(int)(ns%1000000000)/1000000);
	}else{
		tm = *localtime(time(0));
		return fmtprint(fmt, "%04d/%02d%02d %02d:%02d:%02d", 
			tm.year+1900, tm.mon+1, tm.mday, 
			tm.hour, tm.min, tm.sec);
	}
}

