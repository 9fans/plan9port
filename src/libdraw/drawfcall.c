#include <u.h>
#include <libc.h>
#include <draw.h>
#include <mouse.h>
#include <cursor.h>
#include <drawfcall.h>

static int
_stringsize(char *s)
{
	if(s == nil)
		s = "";
	return 4+strlen(s);
}

static int
PUTSTRING(uchar *p, char *s)
{
	int n;

	if(s == nil)
		s = "";
	n = strlen(s);
	PUT(p, n);
	memmove(p+4, s, n);
	return n+4;
}

static int
GETSTRING(uchar *p, char **s)
{
	int n;

	GET(p, n);
	memmove(p, p+4, n);
	*s = (char*)p;
	p[n] = 0;
	return n+4;
}

uint
sizeW2M(Wsysmsg *m)
{
	switch(m->type){
	default:
		return 0;
	case Trdmouse:
	case Rbouncemouse:
	case Rmoveto:
	case Rcursor:
	case Rcursor2:
	case Trdkbd:
	case Trdkbd4:
	case Rlabel:
	case Rctxt:
	case Rinit:
	case Trdsnarf:
	case Rwrsnarf:
	case Ttop:
	case Rtop:
	case Rresize:
		return 4+1+1;
	case Rrdmouse:
		return 4+1+1+4+4+4+4+1;
	case Tbouncemouse:
		return 4+1+1+4+4+4;
	case Tmoveto:
		return 4+1+1+4+4;
	case Tcursor:
		return 4+1+1+4+4+2*16+2*16+1;
	case Tcursor2:
		return 4+1+1+4+4+2*16+2*16+4+4+4*32+4*32+1;
	case Rerror:
		return 4+1+1+_stringsize(m->error);
	case Rrdkbd:
		return 4+1+1+2;
	case Rrdkbd4:
		return 4+1+1+4;
	case Tlabel:
		return 4+1+1+_stringsize(m->label);
	case Tctxt:
		return 4+1+1
			+_stringsize(m->id);
	case Tinit:
		return 4+1+1
			+_stringsize(m->winsize)
			+_stringsize(m->label);
	case Rrdsnarf:
	case Twrsnarf:
		return 4+1+1+_stringsize(m->snarf);
	case Rrddraw:
	case Twrdraw:
		return 4+1+1+4+m->count;
	case Trddraw:
	case Rwrdraw:
		return 4+1+1+4;
	case Tresize:
		return 4+1+1+4*4;
	}
}

uint
convW2M(Wsysmsg *m, uchar *p, uint n)
{
	int nn;

	nn = sizeW2M(m);
	if(n < nn || nn == 0 || n < 6)
		return 0;
	PUT(p, nn);
	p[4] = m->tag;
	p[5] = m->type;

	switch(m->type){
	default:
		return 0;
	case Trdmouse:
	case Rbouncemouse:
	case Rmoveto:
	case Rcursor:
	case Rcursor2:
	case Trdkbd:
	case Trdkbd4:
	case Rlabel:
	case Rctxt:
	case Rinit:
	case Trdsnarf:
	case Rwrsnarf:
	case Ttop:
	case Rtop:
	case Rresize:
		break;
	case Rerror:
		PUTSTRING(p+6, m->error);
		break;
	case Rrdmouse:
		PUT(p+6, m->mouse.xy.x);
		PUT(p+10, m->mouse.xy.y);
		PUT(p+14, m->mouse.buttons);
		PUT(p+18, m->mouse.msec);
		p[19] = m->resized;
		break;
	case Tbouncemouse:
		PUT(p+6, m->mouse.xy.x);
		PUT(p+10, m->mouse.xy.y);
		PUT(p+14, m->mouse.buttons);
		break;
	case Tmoveto:
		PUT(p+6, m->mouse.xy.x);
		PUT(p+10, m->mouse.xy.y);
		break;
	case Tcursor:
		PUT(p+6, m->cursor.offset.x);
		PUT(p+10, m->cursor.offset.y);
		memmove(p+14, m->cursor.clr, sizeof m->cursor.clr);
		memmove(p+46, m->cursor.set, sizeof m->cursor.set);
		p[78] = m->arrowcursor;
		break;
	case Tcursor2:
		PUT(p+6, m->cursor.offset.x);
		PUT(p+10, m->cursor.offset.y);
		memmove(p+14, m->cursor.clr, sizeof m->cursor.clr);
		memmove(p+46, m->cursor.set, sizeof m->cursor.set);
		PUT(p+78, m->cursor2.offset.x);
		PUT(p+82, m->cursor2.offset.y);
		memmove(p+86, m->cursor2.clr, sizeof m->cursor2.clr);
		memmove(p+214, m->cursor2.set, sizeof m->cursor2.set);
		p[342] = m->arrowcursor;
		break;
	case Rrdkbd:
		PUT2(p+6, m->rune);
		break;
	case Rrdkbd4:
		PUT(p+6, m->rune);
		break;
	case Tlabel:
		PUTSTRING(p+6, m->label);
		break;
	case Tctxt:
		PUTSTRING(p+6, m->id);
		break;
	case Tinit:
		p += 6;
		p += PUTSTRING(p, m->winsize);
		p += PUTSTRING(p, m->label);
		break;
	case Rrdsnarf:
	case Twrsnarf:
		PUTSTRING(p+6, m->snarf);
		break;
	case Rrddraw:
	case Twrdraw:
		PUT(p+6, m->count);
		memmove(p+10, m->data, m->count);
		break;
	case Trddraw:
	case Rwrdraw:
		PUT(p+6, m->count);
		break;
	case Tresize:
		PUT(p+6, m->rect.min.x);
		PUT(p+10, m->rect.min.y);
		PUT(p+14, m->rect.max.x);
		PUT(p+18, m->rect.max.y);
		break;
	}
	return nn;
}

