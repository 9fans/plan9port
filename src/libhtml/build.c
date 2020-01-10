#include <u.h>
#include <libc.h>
#include <draw.h>
#include <ctype.h>
#include <html.h>
#include "impl.h"

/* A stack for holding integer values */
enum {
	Nestmax = 40	/* max nesting level of lists, font styles, etc. */
};

struct Stack {
	int		n;				/* next available slot (top of stack is stack[n-1]) */
	int		slots[Nestmax];	/* stack entries */
};

/* Parsing state */
struct Pstate
{
	Pstate*	next;			/* in stack of Pstates */
	int		skipping;		/* true when we shouldn't add items */
	int		skipwhite;		/* true when we should strip leading space */
	int		curfont;		/* font index for current font */
	int		curfg;		/* current foreground color */
	Background	curbg;	/* current background */
	int		curvoff;		/* current baseline offset */
	uchar	curul;		/* current underline/strike state */
	uchar	curjust;		/* current justify state */
	int		curanchor;	/* current (href) anchor id (if in one), or 0 */
	int		curstate;		/* current value of item state */
	int		literal;		/* current literal state */
	int		inpar;		/* true when in a paragraph-like construct */
	int		adjsize;		/* current font size adjustment */
	Item*	items;		/* dummy head of item list we're building */
	Item*	lastit;		/* tail of item list we're building */
	Item*	prelastit;		/* item before lastit */
	Stack	fntstylestk;	/* style stack */
	Stack	fntsizestk;		/* size stack */
	Stack	fgstk;		/* text color stack */
	Stack	ulstk;		/* underline stack */
	Stack	voffstk;		/* vertical offset stack */
	Stack	listtypestk;	/* list type stack */
	Stack	listcntstk;		/* list counter stack */
	Stack	juststk;		/* justification stack */
	Stack	hangstk;		/* hanging stack */
};

struct ItemSource
{
	Docinfo*		doc;
	Pstate*		psstk;
	int			nforms;
	int			ntables;
	int			nanchors;
	int			nframes;
	Form*		curform;
	Map*		curmap;
	Table*		tabstk;
	Kidinfo*		kidstk;
};

/* Some layout parameters */
enum {
	FRKIDMARGIN = 6,	/* default margin around kid frames */
	IMGHSPACE = 0,	/* default hspace for images (0 matches IE, Netscape) */
	IMGVSPACE = 0,	/* default vspace for images */
	FLTIMGHSPACE = 2,	/* default hspace for float images */
	TABSP = 5,		/* default cellspacing for tables */
	TABPAD = 1,		/* default cell padding for tables */
	LISTTAB = 1,		/* number of tabs to indent lists */
	BQTAB = 1,		/* number of tabs to indent blockquotes */
	HRSZ = 2,			/* thickness of horizontal rules */
	SUBOFF = 4,		/* vertical offset for subscripts */
	SUPOFF = 6,		/* vertical offset for superscripts */
	NBSP = 160		/* non-breaking space character */
};

/* These tables must be sorted */
static StringInt *align_tab;
static AsciiInt _align_tab[] = {
	{"baseline",	ALbaseline},
	{"bottom",	ALbottom},
	{"center",	ALcenter},
	{"char",		ALchar},
	{"justify",	ALjustify},
	{"left",		ALleft},
	{"middle",	ALmiddle},
	{"right",		ALright},
	{"top",		ALtop}
};
#define NALIGNTAB (sizeof(_align_tab)/sizeof(StringInt))

static StringInt *input_tab;
static AsciiInt _input_tab[] = {
	{"button",	Fbutton},
	{"checkbox",	Fcheckbox},
	{"file",		Ffile},
	{"hidden",	Fhidden},
	{"image",	Fimage},
	{"password",	Fpassword},
	{"radio",		Fradio},
	{"reset",		Freset},
	{"submit",	Fsubmit},
	{"text",		Ftext}
};
#define NINPUTTAB (sizeof(_input_tab)/sizeof(StringInt))

static StringInt *clear_tab;
static AsciiInt _clear_tab[] = {
	{"all",	IFcleft|IFcright},
	{"left",	IFcleft},
	{"right",	IFcright}
};
#define NCLEARTAB (sizeof(_clear_tab)/sizeof(StringInt))

static StringInt *fscroll_tab;
static AsciiInt _fscroll_tab[] = {
	{"auto",	FRhscrollauto|FRvscrollauto},
	{"no",	FRnoscroll},
	{"yes",	FRhscroll|FRvscroll},
};
#define NFSCROLLTAB (sizeof(_fscroll_tab)/sizeof(StringInt))

static StringInt *shape_tab;
static AsciiInt _shape_tab[] = {
	{"circ",		SHcircle},
	{"circle",		SHcircle},
	{"poly",		SHpoly},
	{"polygon",	SHpoly},
	{"rect",		SHrect},
	{"rectangle",	SHrect}
};
#define NSHAPETAB (sizeof(_shape_tab)/sizeof(StringInt))

static StringInt *method_tab;
static AsciiInt _method_tab[] = {
	{"get",		HGet},
	{"post",		HPost}
};
#define NMETHODTAB (sizeof(_method_tab)/sizeof(StringInt))

static Rune** roman;
static char* _roman[15]= {
	"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X",
	"XI", "XII", "XIII", "XIV", "XV"
};
#define NROMAN 15

/* List number types */
enum {
	LTdisc, LTsquare, LTcircle, LT1, LTa, LTA, LTi, LTI
};

enum {
	SPBefore = 2,
	SPAfter = 4,
	BL = 1,
	BLBA = (BL|SPBefore|SPAfter)
};

/* blockbrk[tag] is break info for a block level element, or one */
/* of a few others that get the same treatment re ending open paragraphs */
/* and requiring a line break / vertical space before them. */
/* If we want a line of space before the given element, SPBefore is OR'd in. */
/* If we want a line of space after the given element, SPAfter is OR'd in. */

static uchar blockbrk[Numtags]= {
/*Notfound*/ 0,
/*Comment*/ 0,
/*Ta*/ 0,
/*Tabbr*/ 0,
/*Tacronym*/ 0,
/*Taddress*/ BLBA,
/*Tapplet*/ 0,
/*Tarea*/ 0,
/*Tb*/ 0,
/*Tbase*/ 0,
/*Tbasefont*/ 0,
/*Tbdo*/ 0,
/*Tbig*/ 0,
/*Tblink*/ 0,
/*Tblockquote*/ BLBA,
/*Tbody*/ 0,
/*Tbq*/ 0,
/*Tbr*/ 0,
/*Tbutton*/ 0,
/*Tcaption*/ 0,
/*Tcenter*/ BL,
/*Tcite*/ 0,
/*Tcode*/ 0,
/*Tcol*/ 0,
/*Tcolgroup*/ 0,
/*Tdd*/ BL,
/*Tdel*/ 0,
/*Tdfn*/ 0,
/*Tdir*/ BLBA,
/*Tdiv*/ BL,
/*Tdl*/ BLBA,
/*Tdt*/ BL,
/*Tem*/ 0,
/*Tfieldset*/ 0,
/*Tfont*/ 0,
/*Tform*/ BLBA,
/*Tframe*/ 0,
/*Tframeset*/ 0,
/*Th1*/ BL,
/*Th2*/ BL,
/*Th3*/ BL,
/*Th4*/ BL,
/*Th5*/ BL,
/*Th6*/ BL,
/*Thead*/ 0,
/*Thr*/ BL,
/*Thtml*/ 0,
/*Ti*/ 0,
/*Tiframe*/ 0,
/*Timg*/ 0,
/*Tinput*/ 0,
/*Tins*/ 0,
/*Tisindex*/ BLBA,
/*Tkbd*/ 0,
/*Tlabel*/ 0,
/*Tlegend*/ 0,
/*Tli*/ BL,
/*Tlink*/ 0,
/*Tmap*/ 0,
/*Tmenu*/ BLBA,
/*Tmeta*/ 0,
/*Tnobr*/ 0,
/*Tnoframes*/ 0,
/*Tnoscript*/ 0,
/*Tobject*/ 0,
/*Tol*/ BLBA,
/*Toptgroup*/ 0,
/*Toption*/ 0,
/*Tp*/ BLBA,
/*Tparam*/ 0,
/*Tpre*/ BLBA,
/*Tq*/ 0,
/*Ts*/ 0,
/*Tsamp*/ 0,
/*Tscript*/ 0,
/*Tselect*/ 0,
/*Tsmall*/ 0,
/*Tspan*/ 0,
/*Tstrike*/ 0,
/*Tstrong*/ 0,
/*Tstyle*/ 0,
/*Tsub*/ 0,
/*Tsup*/ 0,
/*Ttable*/ 0,
/*Ttbody*/ 0,
/*Ttd*/ 0,
/*Ttextarea*/ 0,
/*Ttfoot*/ 0,
/*Tth*/ 0,
/*Tthead*/ 0,
/*Ttitle*/ 0,
/*Ttr*/ 0,
/*Ttt*/ 0,
/*Tu*/ 0,
/*Tul*/ BLBA,
/*Tvar*/ 0,
};

enum {
	AGEN = 1
};

/* attrinfo is information about attributes. */
/* The AGEN value means that the attribute is generic (applies to almost all elements) */
static uchar attrinfo[Numattrs]= {
/*Aabbr*/ 0,
/*Aaccept_charset*/ 0,
/*Aaccess_key*/ 0,
/*Aaction*/ 0,
/*Aalign*/ 0,
/*Aalink*/ 0,
/*Aalt*/ 0,
/*Aarchive*/ 0,
/*Aaxis*/ 0,
/*Abackground*/ 0,
/*Abgcolor*/ 0,
/*Aborder*/ 0,
/*Acellpadding*/ 0,
/*Acellspacing*/ 0,
/*Achar*/ 0,
/*Acharoff*/ 0,
/*Acharset*/ 0,
/*Achecked*/ 0,
/*Acite*/ 0,
/*Aclass*/ AGEN,
/*Aclassid*/ 0,
/*Aclear*/ 0,
/*Acode*/ 0,
/*Acodebase*/ 0,
/*Acodetype*/ 0,
/*Acolor*/ 0,
/*Acols*/ 0,
/*Acolspan*/ 0,
/*Acompact*/ 0,
/*Acontent*/ 0,
/*Acoords*/ 0,
/*Adata*/ 0,
/*Adatetime*/ 0,
/*Adeclare*/ 0,
/*Adefer*/ 0,
/*Adir*/ 0,
/*Adisabled*/ 0,
/*Aenctype*/ 0,
/*Aface*/ 0,
/*Afor*/ 0,
/*Aframe*/ 0,
/*Aframeborder*/ 0,
/*Aheaders*/ 0,
/*Aheight*/ 0,
/*Ahref*/ 0,
/*Ahreflang*/ 0,
/*Ahspace*/ 0,
/*Ahttp_equiv*/ 0,
/*Aid*/ AGEN,
/*Aismap*/ 0,
/*Alabel*/ 0,
/*Alang*/ 0,
/*Alink*/ 0,
/*Alongdesc*/ 0,
/*Amarginheight*/ 0,
/*Amarginwidth*/ 0,
/*Amaxlength*/ 0,
/*Amedia*/ 0,
/*Amethod*/ 0,
/*Amultiple*/ 0,
/*Aname*/ 0,
/*Anohref*/ 0,
/*Anoresize*/ 0,
/*Anoshade*/ 0,
/*Anowrap*/ 0,
/*Aobject*/ 0,
/*Aonblur*/ AGEN,
/*Aonchange*/ AGEN,
/*Aonclick*/ AGEN,
/*Aondblclick*/ AGEN,
/*Aonfocus*/ AGEN,
/*Aonkeypress*/ AGEN,
/*Aonkeyup*/ AGEN,
/*Aonload*/ AGEN,
/*Aonmousedown*/ AGEN,
/*Aonmousemove*/ AGEN,
/*Aonmouseout*/ AGEN,
/*Aonmouseover*/ AGEN,
/*Aonmouseup*/ AGEN,
/*Aonreset*/ AGEN,
/*Aonselect*/ AGEN,
/*Aonsubmit*/ AGEN,
/*Aonunload*/ AGEN,
/*Aprofile*/ 0,
/*Aprompt*/ 0,
/*Areadonly*/ 0,
/*Arel*/ 0,
/*Arev*/ 0,
/*Arows*/ 0,
/*Arowspan*/ 0,
/*Arules*/ 0,
/*Ascheme*/ 0,
/*Ascope*/ 0,
/*Ascrolling*/ 0,
/*Aselected*/ 0,
/*Ashape*/ 0,
/*Asize*/ 0,
/*Aspan*/ 0,
/*Asrc*/ 0,
/*Astandby*/ 0,
/*Astart*/ 0,
/*Astyle*/ AGEN,
/*Asummary*/ 0,
/*Atabindex*/ 0,
/*Atarget*/ 0,
/*Atext*/ 0,
/*Atitle*/ AGEN,
/*Atype*/ 0,
/*Ausemap*/ 0,
/*Avalign*/ 0,
/*Avalue*/ 0,
/*Avaluetype*/ 0,
/*Aversion*/ 0,
/*Avlink*/ 0,
/*Avspace*/ 0,
/*Awidth*/ 0,
};

static uchar scriptev[Numattrs]= {
/*Aabbr*/ 0,
/*Aaccept_charset*/ 0,
/*Aaccess_key*/ 0,
/*Aaction*/ 0,
/*Aalign*/ 0,
/*Aalink*/ 0,
/*Aalt*/ 0,
/*Aarchive*/ 0,
/*Aaxis*/ 0,
/*Abackground*/ 0,
/*Abgcolor*/ 0,
/*Aborder*/ 0,
/*Acellpadding*/ 0,
/*Acellspacing*/ 0,
/*Achar*/ 0,
/*Acharoff*/ 0,
/*Acharset*/ 0,
/*Achecked*/ 0,
/*Acite*/ 0,
/*Aclass*/ 0,
/*Aclassid*/ 0,
/*Aclear*/ 0,
/*Acode*/ 0,
/*Acodebase*/ 0,
/*Acodetype*/ 0,
/*Acolor*/ 0,
/*Acols*/ 0,
/*Acolspan*/ 0,
/*Acompact*/ 0,
/*Acontent*/ 0,
/*Acoords*/ 0,
/*Adata*/ 0,
/*Adatetime*/ 0,
/*Adeclare*/ 0,
/*Adefer*/ 0,
/*Adir*/ 0,
/*Adisabled*/ 0,
/*Aenctype*/ 0,
/*Aface*/ 0,
/*Afor*/ 0,
/*Aframe*/ 0,
/*Aframeborder*/ 0,
/*Aheaders*/ 0,
/*Aheight*/ 0,
/*Ahref*/ 0,
/*Ahreflang*/ 0,
/*Ahspace*/ 0,
/*Ahttp_equiv*/ 0,
/*Aid*/ 0,
/*Aismap*/ 0,
/*Alabel*/ 0,
/*Alang*/ 0,
/*Alink*/ 0,
/*Alongdesc*/ 0,
/*Amarginheight*/ 0,
/*Amarginwidth*/ 0,
/*Amaxlength*/ 0,
/*Amedia*/ 0,
/*Amethod*/ 0,
/*Amultiple*/ 0,
/*Aname*/ 0,
/*Anohref*/ 0,
/*Anoresize*/ 0,
/*Anoshade*/ 0,
/*Anowrap*/ 0,
/*Aobject*/ 0,
/*Aonblur*/ SEonblur,
/*Aonchange*/ SEonchange,
/*Aonclick*/ SEonclick,
/*Aondblclick*/ SEondblclick,
/*Aonfocus*/ SEonfocus,
/*Aonkeypress*/ SEonkeypress,
/*Aonkeyup*/ SEonkeyup,
/*Aonload*/ SEonload,
/*Aonmousedown*/ SEonmousedown,
/*Aonmousemove*/ SEonmousemove,
/*Aonmouseout*/ SEonmouseout,
/*Aonmouseover*/ SEonmouseover,
/*Aonmouseup*/ SEonmouseup,
/*Aonreset*/ SEonreset,
/*Aonselect*/ SEonselect,
/*Aonsubmit*/ SEonsubmit,
/*Aonunload*/ SEonunload,
/*Aprofile*/ 0,
/*Aprompt*/ 0,
/*Areadonly*/ 0,
/*Arel*/ 0,
/*Arev*/ 0,
/*Arows*/ 0,
/*Arowspan*/ 0,
/*Arules*/ 0,
/*Ascheme*/ 0,
/*Ascope*/ 0,
/*Ascrolling*/ 0,
/*Aselected*/ 0,
/*Ashape*/ 0,
/*Asize*/ 0,
/*Aspan*/ 0,
/*Asrc*/ 0,
/*Astandby*/ 0,
/*Astart*/ 0,
/*Astyle*/ 0,
/*Asummary*/ 0,
/*Atabindex*/ 0,
/*Atarget*/ 0,
/*Atext*/ 0,
/*Atitle*/ 0,
/*Atype*/ 0,
/*Ausemap*/ 0,
/*Avalign*/ 0,
/*Avalue*/ 0,
/*Avaluetype*/ 0,
/*Aversion*/ 0,
/*Avlink*/ 0,
/*Avspace*/ 0,
/*Awidth*/ 0,
};

