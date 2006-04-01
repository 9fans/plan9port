
/* UTILS */
typedef struct List List;
typedef struct Strlist Strlist;

/* List of integers (and also generic list with next pointer at beginning) */
struct List
{
	List*	next;
	int	val;
};

struct Strlist
{
	Strlist*	next;
	Rune*	val;
};

extern int		_inclass(Rune c, Rune* cl);
extern int		_listlen(List* l);
extern Rune*	_ltoStr(int n);
extern List*	_newlist(int val, List* rest);
extern Rune*	_newstr(int n);
extern int		_prefix(Rune* pre, Rune* s);
extern List*	_revlist(List* l);
extern void	_splitl(Rune* s, int n, Rune* cl, Rune** p1, int* n1, Rune** p2, int* n2);
extern void	_splitr(Rune* s, int n, Rune* cl, Rune** p1, int* n1, Rune** p2, int* n2);
extern int		_splitall(Rune* s, int n, Rune* cl, Rune** strarr, int* lenarr, int alen);
extern Rune*	_Stradd(Rune*s1, Rune* s2, int n);
extern Rune*	_Strclass(Rune* s, Rune* cl);
extern int		_Strcmp(Rune* s1, Rune* s2);
extern Rune*	_Strdup(Rune* s);
extern Rune*	_Strdup2(Rune* s, Rune* t);
extern int		_Streqn(Rune* s1, int n1, Rune* s2);
extern int		_Strlen(Rune* s);
extern Rune*	_Strnclass(Rune* s, Rune* cl, int n);
extern int		_Strncmpci(Rune* s1, int n1, Rune* s2);
extern Rune*	_Strndup(Rune* s, int n);
extern Rune*	_Strnrclass(Rune* s, Rune* cl, int n);
extern Rune*	_Strrclass(Rune* s, Rune* cl);
extern Rune*	_Strsubstr(Rune* s, int start, int stop);
extern long	_Strtol(Rune* s, Rune** eptr, int base);
extern void	_trimwhite(Rune* s, int n, Rune** pans, int* panslen);

extern Rune	notwhitespace[];
extern Rune	whitespace[];

/* STRINTTAB */
typedef struct StringInt StringInt;

/* Element of String-Int table (used for keyword lookup) */
struct StringInt
{
	Rune*	key;
	int	val;
};

extern int			_lookup(StringInt* t, int n, Rune* key, int keylen, int* pans);
extern StringInt*	_makestrinttab(Rune** a, int n);
extern Rune*		_revlookup(StringInt* t, int n, int val);

/* Colors, in html format, not Plan 9 format.  (RGB values in bottom 3 bytes) */
enum {
	White = 0xFFFFFF,
	Black = 0x000000,
	Blue = 0x0000CC
};

/* LEX */

/* HTML 4.0 tags (plus blink, nobr) */
/* sorted in lexical order; used as array indices */
enum {
	Notfound,
	Comment,
	Ta, Tabbr, Tacronym, Taddress, Tapplet, Tarea,
	Tb, Tbase, Tbasefont, Tbdo, Tbig, Tblink,
	Tblockquote, Tbody, Tbq, Tbr, Tbutton,
	Tcaption, Tcenter, Tcite, Tcode, Tcol, Tcolgroup,
	Tdd, Tdel, Tdfn, Tdir, Tdiv, Tdl, Tdt,
	Tem,
	Tfieldset, Tfont, Tform, Tframe, Tframeset,
	Th1, Th2, Th3, Th4, Th5, Th6,
	Thead, Thr, Thtml,
	Ti, Tiframe, Timg, Tinput, Tins, Tisindex,
	Tkbd,
	Tlabel, Tlegend, Tli, Tlink,
	Tmap, Tmenu, Tmeta,
	Tnobr, Tnoframes, Tnoscript,
	Tobject, Tol, Toptgroup, Toption,
	Tp, Tparam, Tpre,
	Tq,
	Ts, Tsamp, Tscript, Tselect, Tsmall,
	Tspan, Tstrike, Tstrong, Tstyle, Tsub, Tsup,
	Ttable, Ttbody, Ttd, Ttextarea, Ttfoot,
	Tth, Tthead, Ttitle, Ttr, Ttt,
	Tu, Tul,
	Tvar,
	Numtags,
	RBRA = Numtags,
	Data = Numtags+RBRA
};

/* HTML 4.0 tag attributes */
/* Keep sorted in lexical order */
enum {
	Aabbr, Aaccept_charset, Aaccess_key, Aaction,
	Aalign, Aalink, Aalt, Aarchive, Aaxis,
	Abackground, Abgcolor, Aborder,
	Acellpadding, Acellspacing, Achar, Acharoff,
	Acharset, Achecked, Acite, Aclass, Aclassid,
	Aclear, Acode, Acodebase, Acodetype, Acolor,
	Acols, Acolspan, Acompact, Acontent, Acoords,
	Adata, Adatetime, Adeclare, Adefer, Adir, Adisabled,
	Aenctype,
	Aface, Afor, Aframe, Aframeborder,
	Aheaders, Aheight, Ahref, Ahreflang, Ahspace, Ahttp_equiv,
	Aid, Aismap,
	Alabel, Alang, Alink, Alongdesc,
	Amarginheight, Amarginwidth, Amaxlength,
	Amedia, Amethod, Amultiple,
	Aname, Anohref, Anoresize, Anoshade, Anowrap,
	Aobject, Aonblur, Aonchange, Aonclick, Aondblclick,
	Aonfocus, Aonkeypress, Aonkeyup, Aonload,
	Aonmousedown, Aonmousemove, Aonmouseout,
	Aonmouseover, Aonmouseup, Aonreset, Aonselect,
	Aonsubmit, Aonunload,
	Aprofile, Aprompt,
	Areadonly, Arel, Arev, Arows, Arowspan, Arules,
	Ascheme, Ascope, Ascrolling, Aselected, Ashape,
	Asize, Aspan, Asrc, Astandby, Astart, Astyle, Asummary,
	Atabindex, Atarget, Atext, Atitle, Atype,
	Ausemap,
	Avalign, Avalue, Avaluetype, Aversion, Avlink, Avspace,
	Awidth,
	Numattrs
};

struct Attr
{
	Attr*		next;		/* in list of attrs for a token */
	int		attid;		/* Aabbr, etc. */
	Rune*	value;
};

struct Token
{
	int		tag;		/* Ta, etc */
	Rune*	text;		/* text in Data, attribute text in tag */
	Attr*		attr;		/* list of Attrs */
	int		starti;	/* index into source buffer of token start */
};

extern Rune**	tagnames;
extern Rune**	attrnames;

extern void	_freetokens(Token* tarray, int n);
extern Token*	_gettoks(uchar* data, int datalen, int chset, int mtype, int* plen);
extern int		_tokaval(Token* t, int attid, Rune** pans, int xfer);

/* #pragma varargck	type "T"	Token* */

#include "runetab.h"