uint
convM2W(uchar *p, uint n, Wsysmsg *m)
{
	int nn;

	if(n < 6)
		return 0;
	GET(p, nn);
	if(nn > n)
		return 0;
	m->tag = p[4];
	m->type = p[5];
	switch(m->type){
	default:
		return 0;
	case Trdmouse:
	case Rbouncemouse:
	case Rmoveto:
	case Rcursor:
	case Rcursor2:
	case Trdkbd:
	case Trdkbd4:
	case Rlabel:
	case Rctxt:
	case Rinit:
	case Trdsnarf:
	case Rwrsnarf:
	case Ttop:
	case Rtop:
	case Rresize:
		break;
	case Rerror:
		GETSTRING(p+6, &m->error);
		break;
	case Rrdmouse:
		GET(p+6, m->mouse.xy.x);
		GET(p+10, m->mouse.xy.y);
		GET(p+14, m->mouse.buttons);
		GET(p+18, m->mouse.msec);
		m->resized = p[19];
		break;
	case Tbouncemouse:
		GET(p+6, m->mouse.xy.x);
		GET(p+10, m->mouse.xy.y);
		GET(p+14, m->mouse.buttons);
		break;
	case Tmoveto:
		GET(p+6, m->mouse.xy.x);
		GET(p+10, m->mouse.xy.y);
		break;
	case Tcursor:
		GET(p+6, m->cursor.offset.x);
		GET(p+10, m->cursor.offset.y);
		memmove(m->cursor.clr, p+14, sizeof m->cursor.clr);
		memmove(m->cursor.set, p+46, sizeof m->cursor.set);
		m->arrowcursor = p[78];
		break;
	case Tcursor2:
		GET(p+6, m->cursor.offset.x);
		GET(p+10, m->cursor.offset.y);
		memmove(m->cursor.clr, p+14, sizeof m->cursor.clr);
		memmove(m->cursor.set, p+46, sizeof m->cursor.set);
		GET(p+78, m->cursor2.offset.x);
		GET(p+82, m->cursor2.offset.y);
		memmove(m->cursor2.clr, p+86, sizeof m->cursor2.clr);
		memmove(m->cursor2.set, p+214, sizeof m->cursor2.set);
		m->arrowcursor = p[342];
		break;
	case Rrdkbd:
		GET2(p+6, m->rune);
		break;
	case Rrdkbd4:
		GET(p+6, m->rune);
		break;
	case Tlabel:
		GETSTRING(p+6, &m->label);
		break;
	case Tctxt:
		GETSTRING(p+6, &m->id);
		break;
	case Tinit:
		p += 6;
		p += GETSTRING(p, &m->winsize);
		p += GETSTRING(p, &m->label);
		break;
	case Rrdsnarf:
	case Twrsnarf:
		GETSTRING(p+6, &m->snarf);
		break;
	case Rrddraw:
	case Twrdraw:
		GET(p+6, m->count);
		m->data = p+10;
		break;
	case Trddraw:
	case Rwrdraw:
		GET(p+6, m->count);
		break;
	case Tresize:
		GET(p+6, m->rect.min.x);
		GET(p+10, m->rect.min.y);
		GET(p+14, m->rect.max.x);
		GET(p+18, m->rect.max.y);
		break;
	}
	return nn;
}

