#ifndef _HTML_H_
#define _HTML_H_ 1
#ifdef __cplusplus
extern "C" {
#endif

AUTOLIB(html)
/*
 #pragma lib "libhtml.a"
 #pragma src "/sys/src/libhtml"
*/

/* UTILS */
extern uchar*	fromStr(Rune* buf, int n, int chset);
extern Rune*	toStr(uchar* buf, int n, int chset);

/* Common LEX and BUILD enums */

/* Media types */
enum
{
	ApplMsword,
	ApplOctets,
	ApplPdf,
	ApplPostscript,
	ApplRtf,
	ApplFramemaker,
	ApplMsexcel,
	ApplMspowerpoint,
	UnknownType,
	Audio32kadpcm,
	AudioBasic,
	ImageCgm,
	ImageG3fax,
	ImageGif,
	ImageIef,
	ImageJpeg,
	ImagePng,
	ImageTiff,
	ImageXBit,
	ImageXBit2,
	ImageXBitmulti,
	ImageXXBitmap,
	ModelVrml,
	MultiDigest,
	MultiMixed,
	TextCss,
	TextEnriched,
	TextHtml,
	TextJavascript,
	TextPlain,
	TextRichtext,
	TextSgml,
	TextTabSeparatedValues,
	TextXml,
	VideoMpeg,
	VideoQuicktime,
	NMEDIATYPES
};

/* HTTP methods */
enum
{
	HGet,
	HPost
};

/* Charsets */
enum
{
	UnknownCharset,
	US_Ascii,
	ISO_8859_1,
	UTF_8,
	Unicode,
	NCHARSETS
};

/* Frame Target IDs */
enum {
	FTtop,
	FTself,
	FTparent,
	FTblank
};

/* LEX */
typedef struct Token Token;
typedef struct Attr Attr;

/* BUILD */

typedef struct Item Item;
typedef struct Itext Itext;
typedef struct Irule Irule;
typedef struct Iimage Iimage;
typedef struct Iformfield Iformfield;
typedef struct Itable Itable;
typedef struct Ifloat Ifloat;
typedef struct Ispacer Ispacer;
typedef struct Genattr Genattr;
typedef struct SEvent SEvent;
typedef struct Formfield Formfield;
typedef struct Option Option;
typedef struct Form Form;
typedef struct Table Table;
typedef struct Tablecol Tablecol;
typedef struct Tablerow Tablerow;
typedef struct Tablecell Tablecell;
typedef struct Align Align;
typedef struct Dimen Dimen;
typedef struct Anchor Anchor;
typedef struct DestAnchor DestAnchor;
typedef struct Map Map;
typedef struct Area Area;
typedef struct Background Background;
typedef struct Kidinfo Kidinfo;
typedef struct Docinfo Docinfo;
typedef struct Stack Stack;
typedef struct Pstate Pstate;
typedef struct ItemSource ItemSource;
typedef struct Lay Lay;	/* defined in Layout module */

/* Alignment types */
enum {
	ALnone = 0, ALleft, ALcenter, ALright, ALjustify,
	ALchar, ALtop, ALmiddle, ALbottom, ALbaseline
};

struct Align
{
	uchar	halign;	/* one of ALnone, ALleft, etc. */
	uchar	valign;	/* one of ALnone, ALtop, etc. */
};

/* A Dimen holds a dimension specification, especially for those */
/* cases when a number can be followed by a % or a * to indicate */
/* percentage of total or relative weight. */
/* Dnone means no dimension was specified */

/* To fit in a word, use top bits to identify kind, rest for value */
enum {
	Dnone =		0,
	Dpixels =		(1<<29),
	Dpercent =	(2<<29),
	Drelative =	(3<<29),
	Dkindmask =	(3<<29),
	Dspecmask =	(~Dkindmask)
};

struct Dimen
{
	int	kindspec;		/* kind | spec */
};

/* Background is either an image or a color. */
/* If both are set, the image has precedence. */
struct Background
{
	Rune*	image;	/* url */
	int		color;
};


/* There are about a half dozen Item variants. */
/* The all look like this at the start (using Plan 9 C's */
/* anonymous structure member mechanism), */
/* and then the tag field dictates what extra fields there are. */
struct Item
{
	Item*	next;		/* successor in list of items */
	int		width;	/* width in pixels (0 for floating items) */
	int		height;	/* height in pixels */
	Rectangle	r;
	int		ascent;	/* ascent (from top to baseline) in pixels */
	int		anchorid;	/* if nonzero, which anchor we're in */
	int		state;	/* flags and values (see below) */
	Genattr*	genattr;	/* generic attributes and events */
	int		tag;		/* variant discriminator: Itexttag, etc. */
};

/* Item variant tags */
enum {
	Itexttag,
	Iruletag,
	Iimagetag,
	Iformfieldtag,
	Itabletag,
	Ifloattag,
	Ispacertag
};

struct Itext
{
	Item item;				/* (with tag ==Itexttag) */
	Rune*	s;			/* the characters */
	int		fnt;			/* style*NumSize+size (see font stuff, below) */
	int		fg;			/* Pixel (color) for text */
	uchar	voff;			/* Voffbias+vertical offset from baseline, in pixels (+ve == down) */
	uchar	ul;			/* ULnone, ULunder, or ULmid */
};

struct Irule
{
	Item item;				/* (with tag ==Iruletag) */
	uchar	align;		/* alignment spec */
	uchar	noshade;		/* if true, don't shade */
	int		size;			/* size attr (rule height) */
	Dimen	wspec;		/* width spec */
};


struct Iimage
{
	Item item;				/* (with tag ==Iimagetag) */
	Rune*	imsrc;		/* image src url */
	int		imwidth;		/* spec width (actual, if no spec) */
	int		imheight;		/* spec height (actual, if no spec) */
	Rune*	altrep;		/* alternate representation, in absence of image */
	Map*	map;			/* if non-nil, client side map */
	int		ctlid;			/* if animated */
	uchar	align;		/* vertical alignment */
	uchar	hspace;		/* in pixels; buffer space on each side */
	uchar	vspace;		/* in pixels; buffer space on top and bottom */
	uchar	border;		/* in pixels: border width to draw around image */
	Iimage*	nextimage;	/* next in list of document's images */
	void *aux;
};


struct Iformfield
{
	Item item;				/* (with tag ==Iformfieldtag) */
	Formfield*	formfield;
	void *aux;
};


struct Itable
{
	Item item;				/* (with tag ==Itabletag) */
	Table*	table;
};


struct Ifloat
{
	Item _item;				/* (with tag ==Ifloattag) */
	Item*	item;			/* table or image item that floats */
	int		x;			/* x coord of top (from right, if ALright) */
	int		y;			/* y coord of top */
	uchar	side;			/* margin it floats to: ALleft or ALright */
	uchar	infloats;		/* true if this has been added to a lay.floats */
	Ifloat*	nextfloat;		/* in list of floats */
};


struct Ispacer
{
	Item item;				/* (with tag ==Ispacertag) */
	int		spkind;		/* ISPnull, etc. */
};

/* Item state flags and value fields */
enum {
/*	IFbrk =			0x80000000,	// forced break before this item */
#define	IFbrk		0x80000000 /* too big for sun */
	IFbrksp =			0x40000000,	/* add 1 line space to break (IFbrk set too) */
	IFnobrk =			0x20000000,	/* break not allowed before this item */
	IFcleft =			0x10000000,	/* clear left floats (IFbrk set too) */
	IFcright =			0x08000000,	/* clear right floats (IFbrk set too) */
	IFwrap =			0x04000000,	/* in a wrapping (non-pre) line */
	IFhang =			0x02000000,	/* in a hanging (into left indent) item */
	IFrjust =			0x01000000,	/* right justify current line */
	IFcjust =			0x00800000,	/* center justify current line */
	IFsmap =			0x00400000,	/* image is server-side map */
	IFindentshift =		8,
	IFindentmask =		(255<<IFindentshift),	/* current indent, in tab stops */
	IFhangmask =		255			/* current hang into left indent, in 1/10th tabstops */
};

/* Bias added to Itext's voff field */
enum { Voffbias = 128 };

/* Spacer kinds */
enum {
	ISPnull,			/* 0 height and width */
	ISPvline,			/* height and ascent of current font */
	ISPhspace,		/* width of space in current font */
	ISPgeneral		/* other purposes (e.g., between markers and list) */
};

/* Generic attributes and events (not many elements will have any of these set) */
struct Genattr
{
	Rune*	id;
	Rune*	class;
	Rune*	style;
	Rune*	title;
	SEvent*	events;
};

struct SEvent
{
	SEvent*	next;		/* in list of events */
	int		type;		/* SEonblur, etc. */
	Rune*	script;
};

enum {
	SEonblur, SEonchange, SEonclick, SEondblclick,
	SEonfocus, SEonkeypress, SEonkeyup, SEonload,
	SEonmousedown, SEonmousemove, SEonmouseout,
	SEonmouseover, SEonmouseup, SEonreset, SEonselect,
	SEonsubmit, SEonunload,
	Numscriptev
};

/* Form field types */
enum {
	Ftext,
	Fpassword,
	Fcheckbox,
	Fradio,
	Fsubmit,
	Fhidden,
	Fimage,
	Freset,
	Ffile,
	Fbutton,
	Fselect,
	Ftextarea
};

/* Information about a field in a form */
struct Formfield
{
	Formfield*	next;		/* in list of fields for a form */
	int			ftype;	/* Ftext, Fpassword, etc. */
	int			fieldid;	/* serial no. of field within its form */
	Form*		form;	/* containing form */
	Rune*		name;	/* name attr */
	Rune*		value;	/* value attr */
	int			size;		/* size attr */
	int			maxlength;	/* maxlength attr */
	int			rows;	/* rows attr */
	int			cols;		/* cols attr */
	uchar		flags;	/* FFchecked, etc. */
	Option*		options;	/* for Fselect fields */
	Item*		image;	/* image item, for Fimage fields */
	int			ctlid;		/* identifies control for this field in layout */
	SEvent*		events;	/* same as genattr->events of containing item */
};

enum {
	FFchecked =	(1<<7),
	FFmultiple =	(1<<6)
};

/* Option holds info about an option in a "select" form field */
struct Option
{
	Option*	next;			/* next in list of options for a field */
	int		selected;		/* true if selected initially */
	Rune*	value;		/* value attr */
	Rune*	display;		/* display string */
};

/* Form holds info about a form */
struct Form
{
	Form*		next;		/* in list of forms for document */
	int			formid;	/* serial no. of form within its doc */
	Rune*		name;	/* name or id attr (netscape uses name, HTML 4.0 uses id) */
	Rune*		action;	/* action attr */
	int			target;	/* target attr as targetid */
	int			method;	/* HGet or HPost */
	int			nfields;	/* number of fields */
	Formfield*	fields;	/* field's forms, in input order */
};

/* Flags used in various table structures */
enum {
	TFparsing =	(1<<7),
	TFnowrap =	(1<<6),
	TFisth =		(1<<5)
};


/* Information about a table */
struct Table
{
	Table*		next;			/* next in list of document's tables */
	int			tableid;		/* serial no. of table within its doc */
	Tablerow*	rows;		/* array of row specs (list during parsing) */
	int			nrow;		/* total number of rows */
	Tablecol*		cols;			/* array of column specs */
	int			ncol;			/* total number of columns */
	Tablecell*		cells;			/* list of unique cells */
	int			ncell;		/* total number of cells */
	Tablecell***	grid;			/* 2-D array of cells */
	Align		align;		/* alignment spec for whole table */
	Dimen		width;		/* width spec for whole table */
	int			border;		/* border attr */
	int			cellspacing;	/* cellspacing attr */
	int			cellpadding;	/* cellpadding attr */
	Background	background;	/* table background */
	Item*		caption;		/* linked list of Items, giving caption */
	uchar		caption_place;	/* ALtop or ALbottom */
	Lay*			caption_lay;	/* layout of caption */
	int			totw;			/* total width */
	int			toth;			/* total height */
	int			caph;		/* caption height */
	int			availw;		/* used for previous 3 sizes */
	Token*		tabletok;		/* token that started the table */
	uchar		flags;		/* Lchanged, perhaps */
};


struct Tablecol
{
	int		width;
	Align	align;
	Point		pos;
};


struct Tablerow
{
	Tablerow*	next;			/* Next in list of rows, during parsing */
	Tablecell*		cells;			/* Cells in row, linked through nextinrow */
	int			height;
	int			ascent;
	Align		align;
	Background	background;
	Point			pos;
	uchar		flags;		/* 0 or TFparsing */
};


/* A Tablecell is one cell of a table. */
/* It may span multiple rows and multiple columns. */
/* Cells are linked on two lists: the list for all the cells of */
/* a document (the next pointers), and the list of all the */
/* cells that start in a given row (the nextinrow pointers) */
struct Tablecell
{
	Tablecell*		next;			/* next in list of table's cells */
	Tablecell*		nextinrow;	/* next in list of row's cells */
	int			cellid;		/* serial no. of cell within table */
	Item*		content;		/* contents before layout */
	Lay*			lay;			/* layout of cell */
	int			rowspan;		/* number of rows spanned by this cell */
	int			colspan;		/* number of cols spanned by this cell */
	Align		align;		/* alignment spec */
	uchar		flags;		/* TFparsing, TFnowrap, TFisth */
	Dimen		wspec;		/* suggested width */
	int			hspec;		/* suggested height */
	Background	background;	/* cell background */
	int			minw;		/* minimum possible width */
	int			maxw;		/* maximum width */
	int			ascent;		/* cell's ascent */
	int			row;			/* row of upper left corner */
	int			col;			/* col of upper left corner */
	Point			pos;			/* nw corner of cell contents, in cell */
};

/* Anchor is for info about hyperlinks that go somewhere */
struct Anchor
{
	Anchor*		next;		/* next in list of document's anchors */
	int			index;	/* serial no. of anchor within its doc */
	Rune*		name;	/* name attr */
	Rune*		href;		/* href attr */
	int			target;	/* target attr as targetid */
};


/* DestAnchor is for info about hyperlinks that are destinations */
struct DestAnchor
{
	DestAnchor*	next;		/* next in list of document's destanchors */
	int			index;	/* serial no. of anchor within its doc */
	Rune*		name;	/* name attr */
	Item*		item;		/* the destination */
};


/* Maps (client side) */
struct Map
{
	Map*	next;			/* next in list of document's maps */
	Rune*	name;		/* map name */
	Area*	areas;		/* list of map areas */
};


struct Area
{
	Area*		next;		/* next in list of a map's areas */
	int			shape;	/* SHrect, etc. */
	Rune*		href;		/* associated hypertext link */
	int			target;	/* associated target frame */
	Dimen*		coords;	/* array of coords for shape */
	int			ncoords;	/* size of coords array */
};

/* Area shapes */
enum {
	SHrect, SHcircle, SHpoly
};

/* Fonts are represented by integers: style*NumSize + size */

/* Font styles */
enum {
	FntR,			/* roman */
	FntI,			/* italic */
	FntB,			/* bold */
	FntT,			/* typewriter */
	NumStyle
};

/* Font sizes */
enum {
	Tiny,
	Small,
	Normal,
	Large,
	Verylarge,
	NumSize
};

enum {
	NumFnt = (NumStyle*NumSize),
	DefFnt = (FntR*NumSize+Normal)
};

/* Lines are needed through some text items, for underlining or strikethrough */
enum {
	ULnone, ULunder, ULmid
};

/* Kidinfo flags */
enum {
	FRnoresize =	(1<<0),
	FRnoscroll =	(1<<1),
	FRhscroll = 	(1<<2),
	FRvscroll =	(1<<3),
	FRhscrollauto = (1<<4),
	FRvscrollauto =	(1<<5)
};

/* Information about child frame or frameset */
struct Kidinfo
{
	Kidinfo*		next;		/* in list of kidinfos for a frameset */
	int			isframeset;

	/* fields for "frame" */
	Rune*		src;		/* only nil if a "dummy" frame or this is frameset */
	Rune*		name;	/* always non-empty if this isn't frameset */
	int			marginw;
	int			marginh;
	int			framebd;
	int			flags;

	/* fields for "frameset" */
	Dimen*		rows;	/* array of row dimensions */
	int			nrows;	/* length of rows */
	Dimen*		cols;		/* array of col dimensions */
	int			ncols;	/* length of cols */
	Kidinfo*		kidinfos;
	Kidinfo*		nextframeset;	/* parsing stack */
};


/* Document info (global information about HTML page) */
struct Docinfo
{
	/* stuff from HTTP headers, doc head, and body tag */
	Rune*		src;				/* original source of doc */
	Rune*		base;			/* base URL of doc */
	Rune*		doctitle;			/* from <title> element */
	Background	background;		/* background specification */
	Iimage*		backgrounditem;	/* Image Item for doc background image, or nil */
	int			text;				/* doc foreground (text) color */
	int			link;				/* unvisited hyperlink color */
	int			vlink;			/* visited hyperlink color */
	int			alink;			/* highlighting hyperlink color */
	int			target;			/* target frame default */
	int			chset;			/* ISO_8859, etc. */
	int			mediatype;		/* TextHtml, etc. */
	int			scripttype;		/* TextJavascript, etc. */
	int			hasscripts;		/* true if scripts used */
	Rune*		refresh;			/* content of <http-equiv=Refresh ...> */
	Kidinfo*		kidinfo;			/* if a frameset */
	int			frameid;			/* id of document frame */

	/* info needed to respond to user actions */
	Anchor*		anchors;			/* list of href anchors */
	DestAnchor*	dests;			/* list of destination anchors */
	Form*		forms;			/* list of forms */
	Table*		tables;			/* list of tables */
	Map*		maps;			/* list of maps */
	Iimage*		images;			/* list of image items (through nextimage links) */
};

extern int			dimenkind(Dimen d);
extern int			dimenspec(Dimen d);
extern void		freedocinfo(Docinfo* d);
extern void		freeitems(Item* ithead);
extern Item*		parsehtml(uchar* data, int datalen, Rune* src, int mtype, int chset, Docinfo** pdi);
extern void		printitems(Item* items, char* msg);
extern int			targetid(Rune* s);
extern Rune*		targetname(int targid);
extern int			validitems(Item* i);

/* #pragma varargck	type "I"	Item* */

/* Control print output */
extern int			warn;
extern int			dbglex;
extern int			dbgbuild;

/* To be provided by caller */
/* emalloc and erealloc should not return if can't get memory. */
/* emalloc should zero its memory. */
extern void*	emalloc(ulong);
extern void*	erealloc(void* p, ulong size);
#ifdef __cpluspplus
}
#endif
#endif