/* Color lookup table */
static StringInt *color_tab;
static AsciiInt _color_tab[] = {
	{"aqua", 0x00FFFF},
	{"black",  0x000000},
	{"blue", 0x0000CC},
	{"fuchsia", 0xFF00FF},
	{"gray", 0x808080},
	{"green", 0x008000},
	{"lime", 0x00FF00},
	{"maroon", 0x800000},
	{"navy", 0x000080,},
	{"olive", 0x808000},
	{"purple", 0x800080},
	{"red", 0xFF0000},
	{"silver", 0xC0C0C0},
	{"teal", 0x008080},
	{"white", 0xFFFFFF},
	{"yellow", 0xFFFF00}
};
#define NCOLORS (sizeof(_color_tab)/sizeof(StringInt))

static StringInt 		*targetmap;
static int			targetmapsize;
static int			ntargets;

static int buildinited = 0;

#define SMALLBUFSIZE 240
#define BIGBUFSIZE 2000

int	dbgbuild = 0;
int	warn = 0;

static Align		aalign(Token* tok);
static int			acolorval(Token* tok, int attid, int dflt);
static void			addbrk(Pstate* ps, int sp, int clr);
static void			additem(Pstate* ps, Item* it, Token* tok);
static void			addlinebrk(Pstate* ps, int clr);
static void			addnbsp(Pstate* ps);
static void			addtext(Pstate* ps, Rune* s);
static Dimen		adimen(Token* tok, int attid);
static int			aflagval(Token* tok, int attid);
static int			aintval(Token* tok, int attid, int dflt);
static Rune*		astrval(Token* tok, int attid, Rune* dflt);
static int			atabval(Token* tok, int attid, StringInt* tab, int ntab, int dflt);
static int			atargval(Token* tok, int dflt);
static int			auintval(Token* tok, int attid, int dflt);
static Rune*		aurlval(Token* tok, int attid, Rune* dflt, Rune* base);
static Rune*		aval(Token* tok, int attid);
static void			buildinit(void);
static Pstate*		cell_pstate(Pstate* oldps, int ishead);
static void			changehang(Pstate* ps, int delta);
static void			changeindent(Pstate* ps, int delta);
static int			color(Rune* s, int dflt);
static void			copystack(Stack* tostk, Stack* fromstk);
static int			dimprint(char* buf, int nbuf, Dimen d);
static Pstate*		finishcell(Table* curtab, Pstate* psstk);
static void			finish_table(Table* t);
static void			freeanchor(Anchor* a);
static void			freedestanchor(DestAnchor* da);
static void			freeform(Form* f);
static void			freeformfield(Formfield* ff);
static void			freeitem(Item* it);
static void			freepstate(Pstate* p);
static void			freepstatestack(Pstate* pshead);
static void			freescriptevents(SEvent* ehead);
static void			freetable(Table* t);
static Map*		getmap(Docinfo* di, Rune* name);
static Rune*		getpcdata(Token* toks, int tokslen, int* ptoki);
static Pstate*		lastps(Pstate* psl);
static Rune*		listmark(uchar ty, int n);
static int			listtyval(Token* tok, int dflt);
static Align		makealign(int halign, int valign);
static Background	makebackground(Rune* imgurl, int color);
static Dimen		makedimen(int kind, int spec);
static Anchor*		newanchor(int index, Rune* name, Rune* href, int target, Anchor* link);
static Area*		newarea(int shape, Rune* href, int target, Area* link);
static DestAnchor*	newdestanchor(int index, Rune* name, Item* item, DestAnchor* link);
static Docinfo*		newdocinfo(void);
static Genattr*		newgenattr(Rune* id, Rune* class, Rune* style, Rune* title, SEvent* events);
static Form*		newform(int formid, Rune* name, Rune* action,
					int target, int method, Form* link);
static Formfield*	newformfield(int ftype, int fieldid, Form* form, Rune* name,
					Rune* value, int size, int maxlength, Formfield* link);
static Item*		newifloat(Item* it, int side);
static Item*		newiformfield(Formfield* ff);
static Item*		newiimage(Rune* src, Rune* altrep, int align, int width, int height,
					int hspace, int vspace, int border, int ismap, Map* map);
static Item*		newirule(int align, int size, int noshade, Dimen wspec);
static Item*		newispacer(int spkind);
static Item*		newitable(Table* t);
static ItemSource*	newitemsource(Docinfo* di);
static Item*		newitext(Rune* s, int fnt, int fg, int voff, int ul);
static Kidinfo*		newkidinfo(int isframeset, Kidinfo* link);
static Option*		newoption(int selected, Rune* value, Rune* display, Option* link);
static Pstate*		newpstate(Pstate* link);
static SEvent*		newscriptevent(int type, Rune* script, SEvent* link);
static Table*		newtable(int tableid, Align align, Dimen width, int border,
					int cellspacing, int cellpadding, Background bg, Token* tok, Table* link);
static Tablecell*	newtablecell(int cellid, int rowspan, int colspan, Align align, Dimen wspec,
					int hspec, Background bg, int flags, Tablecell* link);
static Tablerow*	newtablerow(Align align, Background bg, int flags, Tablerow* link);
static Dimen		parsedim(Rune* s, int ns);
static void			pop(Stack* stk);
static void			popfontsize(Pstate* ps);
static void			popfontstyle(Pstate* ps);
static void			popjust(Pstate* ps);
static int			popretnewtop(Stack* stk, int dflt);
static int			push(Stack* stk, int val);
static void			pushfontsize(Pstate* ps, int sz);
static void			pushfontstyle(Pstate* ps, int sty);
static void			pushjust(Pstate* ps, int j);
static Item*		textit(Pstate* ps, Rune* s);
static Rune*		removeallwhite(Rune* s);
static void			resetdocinfo(Docinfo* d);
static void			setcurfont(Pstate* ps);
static void			setcurjust(Pstate* ps);
static void			setdimarray(Token* tok, int attid, Dimen** pans, int* panslen);
static Rune*		stringalign(int a);
static void			targetmapinit(void);
static int			toint(Rune* s);
static int			top(Stack* stk, int dflt);
static void			trim_cell(Tablecell* c);
static int			validalign(Align a);
static int			validdimen(Dimen d);
static int			validformfield(Formfield* f);
static int			validhalign(int a);
static int			validptr(void* p);
static int			validStr(Rune* s);
static int			validtable(Table* t);
static int			validtablerow(Tablerow* r);
static int			validtablecol(Tablecol* c);
static int			validtablecell(Tablecell* c);
static int			validvalign(int a);
static int			Iconv(Fmt *f);

static void
buildinit(void)
{
	_runetabinit();
	roman = _cvtstringtab(_roman, nelem(_roman));
	color_tab = _cvtstringinttab(_color_tab, nelem(_color_tab));
	method_tab = _cvtstringinttab(_method_tab, nelem(_method_tab));
	shape_tab = _cvtstringinttab(_shape_tab, nelem(_shape_tab));
	fscroll_tab = _cvtstringinttab(_fscroll_tab, nelem(_fscroll_tab));
	clear_tab = _cvtstringinttab(_clear_tab, nelem(_clear_tab));
	input_tab = _cvtstringinttab(_input_tab, nelem(_input_tab));
	align_tab = _cvtstringinttab(_align_tab, nelem(_align_tab));

	fmtinstall('I', Iconv);
	targetmapinit();
	buildinited = 1;
}

static ItemSource*
newitemsource(Docinfo* di)
{
	ItemSource*	is;
	Pstate*	ps;

	ps = newpstate(nil);
	if(di->mediatype != TextHtml) {
		ps->curstate &= ~IFwrap;
		ps->literal = 1;
		pushfontstyle(ps, FntT);
	}
	is = (ItemSource*)emalloc(sizeof(ItemSource));
	is->doc = di;
	is->psstk = ps;
	is->nforms = 0;
	is->ntables = 0;
	is->nanchors = 0;
	is->nframes = 0;
	is->curform = nil;
	is->curmap = nil;
	is->tabstk = nil;
	is->kidstk = nil;
	return is;
}

static Item *getitems(ItemSource* is, uchar* data, int datalen);

/* Parse an html document and create a list of layout items. */
/* Allocate and return document info in *pdi. */
/* When caller is done with the items, it should call */
/* freeitems on the returned result, and then */
/* freedocinfo(*pdi). */
Item*
parsehtml(uchar* data, int datalen, Rune* pagesrc, int mtype, int chset, Docinfo** pdi)
{
	Item *it;
	Docinfo*	di;
	ItemSource*	is;

	di = newdocinfo();
	di->src = _Strdup(pagesrc);
	di->base = _Strdup(pagesrc);
	di->mediatype = mtype;
	di->chset = chset;
	*pdi = di;
	is = newitemsource(di);
	it = getitems(is, data, datalen);
	freepstatestack(is->psstk);
	free(is);
	return it;
}

