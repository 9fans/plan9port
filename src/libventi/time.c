#include <u.h>
#include <libc.h>
#include <venti.h>

int
vttimefmt(Fmt *fmt)
{
	vlong ns;
	Tm tm;

	if(fmt->flags&FmtSign){
		ns = va_arg(fmt->args, long);
		ns *= 1000000000;
	} else
		ns = nsec();
	tm = *localtime(ns/1000000000);
	if(fmt->flags&FmtLong){
		return fmtprint(fmt, "%04d/%02d%02d %02d:%02d:%02d.%03d",
			tm.year+1900, tm.mon+1, tm.mday,
			tm.hour, tm.min, tm.sec,
			(int)(ns%1000000000)/1000000);
	}else{
		return fmtprint(fmt, "%04d/%02d%02d %02d:%02d:%02d",
			tm.year+1900, tm.mon+1, tm.mday,
			tm.hour, tm.min, tm.sec);
	}
}