int
readwsysmsg(int fd, uchar *buf, uint nbuf)
{
	int n;

	if(nbuf < 6)
		return -1;
	if(readn(fd, buf, 4) != 4)
		return -1;
	GET(buf, n);
	if(n > nbuf)
		return -1;
	if(readn(fd, buf+4, n-4) != n-4)
		return -1;
	return n;
}

int
drawfcallfmt(Fmt *fmt)
{
	Wsysmsg *m;

	m = va_arg(fmt->args, Wsysmsg*);
	fmtprint(fmt, "tag=%d ", m->tag);
	switch(m->type){
	default:
		return fmtprint(fmt, "unknown msg %d", m->type);
	case Rerror:
		return fmtprint(fmt, "Rerror error='%s'", m->error);
	case Trdmouse:
		return fmtprint(fmt, "Trdmouse");
	case Rrdmouse:
		return fmtprint(fmt, "Rrdmouse x=%d y=%d buttons=%d msec=%d resized=%d",
			m->mouse.xy.x, m->mouse.xy.y,
			m->mouse.buttons, m->mouse.msec, m->resized);
	case Tbouncemouse:
		return fmtprint(fmt, "Tbouncemouse x=%d y=%d buttons=%d",
			m->mouse.xy.x, m->mouse.xy.y, m->mouse.buttons);
	case Rbouncemouse:
		return fmtprint(fmt, "Rbouncemouse");
	case Tmoveto:
		return fmtprint(fmt, "Tmoveto x=%d y=%d", m->mouse.xy.x, m->mouse.xy.y);
	case Rmoveto:
		return fmtprint(fmt, "Rmoveto");
	case Tcursor:
		return fmtprint(fmt, "Tcursor arrow=%d", m->arrowcursor);
	case Tcursor2:
		return fmtprint(fmt, "Tcursor2 arrow=%d", m->arrowcursor);
	case Rcursor:
		return fmtprint(fmt, "Rcursor");
	case Rcursor2:
		return fmtprint(fmt, "Rcursor2");
	case Trdkbd:
		return fmtprint(fmt, "Trdkbd");
	case Rrdkbd:
		return fmtprint(fmt, "Rrdkbd rune=%C", m->rune);
	case Trdkbd4:
		return fmtprint(fmt, "Trdkbd4");
	case Rrdkbd4:
		return fmtprint(fmt, "Rrdkbd4 rune=%C", m->rune);
	case Tlabel:
		return fmtprint(fmt, "Tlabel label='%s'", m->label);
	case Rlabel:
		return fmtprint(fmt, "Rlabel");
	case Tctxt:
		return fmtprint(fmt, "Tctxt id='%s'", m->id);
	case Rctxt:
		return fmtprint(fmt, "Rctxt");
	case Tinit:
		return fmtprint(fmt, "Tinit label='%s' winsize='%s'", m->label, m->winsize);
	case Rinit:
		return fmtprint(fmt, "Rinit");
	case Trdsnarf:
		return fmtprint(fmt, "Trdsnarf");
	case Rrdsnarf:
		return fmtprint(fmt, "Rrdsnarf snarf='%s'", m->snarf);
	case Twrsnarf:
		return fmtprint(fmt, "Twrsnarf snarf='%s'", m->snarf);
	case Rwrsnarf:
		return fmtprint(fmt, "Rwrsnarf");
	case Trddraw:
		return fmtprint(fmt, "Trddraw %d", m->count);
	case Rrddraw:
		return fmtprint(fmt, "Rrddraw %d %.*H", m->count, m->count, m->data);
	case Twrdraw:
		return fmtprint(fmt, "Twrdraw %d %.*H", m->count, m->count, m->data);
	case Rwrdraw:
		return fmtprint(fmt, "Rwrdraw %d", m->count);
	case Ttop:
		return fmtprint(fmt, "Ttop");
	case Rtop:
		return fmtprint(fmt, "Rtop");
	case Tresize:
		return fmtprint(fmt, "Tresize %R", m->rect);
	case Rresize:
		return fmtprint(fmt, "Rresize");
	}
}