/* Get a group of tokens for lexer, parse them, and create */
/* a list of layout items. */
/* When caller is done with the items, it should call */
/* freeitems on the returned result. */
static Item*
getitems(ItemSource* is, uchar* data, int datalen)
{
	int	i;
	int	j;
	int	nt;
	int	pt;
	int	doscripts;
	int	tokslen;
	int	toki;
	int	h;
	int	sz;
	int	method;
	int	n;
	int	nblank;
	int	norsz;
	int	bramt;
	int	sty;
	int	nosh;
	int	oldcuranchor;
	int	dfltbd;
	int	v;
	int	hang;
	int	isempty;
	int	tag;
	int	brksp;
	int	target;
	uchar	brk;
	uchar	flags;
	uchar	align;
	uchar	al;
	uchar	ty;
	uchar	ty2;
	Pstate*	ps;
	Pstate*	nextps;
	Pstate*	outerps;
	Table*	curtab;
	Token*	tok;
	Token*	toks;
	Docinfo*	di;
	Item*	ans;
	Item*	img;
	Item*	ffit;
	Item*	tabitem;
	Rune*	s;
	Rune*	t;
	Rune*	name;
	Rune*	enctype;
	Rune*	usemap;
	Rune*	prompt;
	Rune*	equiv;
	Rune*	val;
	Rune*	nsz;
	Rune*	script;
	Map*	map;
	Form*	frm;
	Iimage*	ii;
	Kidinfo*	kd;
	Kidinfo*	ks;
	Kidinfo*	pks;
	Dimen	wd;
	Option*	option;
	Table*	tab;
	Tablecell*	c;
	Tablerow*	tr;
	Formfield*	field;
	Formfield*	ff;
	Rune*	href;
	Rune*	src;
	Rune*	scriptsrc;
	Rune*	bgurl;
	Rune*	action;
	Background	bg;

	if(!buildinited)
		buildinit();
	doscripts = 0;	/* for now */
	ps = is->psstk;
	curtab = is->tabstk;
	di = is->doc;
	toks = _gettoks(data, datalen, di->chset, di->mediatype, &tokslen);
	toki = 0;
	for(; toki < tokslen; toki++) {
		tok = &toks[toki];
		if(dbgbuild > 1)
			fprint(2, "build: curstate %ux, token %T\n", ps->curstate, tok);
		tag = tok->tag;
		brk = 0;
		brksp = 0;
		if(tag < Numtags) {
			brk = blockbrk[tag];
			if(brk&SPBefore)
				brksp = 1;
		}
		else if(tag < Numtags + RBRA) {
			brk = blockbrk[tag - RBRA];
			if(brk&SPAfter)
				brksp = 1;
		}
		if(brk) {
			addbrk(ps, brksp, 0);
			if(ps->inpar) {
				popjust(ps);
				ps->inpar = 0;
			}
		}
		/* check common case first (Data), then switch statement on tag */
		if(tag == Data) {
			/* Lexing didn't pay attention to SGML record boundary rules: */
			/* \n after start tag or before end tag to be discarded. */
			/* (Lex has already discarded all \r's). */
			/* Some pages assume this doesn't happen in <PRE> text, */
			/* so we won't do it if literal is true. */
			/* BUG: won't discard \n before a start tag that begins */
			/* the next bufferful of tokens. */
			s = tok->text;
			n = _Strlen(s);
			if(!ps->literal) {
				i = 0;
				j = n;
				if(toki > 0) {
					pt = toks[toki - 1].tag;
					/* IE and Netscape both ignore this rule (contrary to spec) */
					/* if previous tag was img */
					if(pt < Numtags && pt != Timg && j > 0 && s[0] == '\n')
						i++;
				}
				if(toki < tokslen - 1) {
					nt = toks[toki + 1].tag;
					if(nt >= RBRA && nt < Numtags + RBRA && j > i && s[j - 1] == '\n')
						j--;
				}
				if(i > 0 || j < n) {
					t = s;
					s = _Strsubstr(s, i, j);
					free(t);
					n = j-i;
				}
			}
			if(ps->skipwhite) {
				_trimwhite(s, n, &t, &nt);
				if(t == nil) {
					free(s);
					s = nil;
				}
				else if(t != s) {
					t = _Strndup(t, nt);
					free(s);
					s = t;
				}
				if(s != nil)
					ps->skipwhite = 0;
			}
			tok->text = nil;		/* token doesn't own string anymore */
			if(s != nil){
				addtext(ps, s);
				s = nil;
			}
		}
		else
			switch(tag) {
			/* Some abbrevs used in following DTD comments */
			/* %text = 	#PCDATA */
			/*		| TT | I | B | U | STRIKE | BIG | SMALL | SUB | SUP */
			/*		| EM | STRONG | DFN | CODE | SAMP | KBD | VAR | CITE */
			/*		| A | IMG | APPLET | FONT | BASEFONT | BR | SCRIPT | MAP */
			/*		| INPUT | SELECT | TEXTAREA */
			/* %block = P | UL | OL | DIR | MENU | DL | PRE | DL | DIV | CENTER */
			/*		| BLOCKQUOTE | FORM | ISINDEX | HR | TABLE */
			/* %flow = (%text | %block)* */
			/* %body.content = (%heading | %text | %block | ADDRESS)* */

			/* <!ELEMENT A - - (%text) -(A)> */
			/* Anchors are not supposed to be nested, but you sometimes see */
			/* href anchors inside destination anchors. */
			case Ta:
				if(ps->curanchor != 0) {
					if(warn)
						fprint(2, "warning: nested <A> or missing </A>\n");
					ps->curanchor = 0;
				}
				name = aval(tok, Aname);
				href = aurlval(tok, Ahref, nil, di->base);
				/* ignore rel, rev, and title attrs */
				if(href != nil) {
					target = atargval(tok, di->target);
					di->anchors = newanchor(++is->nanchors, name, href, target, di->anchors);
					if(name != nil)
						name = _Strdup(name);	/* for DestAnchor construction, below */
					ps->curanchor = is->nanchors;
					ps->curfg = push(&ps->fgstk, di->link);
					ps->curul = push(&ps->ulstk, ULunder);
				}
				if(name != nil) {
					/* add a null item to be destination */
					additem(ps, newispacer(ISPnull), tok);
					di->dests = newdestanchor(++is->nanchors, name, ps->lastit, di->dests);
				}
				break;

			case Ta+RBRA :
				if(ps->curanchor != 0) {
					ps->curfg = popretnewtop(&ps->fgstk, di->text);
					ps->curul = popretnewtop(&ps->ulstk, ULnone);
					ps->curanchor = 0;
				}
				break;

			/* <!ELEMENT APPLET - - (PARAM | %text)* > */
			/* We can't do applets, so ignore PARAMS, and let */
			/* the %text contents appear for the alternative rep */
			case Tapplet:
			case Tapplet+RBRA:
				if(warn && tag == Tapplet)
					fprint(2, "warning: <APPLET> ignored\n");
				break;

			/* <!ELEMENT AREA - O EMPTY> */
			case Tarea:
				map = di->maps;
				if(map == nil) {
					if(warn)
						fprint(2, "warning: <AREA> not inside <MAP>\n");
					continue;
				}
				map->areas = newarea(atabval(tok, Ashape, shape_tab, NSHAPETAB, SHrect),
					aurlval(tok, Ahref, nil, di->base),
					atargval(tok, di->target),
					map->areas);
				setdimarray(tok, Acoords, &map->areas->coords, &map->areas->ncoords);
				break;

			/* <!ELEMENT (B|STRONG) - - (%text)*> */
			case Tb:
			case Tstrong:
				pushfontstyle(ps, FntB);
				break;

			case Tb+RBRA:
			case Tcite+RBRA:
			case Tcode+RBRA:
			case Tdfn+RBRA:
			case Tem+RBRA:
			case Tkbd+RBRA:
			case Ti+RBRA:
			case Tsamp+RBRA:
			case Tstrong+RBRA:
			case Ttt+RBRA:
			case Tvar+RBRA :
			case Taddress+RBRA:
				popfontstyle(ps);
				break;

			/* <!ELEMENT BASE - O EMPTY> */
			case Tbase:
				t = di->base;
				di->base = aurlval(tok, Ahref, di->base, di->base);
				if(t != nil)
					free(t);
				di->target = atargval(tok, di->target);
				break;

			/* <!ELEMENT BASEFONT - O EMPTY> */
			case Tbasefont:
				ps->adjsize = aintval(tok, Asize, 3) - 3;
				break;

			/* <!ELEMENT (BIG|SMALL) - - (%text)*> */
			case Tbig:
			case Tsmall:
				sz = ps->adjsize;
				if(tag == Tbig)
					sz += Large;
				else
					sz += Small;
				pushfontsize(ps, sz);
				break;

			case Tbig+RBRA:
			case Tsmall+RBRA:
				popfontsize(ps);
				break;

			/* <!ELEMENT BLOCKQUOTE - - %body.content> */
			case Tblockquote:
				changeindent(ps, BQTAB);
				break;

			case Tblockquote+RBRA:
				changeindent(ps, -BQTAB);
				break;

			/* <!ELEMENT BODY O O %body.content> */
			case Tbody:
				ps->skipping = 0;
				bg = makebackground(nil, acolorval(tok, Abgcolor, di->background.color));
				bgurl = aurlval(tok, Abackground, nil, di->base);
				if(bgurl != nil) {
					if(di->backgrounditem != nil)
						freeitem((Item*)di->backgrounditem);
						/* really should remove old item from di->images list, */
						/* but there should only be one BODY element ... */
					di->backgrounditem = (Iimage*)newiimage(bgurl, nil, ALnone, 0, 0, 0, 0, 0, 0, nil);
					di->backgrounditem->nextimage = di->images;
					di->images = di->backgrounditem;
				}
				ps->curbg = bg;
				di->background = bg;
				di->text = acolorval(tok, Atext, di->text);
				di->link = acolorval(tok, Alink, di->link);
				di->vlink = acolorval(tok, Avlink, di->vlink);
				di->alink = acolorval(tok, Aalink, di->alink);
				if(di->text != ps->curfg) {
					ps->curfg = di->text;
					ps->fgstk.n = 0;
				}
				break;

			case Tbody+RBRA:
				/* HTML spec says ignore things after </body>, */
				/* but IE and Netscape don't */
				/* ps.skipping = 1; */
				break;

			/* <!ELEMENT BR - O EMPTY> */
			case Tbr:
				addlinebrk(ps, atabval(tok, Aclear, clear_tab, NCLEARTAB, 0));
				break;

			/* <!ELEMENT CAPTION - - (%text;)*> */
			case Tcaption:
				if(curtab == nil) {
					if(warn)
						fprint(2, "warning: <CAPTION> outside <TABLE>\n");
					continue;
				}
				if(curtab->caption != nil) {
					if(warn)
						fprint(2, "warning: more than one <CAPTION> in <TABLE>\n");
					continue;
				}
				ps = newpstate(ps);
				curtab->caption_place = atabval(tok, Aalign, align_tab, NALIGNTAB, ALtop);
				break;

			case Tcaption+RBRA:
				nextps = ps->next;
				if(curtab == nil || nextps == nil) {
					if(warn)
						fprint(2, "warning: unexpected </CAPTION>\n");
					continue;
				}
				curtab->caption = ps->items->next;
				free(ps);
				ps = nextps;
				break;

			case Tcenter:
			case Tdiv:
				if(tag == Tcenter)
					al = ALcenter;
				else
					al = atabval(tok, Aalign, align_tab, NALIGNTAB, ps->curjust);
				pushjust(ps, al);
				break;

			case Tcenter+RBRA:
			case Tdiv+RBRA:
				popjust(ps);
				break;

			/* <!ELEMENT DD - O  %flow > */
			case Tdd:
				if(ps->hangstk.n == 0) {
					if(warn)
						fprint(2, "warning: <DD> not inside <DL\n");
					continue;
				}
				h = top(&ps->hangstk, 0);
				if(h != 0)
					changehang(ps, -10*LISTTAB);
				else
					addbrk(ps, 0, 0);
				push(&ps->hangstk, 0);
				break;

			/*<!ELEMENT (DIR|MENU) - - (LI)+ -(%block) > */
			/*<!ELEMENT (OL|UL) - - (LI)+> */
			case Tdir:
			case Tmenu:
			case Tol:
			case Tul:
				changeindent(ps, LISTTAB);
				push(&ps->listtypestk, listtyval(tok, (tag==Tol)? LT1 : LTdisc));
				push(&ps->listcntstk, aintval(tok, Astart, 1));
				break;

			case Tdir+RBRA:
			case Tmenu+RBRA:
			case Tol+RBRA:
			case Tul+RBRA:
				if(ps->listtypestk.n == 0) {
					if(warn)
						fprint(2, "warning: %T ended no list\n", tok);
					continue;
				}
				addbrk(ps, 0, 0);
				pop(&ps->listtypestk);
				pop(&ps->listcntstk);
				changeindent(ps, -LISTTAB);
				break;

			/* <!ELEMENT DL - - (DT|DD)+ > */
			case Tdl:
				changeindent(ps, LISTTAB);
				push(&ps->hangstk, 0);
				break;

			case Tdl+RBRA:
				if(ps->hangstk.n == 0) {
					if(warn)
						fprint(2, "warning: unexpected </DL>\n");
					continue;
				}
				changeindent(ps, -LISTTAB);
				if(top(&ps->hangstk, 0) != 0)
					changehang(ps, -10*LISTTAB);
				pop(&ps->hangstk);
				break;

			/* <!ELEMENT DT - O (%text)* > */
			case Tdt:
				if(ps->hangstk.n == 0) {
					if(warn)
						fprint(2, "warning: <DT> not inside <DL>\n");
					continue;
				}
				h = top(&ps->hangstk, 0);
				pop(&ps->hangstk);
				if(h != 0)
					changehang(ps, -10*LISTTAB);
				changehang(ps, 10*LISTTAB);
				push(&ps->hangstk, 1);
				break;

			/* <!ELEMENT FONT - - (%text)*> */
			case Tfont:
				sz = top(&ps->fntsizestk, Normal);
				if(_tokaval(tok, Asize, &nsz, 0)) {
					if(_prefix(L(Lplus), nsz))
						sz = Normal + _Strtol(nsz+1, nil, 10) + ps->adjsize;
					else if(_prefix(L(Lminus), nsz))
						sz = Normal - _Strtol(nsz+1, nil, 10) + ps->adjsize;
					else if(nsz != nil)
						sz = Normal + (_Strtol(nsz, nil, 10) - 3);
				}
				ps->curfg = push(&ps->fgstk, acolorval(tok, Acolor, ps->curfg));
				pushfontsize(ps, sz);
				break;

			case Tfont+RBRA:
				if(ps->fgstk.n == 0) {
					if(warn)
						fprint(2, "warning: unexpected </FONT>\n");
					continue;
				}
				ps->curfg = popretnewtop(&ps->fgstk, di->text);
				popfontsize(ps);
				break;

			/* <!ELEMENT FORM - - %body.content -(FORM) > */
			case Tform:
				if(is->curform != nil) {
					if(warn)
						fprint(2, "warning: <FORM> nested inside another\n");
					continue;
				}
				action = aurlval(tok, Aaction, di->base, di->base);
				s = aval(tok, Aid);
				name = astrval(tok, Aname, s);
				if(s)
					free(s);
				target = atargval(tok, di->target);
				method = atabval(tok, Amethod, method_tab, NMETHODTAB, HGet);
				if(warn && _tokaval(tok, Aenctype, &enctype, 0) &&
						_Strcmp(enctype, L(Lappl_form)))
					fprint(2, "form enctype %S not handled\n", enctype);
				frm = newform(++is->nforms, name, action, target, method, di->forms);
				di->forms = frm;
				is->curform = frm;
				break;

			case Tform+RBRA:
				if(is->curform == nil) {
					if(warn)
						fprint(2, "warning: unexpected </FORM>\n");
					continue;
				}
				/* put fields back in input order */
				is->curform->fields = (Formfield*)_revlist((List*)is->curform->fields);
				is->curform = nil;
				break;

			/* <!ELEMENT FRAME - O EMPTY> */
			case Tframe:
				ks = is->kidstk;
				if(ks == nil) {
					if(warn)
						fprint(2, "warning: <FRAME> not in <FRAMESET>\n");
					continue;
				}
				ks->kidinfos = kd = newkidinfo(0, ks->kidinfos);
				kd->src = aurlval(tok, Asrc, nil, di->base);
				kd->name = aval(tok, Aname);
				if(kd->name == nil) {
					s = _ltoStr(++is->nframes);
					kd->name = _Strdup2(L(Lfr), s);
					free(s);
				}
				kd->marginw = auintval(tok, Amarginwidth, 0);
				kd->marginh = auintval(tok, Amarginheight, 0);
				kd->framebd = auintval(tok, Aframeborder, 1);
				kd->flags = atabval(tok, Ascrolling, fscroll_tab, NFSCROLLTAB, kd->flags);
				norsz = aflagval(tok, Anoresize);
				if(norsz)
					kd->flags |= FRnoresize;
				break;

			/* <!ELEMENT FRAMESET - - (FRAME|FRAMESET)+> */
			case Tframeset:
				ks = newkidinfo(1, nil);
				pks = is->kidstk;
				if(pks == nil)
					di->kidinfo = ks;
				else  {
					ks->next = pks->kidinfos;
					pks->kidinfos = ks;
				}
				ks->nextframeset = pks;
				is->kidstk = ks;
				setdimarray(tok, Arows, &ks->rows, &ks->nrows);
				if(ks->nrows == 0) {
					ks->rows = (Dimen*)emalloc(sizeof(Dimen));
					ks->nrows = 1;
					ks->rows[0] = makedimen(Dpercent, 100);
				}
				setdimarray(tok, Acols, &ks->cols, &ks->ncols);
				if(ks->ncols == 0) {
					ks->cols = (Dimen*)emalloc(sizeof(Dimen));
					ks->ncols = 1;
					ks->cols[0] = makedimen(Dpercent, 100);
				}
				break;

			case Tframeset+RBRA:
				if(is->kidstk == nil) {
					if(warn)
						fprint(2, "warning: unexpected </FRAMESET>\n");
					continue;
				}
				ks = is->kidstk;
				/* put kids back in original order */
				/* and add blank frames to fill out cells */
				n = ks->nrows*ks->ncols;
				nblank = n - _listlen((List*)ks->kidinfos);
				while(nblank-- > 0)
					ks->kidinfos = newkidinfo(0, ks->kidinfos);
				ks->kidinfos = (Kidinfo*)_revlist((List*)ks->kidinfos);
				is->kidstk = is->kidstk->nextframeset;
				if(is->kidstk == nil) {
					/* end input */
					ans = nil;
					goto return_ans;
				}
				break;

			/* <!ELEMENT H1 - - (%text;)*>, etc. */
			case Th1:
			case Th2:
			case Th3:
			case Th4:
			case Th5:
			case Th6:
				bramt = 1;
				if(ps->items == ps->lastit)
					bramt = 0;
				addbrk(ps, bramt, IFcleft|IFcright);
				sz = Verylarge - (tag - Th1);
				if(sz < Tiny)
					sz = Tiny;
				pushfontsize(ps, sz);
				sty = top(&ps->fntstylestk, FntR);
				if(tag == Th1)
					sty = FntB;
				pushfontstyle(ps, sty);
				pushjust(ps, atabval(tok, Aalign, align_tab, NALIGNTAB, ps->curjust));
				ps->skipwhite = 1;
				break;

			case Th1+RBRA:
			case Th2+RBRA:
			case Th3+RBRA:
			case Th4+RBRA:
			case Th5+RBRA:
			case Th6+RBRA:
				addbrk(ps, 1, IFcleft|IFcright);
				popfontsize(ps);
				popfontstyle(ps);
				popjust(ps);
				break;

			case Thead:
				/* HTML spec says ignore regular markup in head, */
				/* but Netscape and IE don't */
				/* ps.skipping = 1; */
				break;

			case Thead+RBRA:
				ps->skipping = 0;
				break;

			/* <!ELEMENT HR - O EMPTY> */
			case Thr:
				al = atabval(tok, Aalign, align_tab, NALIGNTAB, ALcenter);
				sz = auintval(tok, Asize, HRSZ);
				wd = adimen(tok, Awidth);
				if(dimenkind(wd) == Dnone)
					wd = makedimen(Dpercent, 100);
				nosh = aflagval(tok, Anoshade);
				additem(ps, newirule(al, sz, nosh, wd), tok);
				addbrk(ps, 0, 0);
				break;

			case Ti:
			case Tcite:
			case Tdfn:
			case Tem:
			case Tvar:
			case Taddress:
				pushfontstyle(ps, FntI);
				break;

			/* <!ELEMENT IMG - O EMPTY> */
			case Timg:
				map = nil;
				oldcuranchor = ps->curanchor;
				if(_tokaval(tok, Ausemap, &usemap, 0)) {
					if(!_prefix(L(Lhash), usemap)) {
						if(warn)
							fprint(2, "warning: can't handle non-local map %S\n", usemap);
					}
					else {
						map = getmap(di, usemap+1);
						if(ps->curanchor == 0) {
							di->anchors = newanchor(++is->nanchors, nil, nil, di->target, di->anchors);
							ps->curanchor = is->nanchors;
						}
					}
				}
				align = atabval(tok, Aalign, align_tab, NALIGNTAB, ALbottom);
				dfltbd = 0;
				if(ps->curanchor != 0)
					dfltbd = 2;
				src = aurlval(tok, Asrc, nil, di->base);
				if(src == nil) {
					if(warn)
						fprint(2, "warning: <img> has no src attribute\n");
					ps->curanchor = oldcuranchor;
					continue;
				}
				img = newiimage(src,
						aval(tok, Aalt),
						align,
						auintval(tok, Awidth, 0),
						auintval(tok, Aheight, 0),
						auintval(tok, Ahspace, IMGHSPACE),
						auintval(tok, Avspace, IMGVSPACE),
						auintval(tok, Aborder, dfltbd),
						aflagval(tok, Aismap),
						map);
				if(align == ALleft || align == ALright) {
					additem(ps, newifloat(img, align), tok);
					/* if no hspace specified, use FLTIMGHSPACE */
					if(!_tokaval(tok, Ahspace, &val, 0))
						((Iimage*)img)->hspace = FLTIMGHSPACE;
				}
				else {
					ps->skipwhite = 0;
					additem(ps, img, tok);
				}
				if(!ps->skipping) {
					((Iimage*)img)->nextimage = di->images;
					di->images = (Iimage*)img;
				}
				ps->curanchor = oldcuranchor;
				break;

			/* <!ELEMENT INPUT - O EMPTY> */
			case Tinput:
				ps->skipwhite = 0;
				if(is->curform == nil) {
					if(warn)
						fprint(2, "<INPUT> not inside <FORM>\n");
					continue;
				}
				is->curform->fields = field = newformfield(
						atabval(tok, Atype, input_tab, NINPUTTAB, Ftext),
						++is->curform->nfields,
						is->curform,
						aval(tok, Aname),
						aval(tok, Avalue),
						auintval(tok, Asize, 0),
						auintval(tok, Amaxlength, 1000),
						is->curform->fields);
				if(aflagval(tok, Achecked))
					field->flags = FFchecked;

				switch(field->ftype) {
				case Ftext:
				case Fpassword:
				case Ffile:
					if(field->size == 0)
						field->size = 20;
					break;

				case Fcheckbox:
					if(field->name == nil) {
						if(warn)
							fprint(2, "warning: checkbox form field missing name\n");
						continue;
					}
					if(field->value == nil)
						field->value = _Strdup(L(Lone));
					break;

				case Fradio:
					if(field->name == nil || field->value == nil) {
						if(warn)
							fprint(2, "warning: radio form field missing name or value\n");
						continue;
					}
					break;

				case Fsubmit:
					if(field->value == nil)
						field->value = _Strdup(L(Lsubmit));
					if(field->name == nil)
						field->name = _Strdup(L(Lnoname));
					break;

				case Fimage:
					src = aurlval(tok, Asrc, nil, di->base);
					if(src == nil) {
						if(warn)
							fprint(2, "warning: image form field missing src\n");
						continue;
					}
					/* width and height attrs aren't specified in HTML 3.2, */
					/* but some people provide them and they help avoid */
					/* a relayout */
					field->image = newiimage(src,
						astrval(tok, Aalt, L(Lsubmit)),
						atabval(tok, Aalign, align_tab, NALIGNTAB, ALbottom),
						auintval(tok, Awidth, 0), auintval(tok, Aheight, 0),
						0, 0, 0, 0, nil);
					ii = (Iimage*)field->image;
					ii->nextimage = di->images;
					di->images = ii;
					break;

				case Freset:
					if(field->value == nil)
						field->value = _Strdup(L(Lreset));
					break;

				case Fbutton:
					if(field->value == nil)
						field->value = _Strdup(L(Lspace));
					break;
				}
				ffit = newiformfield(field);
				additem(ps, ffit, tok);
				if(ffit->genattr != nil)
					field->events = ffit->genattr->events;
				break;

			/* <!ENTITY ISINDEX - O EMPTY> */
			case Tisindex:
				ps->skipwhite = 0;
				prompt = astrval(tok, Aprompt, L(Lindex));
				target = atargval(tok, di->target);
				additem(ps, textit(ps, prompt), tok);
				frm = newform(++is->nforms,
						nil,
						di->base,
						target,
						HGet,
						di->forms);
				di->forms = frm;
				ff = newformfield(Ftext,
						1,
						frm,
						_Strdup(L(Lisindex)),
						nil,
						50,
						1000,
						nil);
				frm->fields = ff;
				frm->nfields = 1;
				additem(ps, newiformfield(ff), tok);
				addbrk(ps, 1, 0);
				break;

			/* <!ELEMENT LI - O %flow> */
			case Tli:
				if(ps->listtypestk.n == 0) {
					if(warn)
						fprint(2, "<LI> not in list\n");
					continue;
				}
				ty = top(&ps->listtypestk, 0);
				ty2 = listtyval(tok, ty);
				if(ty != ty2) {
					ty = ty2;
					push(&ps->listtypestk, ty2);
				}
				v = aintval(tok, Avalue, top(&ps->listcntstk, 1));
				if(ty == LTdisc || ty == LTsquare || ty == LTcircle)
					hang = 10*LISTTAB - 3;
				else
					hang = 10*LISTTAB - 1;
				changehang(ps, hang);
				addtext(ps, listmark(ty, v));
				push(&ps->listcntstk, v + 1);
				changehang(ps, -hang);
				ps->skipwhite = 1;
				break;

			/* <!ELEMENT MAP - - (AREA)+> */
			case Tmap:
				if(_tokaval(tok, Aname, &name, 0))
					is->curmap = getmap(di, name);
				break;

			case Tmap+RBRA:
				map = is->curmap;
				if(map == nil) {
					if(warn)
						fprint(2, "warning: unexpected </MAP>\n");
					continue;
				}
				map->areas = (Area*)_revlist((List*)map->areas);
				break;

			case Tmeta:
				if(ps->skipping)
					continue;
				if(_tokaval(tok, Ahttp_equiv, &equiv, 0)) {
					val = aval(tok, Acontent);
					n = _Strlen(equiv);
					if(!_Strncmpci(equiv, n, L(Lrefresh)))
						di->refresh = val;
					else if(!_Strncmpci(equiv, n, L(Lcontent))) {
						n = _Strlen(val);
						if(!_Strncmpci(val, n, L(Ljavascript))
						   || !_Strncmpci(val, n, L(Ljscript1))
						   || !_Strncmpci(val, n, L(Ljscript)))
							di->scripttype = TextJavascript;
						else {
							if(warn)
								fprint(2, "unimplemented script type %S\n", val);
							di->scripttype = UnknownType;
						}
					}
				}
				break;

			/* Nobr is NOT in HMTL 4.0, but it is ubiquitous on the web */
			case Tnobr:
				ps->skipwhite = 0;
				ps->curstate &= ~IFwrap;
				break;

			case Tnobr+RBRA:
				ps->curstate |= IFwrap;
				break;

			/* We do frames, so skip stuff in noframes */
			case Tnoframes:
				ps->skipping = 1;
				break;

			case Tnoframes+RBRA:
				ps->skipping = 0;
				break;

			/* We do scripts (if enabled), so skip stuff in noscripts */
			case Tnoscript:
				if(doscripts)
					ps->skipping = 1;
				break;

			case Tnoscript+RBRA:
				if(doscripts)
					ps->skipping = 0;
				break;

			/* <!ELEMENT OPTION - O (	//PCDATA)> */
			case Toption:
				if(is->curform == nil || is->curform->fields == nil) {
					if(warn)
						fprint(2, "warning: <OPTION> not in <SELECT>\n");
					continue;
				}
				field = is->curform->fields;
				if(field->ftype != Fselect) {
					if(warn)
						fprint(2, "warning: <OPTION> not in <SELECT>\n");
					continue;
				}
				val = aval(tok, Avalue);
				option = newoption(aflagval(tok, Aselected), val, nil, field->options);
				field->options = option;
				option->display =  getpcdata(toks, tokslen, &toki);
				if(val == nil)
					option->value = _Strdup(option->display);
				break;

			/* <!ELEMENT P - O (%text)* > */
			case Tp:
				pushjust(ps, atabval(tok, Aalign, align_tab, NALIGNTAB, ps->curjust));
				ps->inpar = 1;
				ps->skipwhite = 1;
				break;

			case Tp+RBRA:
				break;

			/* <!ELEMENT PARAM - O EMPTY> */
			/* Do something when we do applets... */
			case Tparam:
				break;

			/* <!ELEMENT PRE - - (%text)* -(IMG|BIG|SMALL|SUB|SUP|FONT) > */
			case Tpre:
				ps->curstate &= ~IFwrap;
				ps->literal = 1;
				ps->skipwhite = 0;
				pushfontstyle(ps, FntT);
				break;

			case Tpre+RBRA:
				ps->curstate |= IFwrap;
				if(ps->literal) {
					popfontstyle(ps);
					ps->literal = 0;
				}
				break;

			/* <!ELEMENT SCRIPT - - CDATA> */
			case Tscript:
				if(doscripts) {
					if(!di->hasscripts) {
						if(di->scripttype == TextJavascript) {
							/* TODO: initialize script if nec. */
							/* initjscript(di); */
							di->hasscripts = 1;
						}
					}
				}
				if(!di->hasscripts) {
					if(warn)
						fprint(2, "warning: <SCRIPT> ignored\n");
					ps->skipping = 1;
				}
				else {
					scriptsrc = aurlval(tok, Asrc, nil, di->base);
					script = nil;
					if(scriptsrc != nil) {
						if(warn)
							fprint(2, "warning: non-local <SCRIPT> ignored\n");
						free(scriptsrc);
					}
					else {
						script = getpcdata(toks, tokslen, &toki);
					}
					if(script != nil) {
						if(warn)
							fprint(2, "script ignored\n");
						free(script);
					}
				}
				break;

			case Tscript+RBRA:
				ps->skipping = 0;
				break;

			/* <!ELEMENT SELECT - - (OPTION+)> */
			case Tselect:
				if(is->curform == nil) {
					if(warn)
						fprint(2, "<SELECT> not inside <FORM>\n");
					continue;
				}
				field = newformfield(Fselect,
					++is->curform->nfields,
					is->curform,
					aval(tok, Aname),
					nil,
					auintval(tok, Asize, 0),
					0,
					is->curform->fields);
				is->curform->fields = field;
				if(aflagval(tok, Amultiple))
					field->flags = FFmultiple;
				ffit = newiformfield(field);
				additem(ps, ffit, tok);
				if(ffit->genattr != nil)
					field->events = ffit->genattr->events;
				/* throw away stuff until next tag (should be <OPTION>) */
				s = getpcdata(toks, tokslen, &toki);
				if(s != nil)
					free(s);
				break;

			case Tselect+RBRA:
				if(is->curform == nil || is->curform->fields == nil) {
					if(warn)
						fprint(2, "warning: unexpected </SELECT>\n");
					continue;
				}
				field = is->curform->fields;
				if(field->ftype != Fselect)
					continue;
				/* put options back in input order */
				field->options = (Option*)_revlist((List*)field->options);
				break;

			/* <!ELEMENT (STRIKE|U) - - (%text)*> */
			case Tstrike:
			case Tu:
				ps->curul = push(&ps->ulstk, (tag==Tstrike)? ULmid : ULunder);
				break;

			case Tstrike+RBRA:
			case Tu+RBRA:
				if(ps->ulstk.n == 0) {
					if(warn)
						fprint(2, "warning: unexpected %T\n", tok);
					continue;
				}
				ps->curul = popretnewtop(&ps->ulstk, ULnone);
				break;

			/* <!ELEMENT STYLE - - CDATA> */
			case Tstyle:
				if(warn)
					fprint(2, "warning: unimplemented <STYLE>\n");
				ps->skipping = 1;
				break;

			case Tstyle+RBRA:
				ps->skipping = 0;
				break;

			/* <!ELEMENT (SUB|SUP) - - (%text)*> */
			case Tsub:
			case Tsup:
				if(tag == Tsub)
					ps->curvoff += SUBOFF;
				else
					ps->curvoff -= SUPOFF;
				push(&ps->voffstk, ps->curvoff);
				sz = top(&ps->fntsizestk, Normal);
				pushfontsize(ps, sz - 1);
				break;

			case Tsub+RBRA:
			case Tsup+RBRA:
				if(ps->voffstk.n == 0) {
					if(warn)
						fprint(2, "warning: unexpected %T\n", tok);
					continue;
				}
				ps->curvoff = popretnewtop(&ps->voffstk, 0);
				popfontsize(ps);
				break;

			/* <!ELEMENT TABLE - - (CAPTION?, TR+)> */
			case Ttable:
				ps->skipwhite = 0;
				tab = newtable(++is->ntables,
						aalign(tok),
						adimen(tok, Awidth),
						aflagval(tok, Aborder),
						auintval(tok, Acellspacing, TABSP),
						auintval(tok, Acellpadding, TABPAD),
						makebackground(nil, acolorval(tok, Abgcolor, ps->curbg.color)),
						tok,
						is->tabstk);
				is->tabstk = tab;
				curtab = tab;
				break;

			case Ttable+RBRA:
				if(curtab == nil) {
					if(warn)
						fprint(2, "warning: unexpected </TABLE>\n");
					continue;
				}
				isempty = (curtab->cells == nil);
				if(isempty) {
					if(warn)
						fprint(2, "warning: <TABLE> has no cells\n");
				}
				else {
					ps = finishcell(curtab, ps);
					if(curtab->rows != nil)
						curtab->rows->flags = 0;
					finish_table(curtab);
				}
				ps->skipping = 0;
				if(!isempty) {
					tabitem = newitable(curtab);
					al = curtab->align.halign;
					switch(al) {
					case ALleft:
					case ALright:
						additem(ps, newifloat(tabitem, al), tok);
						break;
					default:
						if(al == ALcenter)
							pushjust(ps, ALcenter);
						addbrk(ps, 0, 0);
						if(ps->inpar) {
							popjust(ps);
							ps->inpar = 0;
						}
						additem(ps, tabitem, curtab->tabletok);
						if(al == ALcenter)
							popjust(ps);
						break;
					}
				}
				if(is->tabstk == nil) {
					if(warn)
						fprint(2, "warning: table stack is wrong\n");
				}
				else
					is->tabstk = is->tabstk->next;
				curtab->next = di->tables;
				di->tables = curtab;
				curtab = is->tabstk;
				if(!isempty)
					addbrk(ps, 0, 0);
				break;

			/* <!ELEMENT (TH|TD) - O %body.content> */
			/* Cells for a row are accumulated in reverse order. */
			/* We push ps on a stack, and use a new one to accumulate */
			/* the contents of the cell. */
			case Ttd:
			case Tth:
				if(curtab == nil) {
					if(warn)
						fprint(2, "%T outside <TABLE>\n", tok);
					continue;
				}
				if(ps->inpar) {
					popjust(ps);
					ps->inpar = 0;
				}
				ps = finishcell(curtab, ps);
				tr = nil;
				if(curtab->rows != nil)
					tr = curtab->rows;
				if(tr == nil || !tr->flags) {
					if(warn)
						fprint(2, "%T outside row\n", tok);
					tr = newtablerow(makealign(ALnone, ALnone),
							makebackground(nil, curtab->background.color),
							TFparsing,
							curtab->rows);
					curtab->rows = tr;
				}
				ps = cell_pstate(ps, tag == Tth);
				flags = TFparsing;
				if(aflagval(tok, Anowrap)) {
					flags |= TFnowrap;
					ps->curstate &= ~IFwrap;
				}
				if(tag == Tth)
					flags |= TFisth;
				c = newtablecell(curtab->cells==nil? 1 : curtab->cells->cellid+1,
						auintval(tok, Arowspan, 1),
						auintval(tok, Acolspan, 1),
						aalign(tok),
						adimen(tok, Awidth),
						auintval(tok, Aheight, 0),
						makebackground(nil, acolorval(tok, Abgcolor, tr->background.color)),
						flags,
						curtab->cells);
				curtab->cells = c;
				ps->curbg = c->background;
				if(c->align.halign == ALnone) {
					if(tr->align.halign != ALnone)
						c->align.halign = tr->align.halign;
					else if(tag == Tth)
						c->align.halign = ALcenter;
					else
						c->align.halign = ALleft;
				}
				if(c->align.valign == ALnone) {
					if(tr->align.valign != ALnone)
						c->align.valign = tr->align.valign;
					else
						c->align.valign = ALmiddle;
				}
				c->nextinrow = tr->cells;
				tr->cells = c;
				break;

			case Ttd+RBRA:
			case Tth+RBRA:
				if(curtab == nil || curtab->cells == nil) {
					if(warn)
						fprint(2, "unexpected %T\n", tok);
					continue;
				}
				ps = finishcell(curtab, ps);
				break;

			/* <!ELEMENT TEXTAREA - - (	//PCDATA)> */
			case Ttextarea:
				if(is->curform == nil) {
					if(warn)
						fprint(2, "<TEXTAREA> not inside <FORM>\n");
					continue;
				}
				field = newformfield(Ftextarea,
					++is->curform->nfields,
					is->curform,
					aval(tok, Aname),
					nil,
					0,
					0,
					is->curform->fields);
				is->curform->fields = field;
				field->rows = auintval(tok, Arows, 3);
				field->cols = auintval(tok, Acols, 50);
				field->value = getpcdata(toks, tokslen, &toki);
				if(warn && toki < tokslen - 1 && toks[toki + 1].tag != Ttextarea + RBRA)
					fprint(2, "warning: <TEXTAREA> data ended by %T\n", &toks[toki + 1]);
				ffit = newiformfield(field);
				additem(ps, ffit, tok);
				if(ffit->genattr != nil)
					field->events = ffit->genattr->events;
				break;

			/* <!ELEMENT TITLE - - (	//PCDATA)* -(%head.misc)> */
			case Ttitle:
				di->doctitle = getpcdata(toks, tokslen, &toki);
				if(warn && toki < tokslen - 1 && toks[toki + 1].tag != Ttitle + RBRA)
					fprint(2, "warning: <TITLE> data ended by %T\n", &toks[toki + 1]);
				break;

			/* <!ELEMENT TR - O (TH|TD)+> */
			/* rows are accumulated in reverse order in curtab->rows */
			case Ttr:
				if(curtab == nil) {
					if(warn)
						fprint(2, "warning: <TR> outside <TABLE>\n");
					continue;
				}
				if(ps->inpar) {
					popjust(ps);
					ps->inpar = 0;
				}
				ps = finishcell(curtab, ps);
				if(curtab->rows != nil)
					curtab->rows->flags = 0;
				curtab->rows = newtablerow(aalign(tok),
					makebackground(nil, acolorval(tok, Abgcolor, curtab->background.color)),
					TFparsing,
					curtab->rows);
				break;

			case Ttr+RBRA:
				if(curtab == nil || curtab->rows == nil) {
					if(warn)
						fprint(2, "warning: unexpected </TR>\n");
					continue;
				}
				ps = finishcell(curtab, ps);
				tr = curtab->rows;
				if(tr->cells == nil) {
					if(warn)
						fprint(2, "warning: empty row\n");
					curtab->rows = tr->next;
					tr->next = nil;
				}
				else
					tr->flags = 0;
				break;

			/* <!ELEMENT (TT|CODE|KBD|SAMP) - - (%text)*> */
			case Ttt:
			case Tcode:
			case Tkbd:
			case Tsamp:
				pushfontstyle(ps, FntT);
				break;

			/* Tags that have empty action */
			case Tabbr:
			case Tabbr+RBRA:
			case Tacronym:
			case Tacronym+RBRA:
			case Tarea+RBRA:
			case Tbase+RBRA:
			case Tbasefont+RBRA:
			case Tbr+RBRA:
			case Tdd+RBRA:
			case Tdt+RBRA:
			case Tframe+RBRA:
			case Thr+RBRA:
			case Thtml:
			case Thtml+RBRA:
			case Timg+RBRA:
			case Tinput+RBRA:
			case Tisindex+RBRA:
			case Tli+RBRA:
			case Tlink:
			case Tlink+RBRA:
			case Tmeta+RBRA:
			case Toption+RBRA:
			case Tparam+RBRA:
			case Ttextarea+RBRA:
			case Ttitle+RBRA:
				break;


			/* Tags not implemented */
			case Tbdo:
			case Tbdo+RBRA:
			case Tbutton:
			case Tbutton+RBRA:
			case Tdel:
			case Tdel+RBRA:
			case Tfieldset:
			case Tfieldset+RBRA:
			case Tiframe:
			case Tiframe+RBRA:
			case Tins:
			case Tins+RBRA:
			case Tlabel:
			case Tlabel+RBRA:
			case Tlegend:
			case Tlegend+RBRA:
			case Tobject:
			case Tobject+RBRA:
			case Toptgroup:
			case Toptgroup+RBRA:
			case Tspan:
			case Tspan+RBRA:
				if(warn) {
					if(tag > RBRA)
						tag -= RBRA;
					fprint(2, "warning: unimplemented HTML tag: %S\n", tagnames[tag]);
				}
				break;

			default:
				if(warn)
					fprint(2, "warning: unknown HTML tag: %S\n", tok->text);
				break;
			}
	}
	/* some pages omit trailing </table> */
	while(curtab != nil) {
		if(warn)
			fprint(2, "warning: <TABLE> not closed\n");
		if(curtab->cells != nil) {
			ps = finishcell(curtab, ps);
			if(curtab->cells == nil) {
				if(warn)
					fprint(2, "warning: empty table\n");
			}
			else {
				if(curtab->rows != nil)
					curtab->rows->flags = 0;
				finish_table(curtab);
				ps->skipping = 0;
				additem(ps, newitable(curtab), curtab->tabletok);
				addbrk(ps, 0, 0);
			}
		}
		if(is->tabstk != nil)
			is->tabstk = is->tabstk->next;
		curtab->next = di->tables;
		di->tables = curtab;
		curtab = is->tabstk;
	}
	outerps = lastps(ps);
	ans = outerps->items->next;
	/* note: ans may be nil and di->kids not nil, if there's a frameset! */
	freeitem(outerps->items);
	outerps->items = newispacer(ISPnull);
	outerps->lastit = outerps->items;
	is->psstk = ps;
	if(ans != nil && di->hasscripts) {
		/* TODO evalscript(nil); */
		;
	}
	freeitems(outerps->items);

return_ans:
	if(dbgbuild) {
		assert(validitems(ans));
		if(ans == nil)
			fprint(2, "getitems returning nil\n");
		else
			printitems(ans, "getitems returning:");
	}
	_freetokens(toks, tokslen);
	return ans;
}

