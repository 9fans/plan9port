/* Copyright (c) 2006 Russ Cox */

/*

tag[1] Rerror error[s]

tag[1] Trdmouse
tag[1] Rrdmouse x[4] y[4] button[4] msec[4] resized[1]

tag[1] Tmoveto x[4] y[4]
tag[1] Rmoveto

tag[1] Tcursor cursor[]
tag[1] Rcursor

tag[1] Tcursor2 cursor[]
tag[1] Rcursor2

tag[1] Tbouncemouse x[4] y[4] button[4]
tag[1] Rbouncemouse

tag[1] Trdkbd
tag[1] Rrdkbd rune[2]

tag[1] Trdkbd4
tag[1] Rrdkbd4 rune[4]

tag[1] Tlabel label[s]
tag[1] Rlabel

tag[1] Tctxt wsysid[s]
tag[1] Rctxt

tag[1] Tinit winsize[s] label[s] font[s]
tag[1] Rinit

tag[1] Trdsnarf
tag[1] Rrdsnarf snarf[s]

tag[1] Twrsnarf snarf[s]
tag[1] Rwrsnarf

tag[1] Trddraw count[4]
tag[1] Rrddraw count[4] data[count]

tag[1] Twrdraw count[4] data[count]
tag[1] Rwrdraw count[4]

tag[1] Ttop
tag[1] Rtop

tag[1] Tresize rect[4*4]
tag[1] Rresize
*/


#define PUT(p, x) \
	(p)[0] = ((x) >> 24)&0xFF, \
	(p)[1] = ((x) >> 16)&0xFF, \
	(p)[2] = ((x) >> 8)&0xFF, \
	(p)[3] = (x)&0xFF

#define GET(p, x) \
	((x) = (u32int)(((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | ((p)[3])))

#define PUT2(p, x) \
	(p)[0] = ((x) >> 8)&0xFF, \
	(p)[1] = (x)&0xFF

#define GET2(p, x) \
	((x) = (((p)[0] << 8) | ((p)[1])))

enum {
	Rerror = 1,
	Trdmouse = 2,
	Rrdmouse,
	Tmoveto = 4,
	Rmoveto,
	Tcursor = 6,
	Rcursor,
	Tbouncemouse = 8,
	Rbouncemouse,
	Trdkbd = 10,
	Rrdkbd,
	Tlabel = 12,
	Rlabel,
	Tinit = 14,
	Rinit,
	Trdsnarf = 16,
	Rrdsnarf,
	Twrsnarf = 18,
	Rwrsnarf,
	Trddraw = 20,
	Rrddraw,
	Twrdraw = 22,
	Rwrdraw,
	Ttop = 24,
	Rtop,
	Tresize = 26,
	Rresize,
	Tcursor2 = 28,
	Rcursor2,
	Tctxt = 30,
	Rctxt,
	Trdkbd4 = 32,
	Rrdkbd4,
	Tmax,
};

enum {
	MAXWMSG = 4*1024*1024
};

typedef struct Wsysmsg Wsysmsg;
struct Wsysmsg
{
	uchar type;
	uchar tag;
	Mouse mouse;
	int resized;
	Cursor cursor;
	Cursor2 cursor2;
	int arrowcursor;
	Rune rune;
	char *winsize;
	char *label;
	char *snarf;
	char *error;
	char *id;
	uchar *data;
	uint count;
	Rectangle rect;
};

uint	convW2M(Wsysmsg*, uchar*, uint);
uint	convM2W(uchar*, uint, Wsysmsg*);
uint	sizeW2M(Wsysmsg*);
int	readwsysmsg(int, uchar*, uint);

int	drawfcallfmt(Fmt*);