/* Concatenate together maximal set of Data tokens, starting at toks[toki+1]. */
/* Lexer has ensured that there will either be a following non-data token or */
/* we will be at eof. */
/* Return emallocd trimmed concatenation, and update *ptoki to last used toki */
static Rune*
getpcdata(Token* toks, int tokslen, int* ptoki)
{
	Rune*	ans;
	Rune*	p;
	Rune*	trimans;
	int	anslen;
	int	trimanslen;
	int	toki;
	Token*	tok;

	ans = nil;
	anslen = 0;
	/* first find length of answer */
	toki = (*ptoki) + 1;
	while(toki < tokslen) {
		tok = &toks[toki];
		if(tok->tag == Data) {
			toki++;
			anslen += _Strlen(tok->text);
		}
		else
			break;
	}
	/* now make up the initial answer */
	if(anslen > 0) {
		ans = _newstr(anslen);
		p = ans;
		toki = (*ptoki) + 1;
		while(toki < tokslen) {
			tok = &toks[toki];
			if(tok->tag == Data) {
				toki++;
				p = _Stradd(p, tok->text, _Strlen(tok->text));
			}
			else
				break;
		}
		*p = 0;
		_trimwhite(ans, anslen, &trimans, &trimanslen);
		if(trimanslen != anslen) {
			p = ans;
			ans = _Strndup(trimans, trimanslen);
			free(p);
		}
	}
	*ptoki = toki-1;
	return ans;
}

/* If still parsing head of curtab->cells list, finish it off */
/* by transferring the items on the head of psstk to the cell. */
/* Then pop the psstk and return the new psstk. */
static Pstate*
finishcell(Table* curtab, Pstate* psstk)
{
	Tablecell*	c;
	Pstate* psstknext;

	c = curtab->cells;
	if(c != nil) {
		if((c->flags&TFparsing)) {
			psstknext = psstk->next;
			if(psstknext == nil) {
				if(warn)
					fprint(2, "warning: parse state stack is wrong\n");
			}
			else {
				c->content = psstk->items->next;
				c->flags &= ~TFparsing;
				freepstate(psstk);
				psstk = psstknext;
			}
		}
	}
	return psstk;
}

/* Make a new Pstate for a cell, based on the old pstate, oldps. */
/* Also, put the new ps on the head of the oldps stack. */
static Pstate*
cell_pstate(Pstate* oldps, int ishead)
{
	Pstate*	ps;
	int	sty;

	ps = newpstate(oldps);
	ps->skipwhite = 1;
	ps->curanchor = oldps->curanchor;
	copystack(&ps->fntstylestk, &oldps->fntstylestk);
	copystack(&ps->fntsizestk, &oldps->fntsizestk);
	ps->curfont = oldps->curfont;
	ps->curfg = oldps->curfg;
	ps->curbg = oldps->curbg;
	copystack(&ps->fgstk, &oldps->fgstk);
	ps->adjsize = oldps->adjsize;
	if(ishead) {
		sty = ps->curfont%NumSize;
		ps->curfont = FntB*NumSize + sty;
	}
	return ps;
}

/* Return a new Pstate with default starting state. */
/* Use link to add it to head of a list, if any. */
static Pstate*
newpstate(Pstate* link)
{
	Pstate*	ps;

	ps = (Pstate*)emalloc(sizeof(Pstate));
	ps->curfont = DefFnt;
	ps->curfg = Black;
	ps->curbg.image = nil;
	ps->curbg.color = White;
	ps->curul = ULnone;
	ps->curjust = ALleft;
	ps->curstate = IFwrap;
	ps->items = newispacer(ISPnull);
	ps->lastit = ps->items;
	ps->prelastit = nil;
	ps->next = link;
	return ps;
}

/* Return last Pstate on psl list */
static Pstate*
lastps(Pstate* psl)
{
	assert(psl != nil);
	while(psl->next != nil)
		psl = psl->next;
	return psl;
}

/* Add it to end of ps item chain, adding in current state from ps. */
/* Also, if tok is not nil, scan it for generic attributes and assign */
/* the genattr field of the item accordingly. */
static void
additem(Pstate* ps, Item* it, Token* tok)
{
	int	aid;
	int	any;
	Rune*	i;
	Rune*	c;
	Rune*	s;
	Rune*	t;
	Attr*	a;
	SEvent*	e;

	if(ps->skipping) {
		if(warn)
			fprint(2, "warning: skipping item: %I\n", it);
		return;
	}
	it->anchorid = ps->curanchor;
	it->state |= ps->curstate;
	if(tok != nil) {
		any = 0;
		i = nil;
		c = nil;
		s = nil;
		t = nil;
		e = nil;
		for(a = tok->attr; a != nil; a = a->next) {
			aid = a->attid;
			if(!attrinfo[aid])
				continue;
			switch(aid) {
			case Aid:
				i = a->value;
				break;

			case Aclass:
				c = a->value;
				break;

			case Astyle:
				s = a->value;
				break;

			case Atitle:
				t = a->value;
				break;

			default:
				assert(aid >= Aonblur && aid <= Aonunload);
				e = newscriptevent(scriptev[a->attid], a->value, e);
				break;
			}
			a->value = nil;
			any = 1;
		}
		if(any)
			it->genattr = newgenattr(i, c, s, t, e);
	}
	ps->curstate &= ~(IFbrk|IFbrksp|IFnobrk|IFcleft|IFcright);
	ps->prelastit = ps->lastit;
	ps->lastit->next = it;
	ps->lastit = it;
}

/* Make a text item out of s, */
/* using current font, foreground, vertical offset and underline state. */
static Item*
textit(Pstate* ps, Rune* s)
{
	assert(s != nil);
	return newitext(s, ps->curfont, ps->curfg, ps->curvoff + Voffbias, ps->curul);
}

/* Add text item or items for s, paying attention to */
/* current font, foreground, baseline offset, underline state, */
/* and literal mode.  Unless we're in literal mode, compress */
/* whitespace to single blank, and, if curstate has a break, */
/* trim any leading whitespace.  Whether in literal mode or not, */
/* turn nonbreaking spaces into spacer items with IFnobrk set. */
/* */
/* In literal mode, break up s at newlines and add breaks instead. */
/* Also replace tabs appropriate number of spaces. */
/* In nonliteral mode, break up the items every 100 or so characters */
/* just to make the layout algorithm not go quadratic. */
/* */
/* addtext assumes ownership of s. */
static void
addtext(Pstate* ps, Rune* s)
{
	int	n;
	int	i;
	int	j;
	int	k;
	int	col;
	int	c;
	int	nsp;
	Item*	it;
	Rune*	ss;
	Rune*	p;
	Rune	buf[SMALLBUFSIZE];

	assert(s != nil);
	n = runestrlen(s);
	i = 0;
	j = 0;
	if(ps->literal) {
		col = 0;
		while(i < n) {
			if(s[i] == '\n') {
				if(i > j) {
					/* trim trailing blanks from line */
					for(k = i; k > j; k--)
						if(s[k - 1] != ' ')
							break;
					if(k > j)
						additem(ps, textit(ps, _Strndup(s+j, k-j)), nil);
				}
				addlinebrk(ps, 0);
				j = i + 1;
				col = 0;
			}
			else {
				if(s[i] == '\t') {
					col += i - j;
					nsp = 8 - (col%8);
					/* make ss = s[j:i] + nsp spaces */
					ss = _newstr(i-j+nsp);
					p = _Stradd(ss, s+j, i-j);
					p = _Stradd(p, L(Ltab2space), nsp);
					*p = 0;
					additem(ps, textit(ps, ss), nil);
					col += nsp;
					j = i + 1;
				}
				else if(s[i] == NBSP) {
					if(i > j)
						additem(ps, textit(ps, _Strndup(s+j, i-j)), nil);
					addnbsp(ps);
					col += (i - j) + 1;
					j = i + 1;
				}
			}
			i++;
		}
		if(i > j) {
			if(j == 0 && i == n) {
				/* just transfer s over */
				additem(ps, textit(ps, s), nil);
			}
			else {
				additem(ps, textit(ps, _Strndup(s+j, i-j)), nil);
				free(s);
			}
		}
		else {
			free(s);
		}
	}
	else {	/* not literal mode */
		if((ps->curstate&IFbrk) || ps->lastit == ps->items)
			while(i < n) {
				c = s[i];
				if(c >= 256 || !isspace(c))
					break;
				i++;
			}
		p = buf;
		for(j = i; i < n; i++) {
			assert(p+i-j < buf+SMALLBUFSIZE-1);
			c = s[i];
			if(c == NBSP) {
				if(i > j)
					p = _Stradd(p, s+j, i-j);
				if(p > buf)
					additem(ps, textit(ps, _Strndup(buf, p-buf)), nil);
				p = buf;
				addnbsp(ps);
				j = i + 1;
				continue;
			}
			if(c < 256 && isspace(c)) {
				if(i > j)
					p = _Stradd(p, s+j, i-j);
				*p++ = ' ';
				while(i < n - 1) {
					c = s[i + 1];
					if(c >= 256 || !isspace(c))
						break;
					i++;
				}
				j = i + 1;
			}
			if(i - j >= 100) {
				p = _Stradd(p, s+j, i+1-j);
				j = i + 1;
			}
			if(p-buf >= 100) {
				additem(ps, textit(ps, _Strndup(buf, p-buf)), nil);
				p = buf;
			}
		}
		if(i > j && j < n) {
			assert(p+i-j < buf+SMALLBUFSIZE-1);
			p = _Stradd(p, s+j, i-j);
		}
		/* don't add a space if previous item ended in a space */
		if(p-buf == 1 && buf[0] == ' ' && ps->lastit != nil) {
			it = ps->lastit;
			if(it->tag == Itexttag) {
				ss = ((Itext*)it)->s;
				k = _Strlen(ss);
				if(k > 0 && ss[k] == ' ')
					p = buf;
			}
		}
		if(p > buf)
			additem(ps, textit(ps, _Strndup(buf, p-buf)), nil);
		free(s);
	}
}

/* Add a break to ps->curstate, with extra space if sp is true. */
/* If there was a previous break, combine this one's parameters */
/* with that to make the amt be the max of the two and the clr */
/* be the most general. (amt will be 0 or 1) */
/* Also, if the immediately preceding item was a text item, */
/* trim any whitespace from the end of it, if not in literal mode. */
/* Finally, if this is at the very beginning of the item list */
/* (the only thing there is a null spacer), then don't add the space. */
static void
addbrk(Pstate* ps, int sp, int clr)
{
	int	state;
	Rune*	l;
	int		nl;
	Rune*	r;
	int		nr;
	Itext*	t;
	Rune*	s;

	state = ps->curstate;
	clr = clr|(state&(IFcleft|IFcright));
	if(sp && !(ps->lastit == ps->items))
		sp = IFbrksp;
	else
		sp = 0;
	ps->curstate = IFbrk|sp|(state&~(IFcleft|IFcright))|clr;
	if(ps->lastit != ps->items) {
		if(!ps->literal && ps->lastit->tag == Itexttag) {
			t = (Itext*)ps->lastit;
			_splitr(t->s, _Strlen(t->s), notwhitespace, &l, &nl, &r, &nr);
			/* try to avoid making empty items */
			/* but not crucial f the occasional one gets through */
			if(nl == 0 && ps->prelastit != nil) {
				ps->lastit = ps->prelastit;
				ps->lastit->next = nil;
				ps->prelastit = nil;
			}
			else {
				s = t->s;
				if(nl == 0) {
					/* need a non-nil pointer to empty string */
					/* (_Strdup(L(Lempty)) returns nil) */
					t->s = emalloc(sizeof(Rune));
					t->s[0] = 0;
				}
				else
					t->s = _Strndup(l, nl);
				if(s)
					free(s);
			}
		}
	}
}

/* Add break due to a <br> or a newline within a preformatted section. */
/* We add a null item first, with current font's height and ascent, to make */
/* sure that the current line takes up at least that amount of vertical space. */
/* This ensures that <br>s on empty lines cause blank lines, and that */
/* multiple <br>s in a row give multiple blank lines. */
/* However don't add the spacer if the previous item was something that */
/* takes up space itself. */
static void
addlinebrk(Pstate* ps, int clr)
{
	int	obrkstate;
	int	b;
	int	addit;

	/* don't want break before our null item unless the previous item */
	/* was also a null item for the purposes of line breaking */
	obrkstate = ps->curstate&(IFbrk|IFbrksp);
	b = IFnobrk;
	addit = 0;
	if(ps->lastit != nil) {
		if(ps->lastit->tag == Ispacertag) {
			if(((Ispacer*)ps->lastit)->spkind == ISPvline)
				b = IFbrk;
			addit = 1;
		}
		else if(ps->lastit->tag == Ifloattag)
			addit = 1;
	}
	if(addit) {
		ps->curstate = (ps->curstate&~(IFbrk|IFbrksp))|b;
		additem(ps, newispacer(ISPvline), nil);
		ps->curstate = (ps->curstate&~(IFbrk|IFbrksp))|obrkstate;
	}
	addbrk(ps, 0, clr);
}

/* Add a nonbreakable space */
static void
addnbsp(Pstate* ps)
{
	/* if nbsp comes right where a break was specified, */
	/* do the break anyway (nbsp is being used to generate undiscardable */
	/* space rather than to prevent a break) */
	if((ps->curstate&IFbrk) == 0)
		ps->curstate |= IFnobrk;
	additem(ps, newispacer(ISPhspace), nil);
	/* but definitely no break on next item */
	ps->curstate |= IFnobrk;
}

/* Change hang in ps.curstate by delta. */
/* The amount is in 1/10ths of tabs, and is the amount that */
/* the current contiguous set of items with a hang value set */
/* is to be shifted left from its normal (indented) place. */
static void
changehang(Pstate* ps, int delta)
{
	int	amt;

	amt = (ps->curstate&IFhangmask) + delta;
	if(amt < 0) {
		if(warn)
			fprint(2, "warning: hang went negative\n");
		amt = 0;
	}
	ps->curstate = (ps->curstate&~IFhangmask)|amt;
}

/* Change indent in ps.curstate by delta. */
static void
changeindent(Pstate* ps, int delta)
{
	int	amt;

	amt = ((ps->curstate&IFindentmask) >> IFindentshift) + delta;
	if(amt < 0) {
		if(warn)
			fprint(2, "warning: indent went negative\n");
		amt = 0;
	}
	ps->curstate = (ps->curstate&~IFindentmask)|(amt << IFindentshift);
}

/* Push val on top of stack, and also return value pushed */
static int
push(Stack* stk, int val)
{
	if(stk->n == Nestmax) {
		if(warn)
			fprint(2, "warning: build stack overflow\n");
	}
	else
		stk->slots[stk->n++] = val;
	return val;
}

/* Pop top of stack */
static void
pop(Stack* stk)
{
	if(stk->n > 0)
		--stk->n;
}

/*Return top of stack, using dflt if stack is empty */
static int
top(Stack* stk, int dflt)
{
	if(stk->n == 0)
		return dflt;
	return stk->slots[stk->n-1];
}

/* pop, then return new top, with dflt if empty */
static int
popretnewtop(Stack* stk, int dflt)
{
	if(stk->n == 0)
		return dflt;
	stk->n--;
	if(stk->n == 0)
		return dflt;
	return stk->slots[stk->n-1];
}

/* Copy fromstk entries into tostk */
static void
copystack(Stack* tostk, Stack* fromstk)
{
	int n;

	n = fromstk->n;
	tostk->n = n;
	memmove(tostk->slots, fromstk->slots, n*sizeof(int));
}

static void
popfontstyle(Pstate* ps)
{
	pop(&ps->fntstylestk);
	setcurfont(ps);
}

static void
pushfontstyle(Pstate* ps, int sty)
{
	push(&ps->fntstylestk, sty);
	setcurfont(ps);
}

static void
popfontsize(Pstate* ps)
{
	pop(&ps->fntsizestk);
	setcurfont(ps);
}

static void
pushfontsize(Pstate* ps, int sz)
{
	push(&ps->fntsizestk, sz);
	setcurfont(ps);
}

static void
setcurfont(Pstate* ps)
{
	int	sty;
	int	sz;

	sty = top(&ps->fntstylestk, FntR);
	sz = top(&ps->fntsizestk, Normal);
	if(sz < Tiny)
		sz = Tiny;
	if(sz > Verylarge)
		sz = Verylarge;
	ps->curfont = sty*NumSize + sz;
}

static void
popjust(Pstate* ps)
{
	pop(&ps->juststk);
	setcurjust(ps);
}

static void
pushjust(Pstate* ps, int j)
{
	push(&ps->juststk, j);
	setcurjust(ps);
}

static void
setcurjust(Pstate* ps)
{
	int	j;
	int	state;

	j = top(&ps->juststk, ALleft);
	if(j != ps->curjust) {
		ps->curjust = j;
		state = ps->curstate;
		state &= ~(IFrjust|IFcjust);
		if(j == ALcenter)
			state |= IFcjust;
		else if(j == ALright)
			state |= IFrjust;
		ps->curstate = state;
	}
}

/* Do final rearrangement after table parsing is finished */
/* and assign cells to grid points */
static void
finish_table(Table* t)
{
	int	ncol;
	int	nrow;
	int	r;
	Tablerow*	rl;
	Tablecell*	cl;
	int*	rowspancnt;
	Tablecell**	rowspancell;
	int	ri;
	int	ci;
	Tablecell*	c;
	Tablecell*	cnext;
	Tablerow*	row;
	Tablerow*	rownext;
	int	rcols;
	int	newncol;
	int	k;
	int	j;
	int	cspan;
	int	rspan;
	int	i;

	rl = t->rows;
	t->nrow = nrow = _listlen((List*)rl);
	t->rows = (Tablerow*)emalloc(nrow * sizeof(Tablerow));
	ncol = 0;
	r = nrow - 1;
	for(row = rl; row != nil; row = rownext) {
		/* copy the data from the allocated Tablerow into the array slot */
		t->rows[r] = *row;
		rownext = row->next;
		row = &t->rows[r];
		r--;
		rcols = 0;
		c = row->cells;

		/* If rowspan is > 1 but this is the last row, */
		/* reset the rowspan */
		if(c != nil && c->rowspan > 1 && r == nrow-2)
				c->rowspan = 1;

		/* reverse row->cells list (along nextinrow pointers) */
		row->cells = nil;
		while(c != nil) {
			cnext = c->nextinrow;
			c->nextinrow = row->cells;
			row->cells = c;
			rcols += c->colspan;
			c = cnext;
		}
		if(rcols > ncol)
			ncol = rcols;
	}
	t->ncol = ncol;
	t->cols = (Tablecol*)emalloc(ncol * sizeof(Tablecol));

	/* Reverse cells just so they are drawn in source order. */
	/* Also, trim their contents so they don't end in whitespace. */
	t->cells = (Tablecell*)_revlist((List*)t->cells);
	for(c = t->cells; c != nil; c= c->next)
		trim_cell(c);
	t->grid = (Tablecell***)emalloc(nrow * sizeof(Tablecell**));
	for(i = 0; i < nrow; i++)
		t->grid[i] = (Tablecell**)emalloc(ncol * sizeof(Tablecell*));

	/* The following arrays keep track of cells that are spanning */
	/* multiple rows;  rowspancnt[i] is the number of rows left */
	/* to be spanned in column i. */
	/* When done, cell's (row,col) is upper left grid point. */
	rowspancnt = (int*)emalloc(ncol * sizeof(int));
	rowspancell = (Tablecell**)emalloc(ncol * sizeof(Tablecell*));
	for(ri = 0; ri < nrow; ri++) {
		row = &t->rows[ri];
		cl = row->cells;
		ci = 0;
		while(ci < ncol || cl != nil) {
			if(ci < ncol && rowspancnt[ci] > 0) {
				t->grid[ri][ci] = rowspancell[ci];
				rowspancnt[ci]--;
				ci++;
			}
			else {
				if(cl == nil) {
					ci++;
					continue;
				}
				c = cl;
				cl = cl->nextinrow;
				cspan = c->colspan;
				rspan = c->rowspan;
				if(ci + cspan > ncol) {
					/* because of row spanning, we calculated */
					/* ncol incorrectly; adjust it */
					newncol = ci + cspan;
					t->cols = (Tablecol*)erealloc(t->cols, newncol * sizeof(Tablecol));
					rowspancnt = (int*)erealloc(rowspancnt, newncol * sizeof(int));
					rowspancell = (Tablecell**)erealloc(rowspancell, newncol * sizeof(Tablecell*));
					k = newncol-ncol;
					memset(t->cols+ncol, 0, k*sizeof(Tablecol));
					memset(rowspancnt+ncol, 0, k*sizeof(int));
					memset(rowspancell+ncol, 0, k*sizeof(Tablecell*));
					for(j = 0; j < nrow; j++) {
						t->grid[j] = (Tablecell**)erealloc(t->grid[j], newncol * sizeof(Tablecell*));
						memset(t->grid[j], 0, k*sizeof(Tablecell*));
					}
					t->ncol = ncol = newncol;
				}
				c->row = ri;
				c->col = ci;
				for(i = 0; i < cspan; i++) {
					t->grid[ri][ci] = c;
					if(rspan > 1) {
						rowspancnt[ci] = rspan - 1;
						rowspancell[ci] = c;
					}
					ci++;
				}
			}
		}
	}
}

/* Remove tail of cell content until it isn't whitespace. */
static void
trim_cell(Tablecell* c)
{
	int	dropping;
	Rune*	s;
	Rune*	x;
	Rune*	y;
	int		nx;
	int		ny;
	Item*	p;
	Itext*	q;
	Item*	pprev;

	dropping = 1;
	while(c->content != nil && dropping) {
		p = c->content;
		pprev = nil;
		while(p->next != nil) {
			pprev = p;
			p = p->next;
		}
		dropping = 0;
		if(!(p->state&IFnobrk)) {
			if(p->tag == Itexttag) {
				q = (Itext*)p;
				s = q->s;
				_splitr(s, _Strlen(s), notwhitespace, &x, &nx, &y, &ny);
				if(nx != 0 && ny != 0) {
					q->s = _Strndup(x, nx);
					free(s);
				}
				break;
			}
		}
		if(dropping) {
			if(pprev == nil)
				c->content = nil;
			else
				pprev->next = nil;
			freeitem(p);
		}
	}
}

/* Caller must free answer (eventually). */
static Rune*
listmark(uchar ty, int n)
{
	Rune*	s;
	Rune*	t;
	int	n2;
	int	i;

	s = nil;
	switch(ty) {
	case LTdisc:
	case LTsquare:
	case LTcircle:
		s = _newstr(1);
		s[0] = (ty == LTdisc)? 0x2022		/* bullet */
			: ((ty == LTsquare)? 0x220e	/* filled square */
			    : 0x2218);				/* degree */
		s[1] = 0;
		break;

	case LT1:
		t = _ltoStr(n);
		n2 = _Strlen(t);
		s = _newstr(n2+1);
		t = _Stradd(s, t, n2);
		*t++ = '.';
		*t = 0;
		break;

	case LTa:
	case LTA:
		n--;
		i = 0;
		if(n < 0)
			n = 0;
		s = _newstr((n <= 25)? 2 : 3);
		if(n > 25) {
			n2 = n%26;
			n /= 26;
			if(n2 > 25)
				n2 = 25;
			s[i++] = n2 + (ty == LTa)? 'a' : 'A';
		}
		s[i++] = n + (ty == LTa)? 'a' : 'A';
		s[i++] = '.';
		s[i] = 0;
		break;

	case LTi:
	case LTI:
		if(n >= NROMAN) {
			if(warn)
				fprint(2, "warning: unimplemented roman number > %d\n", NROMAN);
			n = NROMAN;
		}
		t = roman[n - 1];
		n2 = _Strlen(t);
		s = _newstr(n2+1);
		for(i = 0; i < n2; i++)
			s[i] = (ty == LTi)? tolower(t[i]) : t[i];
		s[i++] = '.';
		s[i] = 0;
		break;
	}
	return s;
}

/* Find map with given name in di.maps. */
/* If not there, add one, copying name. */
/* Ownership of map remains with di->maps list. */
static Map*
getmap(Docinfo* di, Rune* name)
{
	Map*	m;

	for(m = di->maps; m != nil; m = m->next) {
		if(!_Strcmp(name, m->name))
			return m;
	}
	m = (Map*)emalloc(sizeof(Map));
	m->name = _Strdup(name);
	m->areas = nil;
	m->next = di->maps;
	di->maps = m;
	return m;
}

/* Transfers ownership of href to Area */
static Area*
newarea(int shape, Rune* href, int target, Area* link)
{
	Area* a;

	a = (Area*)emalloc(sizeof(Area));
	a->shape = shape;
	a->href = href;
	a->target = target;
	a->next = link;
	return a;
}

/* Return string value associated with attid in tok, nil if none. */
/* Caller must free the result (eventually). */
static Rune*
aval(Token* tok, int attid)
{
	Rune*	ans;

	_tokaval(tok, attid, &ans, 1);	/* transfers string ownership from token to ans */
	return ans;
}

/* Like aval, but use dflt if there was no such attribute in tok. */
/* Caller must free the result (eventually). */
static Rune*
astrval(Token* tok, int attid, Rune* dflt)
{
	Rune*	ans;

	if(_tokaval(tok, attid, &ans, 1))
		return ans;	/* transfers string ownership from token to ans */
	else
		return _Strdup(dflt);
}

/* Here we're supposed to convert to an int, */
/* and have a default when not found */
static int
aintval(Token* tok, int attid, int dflt)
{
	Rune*	ans;

	if(!_tokaval(tok, attid, &ans, 0) || ans == nil)
		return dflt;
	else
		return toint(ans);
}

/* Like aintval, but result should be >= 0 */
static int
auintval(Token* tok, int attid, int dflt)
{
	Rune* ans;
	int v;

	if(!_tokaval(tok, attid, &ans, 0) || ans == nil)
		return dflt;
	else {
		v = toint(ans);
		return v >= 0? v : 0;
	}
}

/* int conversion, but with possible error check (if warning) */
static int
toint(Rune* s)
{
	int ans;
	Rune* eptr;

	ans = _Strtol(s, &eptr, 10);
	if(warn) {
		if(*eptr != 0) {
			eptr = _Strclass(eptr, notwhitespace);
			if(eptr != nil)
				fprint(2, "warning: expected integer, got %S\n", s);
		}
	}
	return ans;
}

/* Attribute value when need a table to convert strings to ints */
static int
atabval(Token* tok, int attid, StringInt* tab, int ntab, int dflt)
{
	Rune*	aval;
	int	ans;

	ans = dflt;
	if(_tokaval(tok, attid, &aval, 0)) {
		if(!_lookup(tab, ntab, aval, _Strlen(aval), &ans)) {
			ans = dflt;
			if(warn)
				fprint(2, "warning: name not found in table lookup: %S\n", aval);
		}
	}
	return ans;
}

/* Attribute value when supposed to be a color */
static int
acolorval(Token* tok, int attid, int dflt)
{
	Rune*	aval;
	int	ans;

	ans = dflt;
	if(_tokaval(tok, attid, &aval, 0))
		ans = color(aval, dflt);
	return ans;
}

/* Attribute value when supposed to be a target frame name */
static int
atargval(Token* tok, int dflt)
{
	int	ans;
	Rune*	aval;

	ans = dflt;
	if(_tokaval(tok, Atarget, &aval, 0)){
		ans = targetid(aval);
	}
	return ans;
}

/* special for list types, where "i" and "I" are different, */
/* but "square" and "SQUARE" are the same */
static int
listtyval(Token* tok, int dflt)
{
	Rune*	aval;
	int	ans;
	int	n;

	ans = dflt;
	if(_tokaval(tok, Atype, &aval, 0)) {
		n = _Strlen(aval);
		if(n == 1) {
			switch(aval[0]) {
			case '1':
				ans = LT1;
				break;
			case 'A':
				ans = LTA;
				break;
			case 'I':
				ans = LTI;
				break;
			case 'a':
				ans = LTa;
				break;
			case 'i':
				ans = LTi;
			default:
				if(warn)
					fprint(2, "warning: unknown list element type %c\n", aval[0]);
			}
		}
		else {
			if(!_Strncmpci(aval, n, L(Lcircle)))
				ans = LTcircle;
			else if(!_Strncmpci(aval, n, L(Ldisc)))
				ans = LTdisc;
			else if(!_Strncmpci(aval, n, L(Lsquare)))
				ans = LTsquare;
			else {
				if(warn)
					fprint(2, "warning: unknown list element type %S\n", aval);
			}
		}
	}
	return ans;
}

/* Attribute value when value is a URL, possibly relative to base. */
/* FOR NOW: leave the url relative. */
/* Caller must free the result (eventually). */
static Rune*
aurlval(Token* tok, int attid, Rune* dflt, Rune* base)
{
	Rune*	ans;
	Rune*	url;

	USED(base);
	ans = nil;
	if(_tokaval(tok, attid, &url, 0) && url != nil)
		ans = removeallwhite(url);
	if(ans == nil)
		ans = _Strdup(dflt);
	return ans;
}

/* Return copy of s but with all whitespace (even internal) removed. */
/* This fixes some buggy URL specification strings. */
static Rune*
removeallwhite(Rune* s)
{
	int	j;
	int	n;
	int	i;
	int	c;
	Rune*	ans;

	j = 0;
	n = _Strlen(s);
	for(i = 0; i < n; i++) {
		c = s[i];
		if(c >= 256 || !isspace(c))
			j++;
	}
	if(j < n) {
		ans = _newstr(j);
		j = 0;
		for(i = 0; i < n; i++) {
			c = s[i];
			if(c >= 256 || !isspace(c))
				ans[j++] = c;
		}
		ans[j] = 0;
	}
	else
		ans = _Strdup(s);
	return ans;
}

/* Attribute value when mere presence of attr implies value of 1, */
/* but if there is an integer there, return it as the value. */
static int
aflagval(Token* tok, int attid)
{
	int	val;
	Rune*	sval;

	val = 0;
	if(_tokaval(tok, attid, &sval, 0)) {
		val = 1;
		if(sval != nil)
			val = toint(sval);
	}
	return val;
}

static Align
makealign(int halign, int valign)
{
	Align	al;

	al.halign = halign;
	al.valign = valign;
	return al;
}

/* Make an Align (two alignments, horizontal and vertical) */
static Align
aalign(Token* tok)
{
	return makealign(
		atabval(tok, Aalign, align_tab, NALIGNTAB, ALnone),
		atabval(tok, Avalign, align_tab, NALIGNTAB, ALnone));
}

/* Make a Dimen, based on value of attid attr */
static Dimen
adimen(Token* tok, int attid)
{
	Rune*	wd;

	if(_tokaval(tok, attid, &wd, 0))
		return parsedim(wd, _Strlen(wd));
	else
		return makedimen(Dnone, 0);
}

/* Parse s[0:n] as num[.[num]][unit][%|*] */
static Dimen
parsedim(Rune* s, int ns)
{
	int	kind;
	int	spec;
	Rune*	l;
	int	nl;
	Rune*	r;
	int	nr;
	int	mul;
	int	i;
	Rune*	f;
	int	nf;
	int	Tkdpi;
	Rune*	units;

	kind = Dnone;
	spec = 0;
	_splitl(s, ns, L(Lnot0to9), &l, &nl, &r, &nr);
	if(nl != 0) {
		spec = 1000*_Strtol(l, nil, 10);
		if(nr > 0 && r[0] == '.') {
			_splitl(r+1, nr-1, L(Lnot0to9), &f, &nf, &r, &nr);
			if(nf != 0) {
				mul = 100;
				for(i = 0; i < nf; i++) {
					spec = spec + mul*(f[i]-'0');
					mul = mul/10;
				}
			}
		}
		kind = Dpixels;
		if(nr != 0) {
			if(nr >= 2) {
				Tkdpi = 100;
				units = r;
				r = r+2;
				nr -= 2;
				if(!_Strncmpci(units, 2, L(Lpt)))
					spec = (spec*Tkdpi)/72;
				else if(!_Strncmpci(units, 2, L(Lpi)))
					spec = (spec*12*Tkdpi)/72;
				else if(!_Strncmpci(units, 2, L(Lin)))
					spec = spec*Tkdpi;
				else if(!_Strncmpci(units, 2, L(Lcm)))
					spec = (spec*100*Tkdpi)/254;
				else if(!_Strncmpci(units, 2, L(Lmm)))
					spec = (spec*10*Tkdpi)/254;
				else if(!_Strncmpci(units, 2, L(Lem)))
					spec = spec*15;
				else {
					if(warn)
						fprint(2, "warning: unknown units %C%Cs\n", units[0], units[1]);
				}
			}
			if(nr >= 1) {
				if(r[0] == '%')
					kind = Dpercent;
				else if(r[0] == '*')
					kind = Drelative;
			}
		}
		spec = spec/1000;
	}
	else if(nr == 1 && r[0] == '*') {
		spec = 1;
		kind = Drelative;
	}
	return makedimen(kind, spec);
}

static void
setdimarray(Token* tok, int attid, Dimen** pans, int* panslen)
{
	Rune*	s;
	Dimen*	d;
	int	k;
	int	nc;
	Rune* a[SMALLBUFSIZE];
	int	an[SMALLBUFSIZE];

	if(_tokaval(tok, attid, &s, 0)) {
		nc = _splitall(s, _Strlen(s), L(Lcommaspace), a, an, SMALLBUFSIZE);
		if(nc > 0) {
			d = (Dimen*)emalloc(nc * sizeof(Dimen));
			for(k = 0; k < nc; k++) {
				d[k] = parsedim(a[k], an[k]);
			}
			*pans = d;
			*panslen = nc;
			return;
		}
	}
	*pans = nil;
	*panslen = 0;
}

static Background
makebackground(Rune* imageurl, int color)
{
	Background bg;

	bg.image = imageurl;
	bg.color = color;
	return bg;
}

static Item*
newitext(Rune* s, int fnt, int fg, int voff, int ul)
{
	Itext* t;

	assert(s != nil);
	t = (Itext*)emalloc(sizeof(Itext));
	t->item.tag = Itexttag;
	t->s = s;
	t->fnt = fnt;
	t->fg = fg;
	t->voff = voff;
	t->ul = ul;
	return (Item*)t;
}

static Item*
newirule(int align, int size, int noshade, Dimen wspec)
{
	Irule* r;

	r = (Irule*)emalloc(sizeof(Irule));
	r->item.tag = Iruletag;
	r->align = align;
	r->size = size;
	r->noshade = noshade;
	r->wspec = wspec;
	return (Item*)r;
}

/* Map is owned elsewhere. */
static Item*
newiimage(Rune* src, Rune* altrep, int align, int width, int height,
		int hspace, int vspace, int border, int ismap, Map* map)
{
	Iimage* i;
	int	state;

	state = 0;
	if(ismap)
		state = IFsmap;
	i = (Iimage*)emalloc(sizeof(Iimage));
	i->item.tag = Iimagetag;
	i->item.state = state;
	i->imsrc = src;
	i->altrep = altrep;
	i->align = align;
	i->imwidth = width;
	i->imheight = height;
	i->hspace = hspace;
	i->vspace = vspace;
	i->border = border;
	i->map = map;
	i->ctlid = -1;
	return (Item*)i;
}

static Item*
newiformfield(Formfield* ff)
{
	Iformfield* f;

	f = (Iformfield*)emalloc(sizeof(Iformfield));
	f->item.tag = Iformfieldtag;
	f->formfield = ff;
	return (Item*)f;
}

static Item*
newitable(Table* tab)
{
	Itable* t;

	t = (Itable*)emalloc(sizeof(Itable));
	t->item.tag = Itabletag;
	t->table = tab;
	return (Item*)t;
}

static Item*
newifloat(Item* it, int side)
{
	Ifloat* f;

	f = (Ifloat*)emalloc(sizeof(Ifloat));
	f->_item.tag = Ifloattag;
	f->_item.state = IFwrap;
	f->item = it;
	f->side = side;
	return (Item*)f;
}

static Item*
newispacer(int spkind)
{
	Ispacer* s;

	s = (Ispacer*)emalloc(sizeof(Ispacer));
	s->item.tag = Ispacertag;
	s->spkind = spkind;
	return (Item*)s;
}

/* Free one item (caller must deal with next pointer) */
static void
freeitem(Item* it)
{
	Iimage* ii;
	Genattr* ga;

	if(it == nil)
		return;

	switch(it->tag) {
	case Itexttag:
		free(((Itext*)it)->s);
		break;
	case Iimagetag:
		ii = (Iimage*)it;
		free(ii->imsrc);
		free(ii->altrep);
		break;
	case Iformfieldtag:
		freeformfield(((Iformfield*)it)->formfield);
		break;
	case Itabletag:
		freetable(((Itable*)it)->table);
		break;
	case Ifloattag:
		freeitem(((Ifloat*)it)->item);
		break;
	}
	ga = it->genattr;
	if(ga != nil) {
		free(ga->id);
		free(ga->class);
		free(ga->style);
		free(ga->title);
		freescriptevents(ga->events);
	}
	free(it);
}

/* Free list of items chained through next pointer */
void
freeitems(Item* ithead)
{
	Item* it;
	Item* itnext;

	it = ithead;
	while(it != nil) {
		itnext = it->next;
		freeitem(it);
		it = itnext;
	}
}

static void
freeformfield(Formfield* ff)
{
	Option* o;
	Option* onext;

	if(ff == nil)
		return;

	free(ff->name);
	free(ff->value);
	for(o = ff->options; o != nil; o = onext) {
		onext = o->next;
		free(o->value);
		free(o->display);
	}
	free(ff);
}

static void
freetable(Table* t)
{
	int i;
	Tablecell* c;
	Tablecell* cnext;

	if(t == nil)
		return;

	/* We'll find all the unique cells via t->cells and next pointers. */
	/* (Other pointers to cells in the table are duplicates of these) */
	for(c = t->cells; c != nil; c = cnext) {
		cnext = c->next;
		freeitems(c->content);
	}
	if(t->grid != nil) {
		for(i = 0; i < t->nrow; i++)
			free(t->grid[i]);
		free(t->grid);
	}
	free(t->rows);
	free(t->cols);
	freeitems(t->caption);
	free(t);
}

static void
freeform(Form* f)
{
	if(f == nil)
		return;

	free(f->name);
	free(f->action);
	/* Form doesn't own its fields (Iformfield items do) */
	free(f);
}

static void
freeforms(Form* fhead)
{
	Form* f;
	Form* fnext;

	for(f = fhead; f != nil; f = fnext) {
		fnext = f->next;
		freeform(f);
	}
}

static void
freeanchor(Anchor* a)
{
	if(a == nil)
		return;

	free(a->name);
	free(a->href);
	free(a);
}

static void
freeanchors(Anchor* ahead)
{
	Anchor* a;
	Anchor* anext;

	for(a = ahead; a != nil; a = anext) {
		anext = a->next;
		freeanchor(a);
	}
}

static void
freedestanchor(DestAnchor* da)
{
	if(da == nil)
		return;

	free(da->name);
	free(da);
}

static void
freedestanchors(DestAnchor* dahead)
{
	DestAnchor* da;
	DestAnchor* danext;

	for(da = dahead; da != nil; da = danext) {
		danext = da->next;
		freedestanchor(da);
	}
}

static void
freearea(Area* a)
{
	if(a == nil)
		return;
	free(a->href);
	free(a->coords);
}

static void freekidinfos(Kidinfo* khead);

static void
freekidinfo(Kidinfo* k)
{
	if(k->isframeset) {
		free(k->rows);
		free(k->cols);
		freekidinfos(k->kidinfos);
	}
	else {
		free(k->src);
		free(k->name);
	}
	free(k);
}

static void
freekidinfos(Kidinfo* khead)
{
	Kidinfo* k;
	Kidinfo* knext;

	for(k = khead; k != nil; k = knext) {
		knext = k->next;
		freekidinfo(k);
	}
}

static void
freemap(Map* m)
{
	Area* a;
	Area* anext;

	if(m == nil)
		return;

	free(m->name);
	for(a = m->areas; a != nil; a = anext) {
		anext = a->next;
		freearea(a);
	}
	free(m);
}

static void
freemaps(Map* mhead)
{
	Map* m;
	Map* mnext;

	for(m = mhead; m != nil; m = mnext) {
		mnext = m->next;
		freemap(m);
	}
}

void
freedocinfo(Docinfo* d)
{
	if(d == nil)
		return;
	free(d->src);
	free(d->base);
	freeitem((Item*)d->backgrounditem);
	free(d->refresh);
	freekidinfos(d->kidinfo);
	freeanchors(d->anchors);
	freedestanchors(d->dests);
	freeforms(d->forms);
	freemaps(d->maps);
	/* tables, images, and formfields are freed when */
	/* the items pointing at them are freed */
	free(d);
}

/* Currently, someone else owns all the memory */
/* pointed to by things in a Pstate. */
static void
freepstate(Pstate* p)
{
	free(p);
}

static void
freepstatestack(Pstate* pshead)
{
	Pstate* p;
	Pstate* pnext;

	for(p = pshead; p != nil; p = pnext) {
		pnext = p->next;
		free(p);
	}
}

static int
Iconv(Fmt *f)
{
	Item*	it;
	Itext*	t;
	Irule*	r;
	Iimage*	i;
	Ifloat*	fl;
	int	state;
	Formfield*	ff;
	Rune*	ty;
	Tablecell*	c;
	Table*	tab;
	char*	p;
	int	cl;
	int	hang;
	int	indent;
	int	bi;
	int	nbuf;
	char	buf[BIGBUFSIZE];

	it = va_arg(f->args, Item*);
	bi = 0;
	nbuf = sizeof(buf);
	state = it->state;
	nbuf = nbuf-1;
	if(state&IFbrk) {
		cl = state&(IFcleft|IFcright);
		p = "";
		if(cl) {
			if(cl == (IFcleft|IFcright))
				p = " both";
			else if(cl == IFcleft)
				p = " left";
			else
				p = " right";
		}
		bi = snprint(buf, nbuf, "brk(%d%s)", (state&IFbrksp)? 1 : 0, p);
	}
	if(state&IFnobrk)
		bi += snprint(buf+bi, nbuf-bi, " nobrk");
	if(!(state&IFwrap))
		bi += snprint(buf+bi, nbuf-bi, " nowrap");
	if(state&IFrjust)
		bi += snprint(buf+bi, nbuf-bi, " rjust");
	if(state&IFcjust)
		bi += snprint(buf+bi, nbuf-bi, " cjust");
	if(state&IFsmap)
		bi += snprint(buf+bi, nbuf-bi, " smap");
	indent = (state&IFindentmask) >> IFindentshift;
	if(indent > 0)
		bi += snprint(buf+bi, nbuf-bi, " indent=%d", indent);
	hang = state&IFhangmask;
	if(hang > 0)
		bi += snprint(buf+bi, nbuf-bi, " hang=%d", hang);

	switch(it->tag) {
	case Itexttag:
		t = (Itext*)it;
		bi += snprint(buf+bi, nbuf-bi, " Text '%S', fnt=%d, fg=%x", t->s, t->fnt, t->fg);
		break;

	case Iruletag:
		r = (Irule*)it;
		bi += snprint(buf+bi, nbuf-bi, "Rule size=%d, al=%S, wspec=", r->size, stringalign(r->align));
		bi += dimprint(buf+bi, nbuf-bi, r->wspec);
		break;

	case Iimagetag:
		i = (Iimage*)it;
		bi += snprint(buf+bi, nbuf-bi,
			"Image src=%S, alt=%S, al=%S, w=%d, h=%d hsp=%d, vsp=%d, bd=%d, map=%S",
			i->imsrc, i->altrep? i->altrep : L(Lempty), stringalign(i->align), i->imwidth, i->imheight,
			i->hspace, i->vspace, i->border, i->map?i->map->name : L(Lempty));
		break;

	case Iformfieldtag:
		ff = ((Iformfield*)it)->formfield;
		if(ff->ftype == Ftextarea)
			ty = L(Ltextarea);
		else if(ff->ftype == Fselect)
			ty = L(Lselect);
		else {
			ty = _revlookup(input_tab, NINPUTTAB, ff->ftype);
			if(ty == nil)
				ty = L(Lnone);
		}
		bi += snprint(buf+bi, nbuf-bi, "Formfield %S, fieldid=%d, formid=%d, name=%S, value=%S",
			ty, ff->fieldid, ff->form->formid, ff->name?  ff->name : L(Lempty),
			ff->value? ff->value : L(Lempty));
		break;

	case Itabletag:
		tab = ((Itable*)it)->table;
		bi += snprint(buf+bi, nbuf-bi, "Table tableid=%d, width=", tab->tableid);
		bi += dimprint(buf+bi, nbuf-bi, tab->width);
		bi += snprint(buf+bi, nbuf-bi, ", nrow=%d, ncol=%d, ncell=%d, totw=%d, toth=%d\n",
			tab->nrow, tab->ncol, tab->ncell, tab->totw, tab->toth);
		for(c = tab->cells; c != nil; c = c->next)
			bi += snprint(buf+bi, nbuf-bi, "Cell %d.%d, at (%d,%d) ",
					tab->tableid, c->cellid, c->row, c->col);
		bi += snprint(buf+bi, nbuf-bi, "End of Table %d", tab->tableid);
		break;

	case Ifloattag:
		fl = (Ifloat*)it;
		bi += snprint(buf+bi, nbuf-bi, "Float, x=%d y=%d, side=%S, it=%I",
			fl->x, fl->y, stringalign(fl->side), fl->item);
		bi += snprint(buf+bi, nbuf-bi, "\n\t");
		break;

	case Ispacertag:
		p = "";
		switch(((Ispacer*)it)->spkind) {
		case ISPnull:
			p = "null";
			break;
		case ISPvline:
			p = "vline";
			break;
		case ISPhspace:
			p = "hspace";
			break;
		}
		bi += snprint(buf+bi, nbuf-bi, "Spacer %s ", p);
		break;
	}
	bi += snprint(buf+bi, nbuf-bi, " w=%d, h=%d, a=%d, anchor=%d\n",
			it->width, it->height, it->ascent, it->anchorid);
	buf[bi] = 0;
	return fmtstrcpy(f, buf);
}

/* String version of alignment 'a' */
static Rune*
stringalign(int a)
{
	Rune*	s;

	s = _revlookup(align_tab, NALIGNTAB, a);
	if(s == nil)
		s = L(Lnone);
	return s;
}

/* Put at most nbuf chars of representation of d into buf, */
/* and return number of characters put */
static int
dimprint(char* buf, int nbuf, Dimen d)
{
	int	n;
	int	k;

	n = 0;
	n += snprint(buf, nbuf, "%d", dimenspec(d));
	k = dimenkind(d);
	if(k == Dpercent)
		buf[n++] = '%';
	if(k == Drelative)
		buf[n++] = '*';
	return n;
}

void
printitems(Item* items, char* msg)
{
	Item*	il;

	fprint(2, "%s\n", msg);
	il = items;
	while(il != nil) {
		fprint(2, "%I", il);
		il = il->next;
	}
}

static Genattr*
newgenattr(Rune* id, Rune* class, Rune* style, Rune* title, SEvent* events)
{
	Genattr* g;

	g = (Genattr*)emalloc(sizeof(Genattr));
	g->id = id;
	g->class = class;
	g->style = style;
	g->title = title;
	g->events = events;
	return g;
}

static Formfield*
newformfield(int ftype, int fieldid, Form* form, Rune* name,
		Rune* value, int size, int maxlength, Formfield* link)
{
	Formfield* ff;

	ff = (Formfield*)emalloc(sizeof(Formfield));
	ff->ftype = ftype;
	ff->fieldid = fieldid;
	ff->form = form;
	ff->name = name;
	ff->value = value;
	ff->size = size;
	ff->maxlength = maxlength;
	ff->ctlid = -1;
	ff->next = link;
	return ff;
}

/* Transfers ownership of value and display to Option. */
static Option*
newoption(int selected, Rune* value, Rune* display, Option* link)
{
	Option *o;

	o = (Option*)emalloc(sizeof(Option));
	o->selected = selected;
	o->value = value;
	o->display = display;
	o->next = link;
	return o;
}

static Form*
newform(int formid, Rune* name, Rune* action, int target, int method, Form* link)
{
	Form* f;

	f = (Form*)emalloc(sizeof(Form));
	f->formid = formid;
	f->name = name;
	f->action = action;
	f->target = target;
	f->method = method;
	f->nfields = 0;
	f->fields = nil;
	f->next = link;
	return f;
}

static Table*
newtable(int tableid, Align align, Dimen width, int border,
	int cellspacing, int cellpadding, Background bg, Token* tok, Table* link)
{
	Table* t;

	t = (Table*)emalloc(sizeof(Table));
	t->tableid = tableid;
	t->align = align;
	t->width = width;
	t->border = border;
	t->cellspacing = cellspacing;
	t->cellpadding = cellpadding;
	t->background = bg;
	t->caption_place = ALbottom;
	t->caption_lay = nil;
	t->tabletok = tok;
	t->tabletok = nil;
	t->next = link;
	return t;
}

static Tablerow*
newtablerow(Align align, Background bg, int flags, Tablerow* link)
{
	Tablerow* tr;

	tr = (Tablerow*)emalloc(sizeof(Tablerow));
	tr->align = align;
	tr->background = bg;
	tr->flags = flags;
	tr->next = link;
	return tr;
}

static Tablecell*
newtablecell(int cellid, int rowspan, int colspan, Align align, Dimen wspec, int hspec,
		Background bg, int flags, Tablecell* link)
{
	Tablecell* c;

	c = (Tablecell*)emalloc(sizeof(Tablecell));
	c->cellid = cellid;
	c->lay = nil;
	c->rowspan = rowspan;
	c->colspan = colspan;
	c->align = align;
	c->flags = flags;
	c->wspec = wspec;
	c->hspec = hspec;
	c->background = bg;
	c->next = link;
	return c;
}

static Anchor*
newanchor(int index, Rune* name, Rune* href, int target, Anchor* link)
{
	Anchor* a;

	a = (Anchor*)emalloc(sizeof(Anchor));
	a->index = index;
	a->name = name;
	a->href = href;
	a->target = target;
	a->next = link;
	return a;
}

static DestAnchor*
newdestanchor(int index, Rune* name, Item* item, DestAnchor* link)
{
	DestAnchor* d;

	d = (DestAnchor*)emalloc(sizeof(DestAnchor));
	d->index = index;
	d->name = name;
	d->item = item;
	d->next = link;
	return d;
}

static SEvent*
newscriptevent(int type, Rune* script, SEvent* link)
{
	SEvent* ans;

	ans = (SEvent*)emalloc(sizeof(SEvent));
	ans->type = type;
	ans->script = script;
	ans->next = link;
	return ans;
}

static void
freescriptevents(SEvent* ehead)
{
	SEvent* e;
	SEvent* nexte;

	e = ehead;
	while(e != nil) {
		nexte = e->next;
		free(e->script);
		free(e);
		e = nexte;
	}
}

static Dimen
makedimen(int kind, int spec)
{
	Dimen d;

	if(spec&Dkindmask) {
		if(warn)
			fprint(2, "warning: dimension spec too big: %d\n", spec);
		spec = 0;
	}
	d.kindspec = kind|spec;
	return d;
}

int
dimenkind(Dimen d)
{
	return (d.kindspec&Dkindmask);
}

int
dimenspec(Dimen d)
{
	return (d.kindspec&Dspecmask);
}

static Kidinfo*
newkidinfo(int isframeset, Kidinfo* link)
{
	Kidinfo*	ki;

	ki = (Kidinfo*)emalloc(sizeof(Kidinfo));
	ki->isframeset = isframeset;
	if(!isframeset) {
		ki->flags = FRhscrollauto|FRvscrollauto;
		ki->marginw = FRKIDMARGIN;
		ki->marginh = FRKIDMARGIN;
		ki->framebd = 1;
	}
	ki->next = link;
	return ki;
}

static Docinfo*
newdocinfo(void)
{
	Docinfo*	d;

	d = (Docinfo*)emalloc(sizeof(Docinfo));
	resetdocinfo(d);
	return d;
}

static void
resetdocinfo(Docinfo* d)
{
	memset(d, 0, sizeof(Docinfo));
	d->background = makebackground(nil, White);
	d->text = Black;
	d->link = Blue;
	d->vlink = Blue;
	d->alink = Blue;
	d->target = FTself;
	d->chset = ISO_8859_1;
	d->scripttype = TextJavascript;
	d->frameid = -1;
}

/* Use targetmap array to keep track of name <-> targetid mapping. */
/* Use real malloc(), and never free */
static void
targetmapinit(void)
{
	targetmapsize = 10;
	targetmap = (StringInt*)emalloc(targetmapsize*sizeof(StringInt));
	memset(targetmap, 0, targetmapsize*sizeof(StringInt));
	targetmap[0].key = _Strdup(L(L_top));
	targetmap[0].val = FTtop;
	targetmap[1].key = _Strdup(L(L_self));
	targetmap[1].val = FTself;
	targetmap[2].key = _Strdup(L(L_parent));
	targetmap[2].val = FTparent;
	targetmap[3].key = _Strdup(L(L_blank));
	targetmap[3].val = FTblank;
	ntargets = 4;
}

int
targetid(Rune* s)
{
	int i;
	int n;

	n = _Strlen(s);
	if(n == 0)
		return FTself;
	for(i = 0; i < ntargets; i++)
		if(_Strcmp(s, targetmap[i].key) == 0)
			return targetmap[i].val;
	if(i >= targetmapsize) {
		targetmapsize += 10;
		targetmap = (StringInt*)erealloc(targetmap, targetmapsize*sizeof(StringInt));
	}
	targetmap[i].key = (Rune*)emalloc((n+1)*sizeof(Rune));
	memmove(targetmap[i].key, s, (n+1)*sizeof(Rune));
	targetmap[i].val = i;
	ntargets++;
	return i;
}

Rune*
targetname(int targid)
{
	int i;

	for(i = 0; i < ntargets; i++)
		if(targetmap[i].val == targid)
			return targetmap[i].key;
	return L(Lquestion);
}

/* Convert HTML color spec to RGB value, returning dflt if can't. */
/* Argument is supposed to be a valid HTML color, or "". */
/* Return the RGB value of the color, using dflt if s */
/* is nil or an invalid color. */
static int
color(Rune* s, int dflt)
{
	int v;
	Rune* rest;

	if(s == nil)
		return dflt;
	if(_lookup(color_tab, NCOLORS, s, _Strlen(s), &v))
		return v;
	if(s[0] == '#')
		s++;
	v = _Strtol(s, &rest, 16);
	if(*rest == 0)
		return v;
	return dflt;
}

/* Debugging */

#define HUGEPIX 10000

/* A "shallow" validitem, that doesn't follow next links */
/* or descend into tables. */
static int
validitem(Item* i)
{
	int ok;
	Itext* ti;
	Irule* ri;
	Iimage* ii;
	Ifloat* fi;
	int a;

	ok = (i->tag >= Itexttag && i->tag <= Ispacertag) &&
		(i->next == nil || validptr(i->next)) &&
		(i->width >= 0 && i->width < HUGEPIX) &&
		(i->height >= 0 && i->height < HUGEPIX) &&
		(i->ascent > -HUGEPIX && i->ascent < HUGEPIX) &&
		(i->anchorid >= 0) &&
		(i->genattr == nil || validptr(i->genattr));
	/* also, could check state for ridiculous combinations */
	/* also, could check anchorid for within-doc-range */
	if(ok)
		switch(i->tag) {
		case Itexttag:
			ti = (Itext*)i;
			ok = validStr(ti->s) &&
				(ti->fnt >= 0 && ti->fnt < NumStyle*NumSize) &&
				(ti->ul == ULnone || ti->ul == ULunder || ti->ul == ULmid);
			break;
		case Iruletag:
			ri = (Irule*)i;
			ok = (validvalign(ri->align) || validhalign(ri->align)) &&
				(ri->size >=0 && ri->size < HUGEPIX);
			break;
		case Iimagetag:
			ii = (Iimage*)i;
			ok = (ii->imsrc == nil || validptr(ii->imsrc)) &&
				(ii->item.width >= 0 && ii->item.width < HUGEPIX) &&
				(ii->item.height >= 0 && ii->item.height < HUGEPIX) &&
				(ii->imwidth >= 0 && ii->imwidth < HUGEPIX) &&
				(ii->imheight >= 0 && ii->imheight < HUGEPIX) &&
				(ii->altrep == nil || validStr(ii->altrep)) &&
				(ii->map == nil || validptr(ii->map)) &&
				(validvalign(ii->align) || validhalign(ii->align)) &&
				(ii->nextimage == nil || validptr(ii->nextimage));
			break;
		case Iformfieldtag:
			ok = validformfield(((Iformfield*)i)->formfield);
			break;
		case Itabletag:
			ok = validptr((Itable*)i);
			break;
		case Ifloattag:
			fi = (Ifloat*)i;
			ok = (fi->side == ALleft || fi->side == ALright) &&
				validitem(fi->item) &&
				(fi->item->tag == Iimagetag || fi->item->tag == Itabletag);
			break;
		case Ispacertag:
			a = ((Ispacer*)i)->spkind;
			ok = a==ISPnull || a==ISPvline || a==ISPhspace || a==ISPgeneral;
			break;
		default:
			ok = 0;
		}
	return ok;
}

/* "deep" validation, that checks whole list of items, */
/* and descends into tables and floated tables. */
/* nil is ok for argument. */
int
validitems(Item* i)
{
	int ok;
	Item* ii;

	ok = 1;
	while(i != nil && ok) {
		ok = validitem(i);
		if(ok) {
			if(i->tag == Itabletag) {
				ok = validtable(((Itable*)i)->table);
			}
			else if(i->tag == Ifloattag) {
				ii = ((Ifloat*)i)->item;
				if(ii->tag == Itabletag)
					ok = validtable(((Itable*)ii)->table);
			}
		}
		if(!ok) {
			fprint(2, "invalid item: %I\n", i);
		}
		i = i->next;
	}
	return ok;
}

static int
validformfield(Formfield* f)
{
	int ok;

	ok = (f->next == nil || validptr(f->next)) &&
		(f->ftype >= 0 && f->ftype <= Ftextarea) &&
		f->fieldid >= 0 &&
		(f->form == nil || validptr(f->form)) &&
		(f->name == nil || validStr(f->name)) &&
		(f->value == nil || validStr(f->value)) &&
		(f->options == nil || validptr(f->options)) &&
		(f->image == nil || validitem(f->image)) &&
		(f->events == nil || validptr(f->events));
	/* when all built, should have f->fieldid < f->form->nfields, */
	/* but this may be called during build... */
	return ok;
}

/* "deep" validation -- checks cell contents too */
static int
validtable(Table* t)
{
	int ok;
	int i, j;
	Tablecell* c;

	ok = (t->next == nil || validptr(t->next)) &&
		t->nrow >= 0 &&
		t->ncol >= 0 &&
		t->ncell >= 0 &&
		validalign(t->align) &&
		validdimen(t->width) &&
		(t->border >= 0 && t->border < HUGEPIX) &&
		(t->cellspacing >= 0 && t->cellspacing < HUGEPIX) &&
		(t->cellpadding >= 0 && t->cellpadding < HUGEPIX) &&
		validitems(t->caption) &&
		(t->caption_place == ALtop || t->caption_place == ALbottom) &&
		(t->totw >= 0 && t->totw < HUGEPIX) &&
		(t->toth >= 0 && t->toth < HUGEPIX) &&
		(t->tabletok == nil || validptr(t->tabletok));
	/* during parsing, t->rows has list; */
	/* only when parsing is done is t->nrow set > 0 */
	if(ok && t->nrow > 0 && t->ncol > 0) {
		/* table is "finished" */
		for(i = 0; i < t->nrow && ok; i++)
			ok = validtablerow(t->rows+i);
		for(j = 0; j < t->ncol && ok; j++)
			ok = validtablecol(t->cols+j);
		for(c = t->cells; c != nil && ok; c = c->next)
			ok = validtablecell(c);
		for(i = 0; i < t->nrow && ok; i++)
			for(j = 0; j < t->ncol && ok; j++)
				ok = validptr(t->grid[i][j]);
	}
	return ok;
}

static int
validvalign(int a)
{
	return a == ALnone || a == ALmiddle || a == ALbottom || a == ALtop || a == ALbaseline;
}

static int
validhalign(int a)
{
	return a == ALnone || a == ALleft || a == ALcenter || a == ALright ||
			a == ALjustify || a == ALchar;
}

static int
validalign(Align a)
{
	return validhalign(a.halign) && validvalign(a.valign);
}

static int
validdimen(Dimen d)
{
	int ok;
	int s;

	ok = 0;
	s = d.kindspec&Dspecmask;
	switch(d.kindspec&Dkindmask) {
	case Dnone:
		ok = s==0;
		break;
	case Dpixels:
		ok = s < HUGEPIX;
		break;
	case Dpercent:
	case Drelative:
		ok = 1;
		break;
	}
	return ok;
}

static int
validtablerow(Tablerow* r)
{
	return (r->cells == nil || validptr(r->cells)) &&
		(r->height >= 0 && r->height < HUGEPIX) &&
		(r->ascent > -HUGEPIX && r->ascent < HUGEPIX) &&
		validalign(r->align);
}

static int
validtablecol(Tablecol* c)
{
	return c->width >= 0 && c->width < HUGEPIX
		&& validalign(c->align);
}

static int
validtablecell(Tablecell* c)
{
	int ok;

	ok = (c->next == nil || validptr(c->next)) &&
		(c->nextinrow == nil || validptr(c->nextinrow)) &&
		(c->content == nil || validptr(c->content)) &&
		(c->lay == nil || validptr(c->lay)) &&
		c->rowspan >= 0 &&
		c->colspan >= 0 &&
		validalign(c->align) &&
		validdimen(c->wspec) &&
		c->row >= 0 &&
		c->col >= 0;
	if(ok) {
		if(c->content != nil)
			ok = validitems(c->content);
	}
	return ok;
}

static int
validptr(void* p)
{
	/* TODO: a better job of this. */
	/* For now, just dereference, which cause a bomb */
	/* if not valid */
	static char c;

	c = *((char*)p);
	USED(c);
	return 1;
}

static int
validStr(Rune* s)
{
	return s != nil && validptr(s);
}
