/* thanks to Caerwyn Jones <caerwyn@comcast.net> for this module */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include "dict.h"

enum {
	Buflen=1000,
	Maxaux=5
};

/* Possible tags */
enum {
	B,		/* Bold */
	Blockquote,	/* Block quote */
	Br,		/* Break line */
	Cd,		/* ? coloquial data */
	Col,		/* ? Coloquial */
	Def,		/* Definition */
	Hw, 		/* Head Word */
	I,		/* Italics */
	P,		/* Paragraph */
	Pos,		/* Part of Speach */
	Sn,		/* Sense */
	U,		/* ? cross reference*/
	Wf,		/* ? word form */
	Ntag		/* end of tags */
};

/* Assoc tables must be sorted on first field */

static Assoc tagtab[] = {
	{"b",			B},
	{"blockquote",	Blockquote},
	{"BR",		Br},
	{"cd",		Cd},
	{"col",		Col},
	{"def",		Def},
	{"hw",		Hw},
	{"i",			I},
	{"p",			P},
	{"pos",		Pos},
	{"sn",		Sn},
	{"u",			U},
	{"wf",		Wf}
};

/* Possible tag auxilliary info */
enum {
	Cols,		/* number of columns in a table */
	Num,		/* letter or number, for a sense */
	St,		/* status (e.g., obs) */
	Naux
};

#if 0
static Assoc auxtab[] = {
	{"cols",	Cols},
	{"num",		Num},
	{"st",		St}
};
#endif

static Assoc spectab[] = {
	{"3on4",	0xbe},
	{"AElig",		0xc6},
	{"Aacute",	0xc1},
	{"Aang",	0xc5},
	{"Abarab",	0x100},
	{"Acirc",	0xc2},
	{"Agrave",	0xc0},
	{"Alpha",	0x391},
	{"Amacr",	0x100},
	{"Asg",		0x1b7},		/* Unicyle. Cf "Sake" */
	{"Auml",	0xc4},
	{"Beta",	0x392},
	{"Cced",	0xc7},
	{"Chacek",	0x10c},
	{"Chi",		0x3a7},
	{"Chirho",	0x2627},		/* Chi Rho U+2627 */
	{"Csigma",	0x3da},
	{"Delta",	0x394},
	{"Eacute",	0xc9},
	{"Ecirc",	0xca},
	{"Edh",		0xd0},
	{"Epsilon",	0x395},
	{"Eta",		0x397},
	{"Gamma",	0x393},
	{"Iacute",	0xcd},
	{"Icirc",	0xce},
	{"Imacr",	0x12a},
	{"Integ",	0x222b},
	{"Iota",	0x399},
	{"Kappa",	0x39a},
	{"Koppa",	0x3de},
	{"Lambda",	0x39b},
	{"Lbar",	0x141},
	{"Mu",		0x39c},
	{"Naira",	0x4e},		/* should have bar through */
	{"Nplus",	0x4e},		/* should have plus above */
	{"Ntilde",	0xd1},
	{"Nu",		0x39d},
	{"Oacute",	0xd3},
	{"Obar",	0xd8},
	{"Ocirc",	0xd4},
	{"Oe",		0x152},
	{"Omega",	0x3a9},
	{"Omicron",	0x39f},
	{"Ouml",	0xd6},
	{"Phi",		0x3a6},
	{"Pi",		0x3a0},
	{"Psi",		0x3a8},
	{"Rho",		0x3a1},
	{"Sacute",	0x15a},
	{"Sigma",	0x3a3},
	{"Summ",	0x2211},
	{"Tau",		0x3a4},
	{"Th",		0xde},
	{"Theta",	0x398},
	{"Tse",		0x426},
	{"Uacute",	0xda},
	{"Ucirc",	0xdb},
	{"Upsilon",	0x3a5},
	{"Uuml",	0xdc},
	{"Wyn",		0x1bf},		/* wynn U+01BF */
	{"Xi",		0x39e},
	{"Ygh",		0x1b7},		/* Yogh	U+01B7 */
	{"Zeta",	0x396},
	{"Zh",		0x1b7},		/* looks like Yogh. Cf "Sake" */
	{"a",		0x61},		/* ante */
	{"aacute",	0xe1},
	{"aang",	0xe5},
	{"aasper",	MAAS},
	{"abreve",	0x103},
	{"acirc",	0xe2},
	{"acute",		LACU},
	{"aelig",		0xe6},
	{"agrave",	0xe0},
	{"ahook",	0x105},
	{"alenis",	MALN},
	{"alpha",	0x3b1},
	{"amacr",	0x101},
	{"amp",		0x26},
	{"and",		MAND},
	{"ang",		LRNG},
	{"angle",	0x2220},
	{"ankh",	0x2625},		/* ankh U+2625 */
	{"ante",	0x61},		/* before (year) */
	{"aonq",	MAOQ},
	{"appreq",	0x2243},
	{"aquar",	0x2652},
	{"arDadfull",	0x636},		/* Dad U+0636 */
	{"arHa",	0x62d},		/* haa U+062D */
	{"arTa",	0x62a},		/* taa U+062A */
	{"arain",	0x639},		/* ain U+0639 */
	{"arainfull",	0x639},		/* ain U+0639 */
	{"aralif",	0x627},		/* alef U+0627 */
	{"arba",	0x628},		/* baa U+0628 */
	{"arha",	0x647},		/* ha U+0647 */
	{"aries",	0x2648},
	{"arnun",	0x646},		/* noon U+0646 */
	{"arnunfull",	0x646},		/* noon U+0646 */
	{"arpa",	0x647},		/* ha U+0647 */
	{"arqoph",	0x642},		/* qaf U+0642 */
	{"arshinfull",	0x634},		/* sheen U+0634 */
	{"arta",	0x62a},		/* taa U+062A */
	{"artafull",	0x62a},		/* taa U+062A */
	{"artha",	0x62b},		/* thaa U+062B */
	{"arwaw",	0x648},		/* waw U+0648 */
	{"arya",	0x64a},		/* ya U+064A */
	{"aryafull",	0x64a},		/* ya U+064A */
	{"arzero",	0x660},		/* indic zero U+0660 */
	{"asg",		0x292},		/* unicycle character. Cf "hallow" */
	{"asper",	LASP},
	{"assert",	0x22a2},
	{"astm",	0x2042},		/* asterism: should be upside down */
	{"at",		0x40},
	{"atilde",	0xe3},
	{"auml",	0xe4},
	{"ayin",	0x639},		/* arabic ain U+0639 */
	{"b1",		0x2d},		/* single bond */
	{"b2",		0x3d},		/* double bond */
	{"b3",		0x2261},		/* triple bond */
	{"bbar",	0x180},		/* b with bar U+0180 */
	{"beta",	0x3b2},
	{"bigobl",	0x2f},
	{"blC",		0x43},		/* should be black letter */
	{"blJ",		0x4a},		/* should be black letter */
	{"blU",		0x55},		/* should be black letter */
	{"blb",		0x62},		/* should be black letter */
	{"blozenge",	0x25ca},		/* U+25CA; should be black */
	{"bly",		0x79},		/* should be black letter */
	{"bra",		MBRA},
	{"brbl",	LBRB},
	{"breve",	LBRV},
	{"bslash",'\\'},
	{"bsquare",	0x25a0},		/* black square U+25A0 */
	{"btril",	0x25c0},		/* U+25C0 */
	{"btrir",	0x25b6},		/* U+25B6 */
	{"c",		0x63},		/* circa */
	{"cab",		0x232a},
	{"cacute",	0x107},
	{"canc",	0x264b},
	{"capr",	0x2651},
	{"caret",	0x5e},
	{"cb",		0x7d},
	{"cbigb",	0x7d},
	{"cbigpren",	0x29},
	{"cbigsb",	0x5d},
	{"cced",	0xe7},
	{"cdil",	LCED},
	{"cdsb",	0x301b},		/* ]] U+301b */
	{"cent",	0xa2},
	{"chacek",	0x10d},
	{"chi",		0x3c7},
	{"circ",	LRNG},
	{"circa",	0x63},		/* about (year) */
	{"circbl",	0x325},		/* ring below accent U+0325 */
	{"circle",	0x25cb},		/* U+25CB */
	{"circledot",	0x2299},
	{"click",	0x296},
	{"club",	0x2663},
	{"comtime",	0x43},
	{"conj",	0x260c},
	{"cprt",	0xa9},
	{"cq",		'\''},
	{"cqq",		0x201d},
	{"cross",	0x2720},		/* maltese cross U+2720 */
	{"crotchet",	0x2669},
	{"csb",		0x5d},
	{"ctilde",	0x63},		/* +tilde */
	{"ctlig",	MLCT},
	{"cyra",	0x430},
	{"cyre",	0x435},
	{"cyrhard",	0x44a},
	{"cyrjat",	0x463},
	{"cyrm",	0x43c},
	{"cyrn",	0x43d},
	{"cyrr",	0x440},
	{"cyrsoft",	0x44c},
	{"cyrt",	0x442},
	{"cyry",	0x44b},
	{"dag",		0x2020},
	{"dbar",	0x111},
	{"dblar",	0x21cb},
	{"dblgt",	0x226b},
	{"dbllt",	0x226a},
	{"dced",	0x64},		/* +cedilla */
	{"dd",		MDD},
	{"ddag",	0x2021},
	{"ddd",		MDDD},
	{"decr",	0x2193},
	{"deg",		0xb0},
	{"dele",	0x64},		/* should be dele */
	{"delta",	0x3b4},
	{"descnode",	0x260b},		/* descending node U+260B */
	{"diamond",	0x2662},
	{"digamma",	0x3dd},
	{"div",		0xf7},
	{"dlessi",	0x131},
	{"dlessj1",	0x6a},		/* should be dotless */
	{"dlessj2",	0x6a},		/* should be dotless */
	{"dlessj3",	0x6a},		/* should be dotless */
	{"dollar",	0x24},
	{"dotab",	LDOT},
	{"dotbl",	LDTB},
	{"drachm",	0x292},
	{"dubh",	0x2d},
	{"eacute",	0xe9},
	{"earth",	0x2641},
	{"easper",	MEAS},
	{"ebreve",	0x115},
	{"ecirc",	0xea},
	{"edh",		0xf0},
	{"egrave",	0xe8},
	{"ehacek",	0x11b},
	{"ehook",	0x119},
	{"elem",	0x220a},
	{"elenis",	MELN},
	{"em",		0x2014},
	{"emacr",	0x113},
	{"emem",	MEMM},
	{"en",		0x2013},
	{"epsilon",	0x3b5},
	{"equil",	0x21cb},
	{"ergo",	0x2234},
	{"es",		MES},
	{"eszett",	0xdf},
	{"eta",		0x3b7},
	{"eth",		0xf0},
	{"euml",	0xeb},
	{"expon",	0x2191},
	{"fact",	0x21},
	{"fata",	0x251},
	{"fatpara",	0xb6},		/* should have fatter, filled in bowl */
	{"female",	0x2640},
	{"ffilig",	MLFFI},
	{"fflig",	MLFF},
	{"ffllig",	MLFFL},
	{"filig",	MLFI},
	{"flat",	0x266d},
	{"fllig",	MLFL},
	{"frE",		0x45},		/* should be curly */
	{"frL",	'L'},		/* should be curly */
	{"frR",		0x52},		/* should be curly */
	{"frakB",	0x42},		/* should have fraktur style */
	{"frakG",	0x47},
	{"frakH",	0x48},
	{"frakI",	0x49},
	{"frakM",	0x4d},
	{"frakU",	0x55},
	{"frakX",	0x58},
	{"frakY",	0x59},
	{"frakh",	0x68},
	{"frbl",	LFRB},
	{"frown",	LFRN},
	{"fs",		0x20},
	{"fsigma",	0x3c2},
	{"gAacute",	0xc1},		/* should be Α+acute */
	{"gaacute",	0x3b1},		/* +acute */
	{"gabreve",	0x3b1},		/* +breve */
	{"gafrown",	0x3b1},		/* +frown */
	{"gagrave",	0x3b1},		/* +grave */
	{"gamacr",	0x3b1},		/* +macron */
	{"gamma",	0x3b3},
	{"gauml",	0x3b1},		/* +umlaut */
	{"ge",		0x2267},
	{"geacute",	0x3b5},		/* +acute */
	{"gegrave",	0x3b5},		/* +grave */
	{"ghacute",	0x3b7},		/* +acute */
	{"ghfrown",	0x3b7},		/* +frown */
	{"ghgrave",	0x3b7},		/* +grave */
	{"ghmacr",	0x3b7},		/* +macron */
	{"giacute",	0x3b9},		/* +acute */
	{"gibreve",	0x3b9},		/* +breve */
	{"gifrown",	0x3b9},		/* +frown */
	{"gigrave",	0x3b9},		/* +grave */
	{"gimacr",	0x3b9},		/* +macron */
	{"giuml",	0x3b9},		/* +umlaut */
	{"glagjat",	0x467},
	{"glots",	0x2c0},
	{"goacute",	0x3bf},		/* +acute */
	{"gobreve",	0x3bf},		/* +breve */
	{"grave",	LGRV},
	{"gt",		0x3e},
	{"guacute",	0x3c5},		/* +acute */
	{"gufrown",	0x3c5},		/* +frown */
	{"gugrave",	0x3c5},		/* +grave */
	{"gumacr",	0x3c5},		/* +macron */
	{"guuml",	0x3c5},		/* +umlaut */
	{"gwacute",	0x3c9},		/* +acute */
	{"gwfrown",	0x3c9},		/* +frown */
	{"gwgrave",	0x3c9},		/* +grave */
	{"hacek",	LHCK},
	{"halft",	0x2308},
	{"hash",	0x23},
	{"hasper",	MHAS},
	{"hatpath",	0x5b2},		/* hataf patah U+05B2 */
	{"hatqam",	0x5b3},		/* hataf qamats U+05B3 */
	{"hatseg",	0x5b1},		/* hataf segol U+05B1 */
	{"hbar",	0x127},
	{"heart",	0x2661},
	{"hebaleph",	0x5d0},		/* aleph U+05D0 */
	{"hebayin",	0x5e2},		/* ayin U+05E2 */
	{"hebbet",	0x5d1},		/* bet U+05D1 */
	{"hebbeth",	0x5d1},		/* bet U+05D1 */
	{"hebcheth",	0x5d7},		/* bet U+05D7 */
	{"hebdaleth",	0x5d3},		/* dalet U+05D3 */
	{"hebgimel",	0x5d2},		/* gimel U+05D2 */
	{"hebhe",	0x5d4},		/* he U+05D4 */
	{"hebkaph",	0x5db},		/* kaf U+05DB */
	{"heblamed",	0x5dc},		/* lamed U+05DC */
	{"hebmem",	0x5de},		/* mem U+05DE */
	{"hebnun",	0x5e0},		/* nun U+05E0 */
	{"hebnunfin",	0x5df},		/* final nun U+05DF */
	{"hebpe",	0x5e4},		/* pe U+05E4 */
	{"hebpedag",	0x5e3},		/* final pe? U+05E3 */
	{"hebqoph",	0x5e7},		/* qof U+05E7 */
	{"hebresh",	0x5e8},		/* resh U+05E8 */
	{"hebshin",	0x5e9},		/* shin U+05E9 */
	{"hebtav",	0x5ea},		/* tav U+05EA */
	{"hebtsade",	0x5e6},		/* tsadi U+05E6 */
	{"hebwaw",	0x5d5},		/* vav? U+05D5 */
	{"hebyod",	0x5d9},		/* yod U+05D9 */
	{"hebzayin",	0x5d6},		/* zayin U+05D6 */
	{"hgz",		0x292},		/* ??? Cf "alet" */
	{"hireq",	0x5b4},		/* U+05B4 */
	{"hlenis",	MHLN},
	{"hook",	LOGO},
	{"horizE",	0x45},		/* should be on side */
	{"horizP",	0x50},		/* should be on side */
	{"horizS",	0x223d},
	{"horizT",	0x22a3},
	{"horizb",	0x7b},		/* should be underbrace */
	{"ia",		0x3b1},
	{"iacute",	0xed},
	{"iasper",	MIAS},
	{"ib",		0x3b2},
	{"ibar",	0x268},
	{"ibreve",	0x12d},
	{"icirc",	0xee},
	{"id",		0x3b4},
	{"ident",	0x2261},
	{"ie",		0x3b5},
	{"ifilig",	MLFI},
	{"ifflig",	MLFF},
	{"ig",		0x3b3},
	{"igrave",	0xec},
	{"ih",		0x3b7},
	{"ii",		0x3b9},
	{"ik",		0x3ba},
	{"ilenis",	MILN},
	{"imacr",	0x12b},
	{"implies",	0x21d2},
	{"index",	0x261e},
	{"infin",	0x221e},
	{"integ",	0x222b},
	{"intsec",	0x2229},
	{"invpri",	0x2cf},
	{"iota",	0x3b9},
	{"iq",		0x3c8},
	{"istlig",	MLST},
	{"isub",	0x3f5},		/* iota below accent */
	{"iuml",	0xef},
	{"iz",		0x3b6},
	{"jup",		0x2643},
	{"kappa",	0x3ba},
	{"koppa",	0x3df},
	{"lambda",	0x3bb},
	{"lar",		0x2190},
	{"lbar",	0x142},
	{"le",		0x2266},
	{"lenis",	LLEN},
	{"leo",		0x264c},
	{"lhalfbr",	0x2308},
	{"lhshoe",	0x2283},
	{"libra",	0x264e},
	{"llswing",	MLLS},
	{"lm",		0x2d0},
	{"logicand",	0x2227},
	{"logicor",	0x2228},
	{"longs",	0x283},
	{"lrar",	0x2194},
	{"lt",		0x3c},
	{"ltappr",	0x227e},
	{"ltflat",	0x2220},
	{"lumlbl",	0x6c},		/* +umlaut below */
	{"mac",		LMAC},
	{"male",	0x2642},
	{"mc",		0x63},		/* should be raised */
	{"merc",	0x263f},		/* mercury U+263F */
	{"min",		0x2212},
	{"moonfq",	0x263d},		/* first quarter moon U+263D */
	{"moonlq",	0x263e},		/* last quarter moon U+263E */
	{"msylab",	0x6d},		/* +sylab (ˌ) */
	{"mu",		0x3bc},
	{"nacute",	0x144},
	{"natural",	0x266e},
	{"neq",		0x2260},
	{"nfacute",	0x2032},
	{"nfasper",	0x2bd},
	{"nfbreve",	0x2d8},
	{"nfced",	0xb8},
	{"nfcirc",	0x2c6},
	{"nffrown",	0x2322},
	{"nfgra",	0x2cb},
	{"nfhacek",	0x2c7},
	{"nfmac",	0xaf},
	{"nftilde",	0x2dc},
	{"nfuml",	0xa8},
	{"ng",		0x14b},
	{"not",		0xac},
	{"notelem",	0x2209},
	{"ntilde",	0xf1},
	{"nu",		0x3bd},
	{"oab",		0x2329},
	{"oacute",	0xf3},
	{"oasper",	MOAS},
	{"ob",		0x7b},
	{"obar",	0xf8},
	{"obigb",	0x7b},		/* should be big */
	{"obigpren",	0x28},
	{"obigsb",	0x5b},		/* should be big */
	{"obreve",	0x14f},
	{"ocirc",	0xf4},
	{"odsb",	0x301a},		/* [[ U+301A */
	{"oelig",		0x153},
	{"oeamp",	0x26},
	{"ograve",	0xf2},
	{"ohook",	0x6f},		/* +hook */
	{"olenis",	MOLN},
	{"omacr",	0x14d},
	{"omega",	0x3c9},
	{"omicron",	0x3bf},
	{"ope",		0x25b},
	{"opp",		0x260d},
	{"oq",		0x60},
	{"oqq",		0x201c},
	{"or",		MOR},
	{"osb",		0x5b},
	{"otilde",	0xf5},
	{"ouml",	0xf6},
	{"ounce",	0x2125},		/* ounce U+2125 */
	{"ovparen",	0x2322},		/* should be sideways ( */
	{"p",		0x2032},
	{"pa",		0x2202},
	{"page",	0x50},
	{"pall",	0x28e},
	{"paln",	0x272},
	{"par",		PAR},
	{"para",	0xb6},
	{"pbar",	0x70},		/* +bar */
	{"per",		0x2118},		/* per U+2118 */
	{"phi",		0x3c6},
	{"phi2",	0x3d5},
	{"pi",		0x3c0},
	{"pisces",	0x2653},
	{"planck",	0x127},
	{"plantinJ",	0x4a},		/* should be script */
	{"pm",		0xb1},
	{"pmil",	0x2030},
	{"pp",		0x2033},
	{"ppp",		0x2034},
	{"prop",	0x221d},
	{"psi",		0x3c8},
	{"pstlg",	0xa3},
	{"q",		0x3f},		/* should be raised */
	{"qamets",	0x5b3},		/* U+05B3 */
	{"quaver",	0x266a},
	{"rar",		0x2192},
	{"rasper",	MRAS},
	{"rdot",	0xb7},
	{"recipe",	0x211e},		/* U+211E */
	{"reg",		0xae},
	{"revC",	0x186},		/* open O U+0186 */
	{"reva",	0x252},
	{"revc",	0x254},
	{"revope",	0x25c},
	{"revr",	0x279},
	{"revsc",	0x2d2},		/* upside-down semicolon */
	{"revv",	0x28c},
	{"rfa",		0x6f},		/* +hook (Cf "goal") */
	{"rhacek",	0x159},
	{"rhalfbr",	0x2309},
	{"rho",		0x3c1},
	{"rhshoe",	0x2282},
	{"rlenis",	MRLN},
	{"rsylab",	0x72},		/* +sylab */
	{"runash",	0x46},		/* should be runic 'ash' */
	{"rvow",	0x2d4},
	{"sacute",	0x15b},
	{"sagit",	0x2650},
	{"sampi",	0x3e1},
	{"saturn",	0x2644},
	{"sced",	0x15f},
	{"schwa",	0x259},
	{"scorpio",	0x264f},
	{"scrA",	0x41},		/* should be script */
	{"scrC",	0x43},
	{"scrE",	0x45},
	{"scrF",	0x46},
	{"scrI",	0x49},
	{"scrJ",	0x4a},
	{"scrL",'L'},
	{"scrO",	0x4f},
	{"scrP",	0x50},
	{"scrQ",	0x51},
	{"scrS",	0x53},
	{"scrT",	0x54},
	{"scrb",	0x62},
	{"scrd",	0x64},
	{"scrh",	0x68},
	{"scrl",	0x6c},
	{"scruple",	0x2108},		/* U+2108 */
	{"sdd",		0x2d0},
	{"sect",	0xa7},
	{"semE",	0x2203},
	{"sh",		0x283},
	{"shacek",	0x161},
	{"sharp",	0x266f},
	{"sheva",	0x5b0},		/* U+05B0 */
	{"shti",	0x26a},
	{"shtsyll",	0x222a},
	{"shtu",	0x28a},
	{"sidetri",	0x22b2},
	{"sigma",	0x3c3},
	{"since",	0x2235},
	{"slge",	0x2265},		/* should have slanted line under */
	{"slle",	0x2264},		/* should have slanted line under */
	{"sm",		0x2c8},
	{"smm",		0x2cc},
	{"spade",	0x2660},
	{"sqrt",	0x221a},
	{"square",	0x25a1},		/* U+25A1 */
	{"ssChi",	0x3a7},		/* should be sans serif */
	{"ssIota",	0x399},
	{"ssOmicron",	0x39f},
	{"ssPi",	0x3a0},
	{"ssRho",	0x3a1},
	{"ssSigma",	0x3a3},
	{"ssTau",	0x3a4},
	{"star",	0x2a},
	{"stlig",	MLST},
	{"sup2",	0x2072},
	{"supgt",	0x2c3},
	{"suplt",	0x2c2},
	{"sur",		0x2b3},
	{"swing",	0x223c},
	{"tau",		0x3c4},
	{"taur",	0x2649},
	{"th",		0xfe},
	{"thbar",	0xfe},		/* +bar */
	{"theta",	0x3b8},
	{"thinqm",	0x3f},		/* should be thinner */
	{"tilde",	LTIL},
	{"times",	0xd7},
	{"tri",		0x2206},
	{"trli",	0x2016},
	{"ts",		0x2009},
	{"uacute",	0xfa},
	{"uasper",	MUAS},
	{"ubar",	0x75},		/* +bar */
	{"ubreve",	0x16d},
	{"ucirc",	0xfb},
	{"udA",		0x2200},
	{"udT",		0x22a5},
	{"uda",		0x250},
	{"udh",		0x265},
	{"udqm",	0xbf},
	{"udpsi",	0x22d4},
	{"udtr",	0x2207},
	{"ugrave",	0xf9},
	{"ulenis",	MULN},
	{"umacr",	0x16b},
	{"uml",		LUML},
	{"undl",	0x2cd},		/* underline accent */
	{"union",	0x222a},
	{"upsilon",	0x3c5},
	{"uuml",	0xfc},
	{"vavpath",	0x5d5},		/* vav U+05D5 (+patah) */
	{"vavsheva",	0x5d5},		/* vav U+05D5 (+sheva) */
	{"vb",		0x7c},
	{"vddd",	0x22ee},
	{"versicle2",	0x2123},		/* U+2123 */
	{"vinc",	0xaf},
	{"virgo",	0x264d},
	{"vpal",	0x25f},
	{"vvf",		0x263},
	{"wasper",	MWAS},
	{"wavyeq",	0x2248},
	{"wlenis",	MWLN},
	{"wyn",		0x1bf},		/* wynn U+01BF */
	{"xi",		0x3be},
	{"yacute",	0xfd},
	{"ycirc",	0x177},
	{"ygh",		0x292},
	{"ymacr",	0x79},		/* +macron */
	{"yuml",	0xff},
	{"zced",	0x7a},		/* +cedilla */
	{"zeta",	0x3b6},
	{"zh",		0x292},
	{"zhacek",	0x17e}
};
/*
   The following special characters don't have close enough
   equivalents in Unicode, so aren't in the above table.
	22n		2^(2^n) Cf Fermat
	2on4		2/4
	3on8		3/8
	Bantuo		Bantu O. Cf Otshi-herero
	Car		C with circular arrow on top
	albrtime 	cut-time: C with vertical line
	ardal		Cf dental
	bantuo		Bantu o. Cf Otshi-herero
	bbc1		single chem bond below
	bbc2		double chem bond below
	bbl1		chem bond like /
	bbl2		chem bond like //
	bbr1		chem bond like \
	bbr2		chem bond \\
	bcop1		copper symbol. Cf copper
	bcop2		copper symbol. Cf copper
	benchm		Cf benchmark
	btc1		single chem bond above
	btc2		double chem bond above
	btl1		chem bond like \
	btl2		chem bond like \\
	btr1		chem bond like /
	btr2		chem bond line //
	burman		Cf Burman
	devph		sanskrit letter. Cf ph
	devrfls		sanskrit letter. Cf cerebral
	duplong[12]	musical note
	egchi		early form of chi
	eggamma[12]	early form of gamma
	egiota		early form of iota
	egkappa		early form of kappa
	eglambda	early form of lambda
	egmu[12]	early form of mu
	egnu[12]	early form of nu
	egpi[123]	early form of pi
	egrho[12]	early form of rho
	egsampi		early form of sampi
	egsan		early form of san
	egsigma[12]	early form of sigma
	egxi[123]	early form of xi
	elatS		early form of S
	elatc[12]	early form of C
	elatg[12]	early form of G
	glagjeri	Slavonic Glagolitic jeri
	glagjeru	Slavonic Glagolitic jeru
	hypolem		hypolemisk (line with underdot)
	lhrbr		lower half }
	longmord	long mordent
	mbwvow		backwards scretched C. Cf retract.
	mord		music symbol.  Cf mordent
	mostra		Cf direct
	ohgcirc		old form of circumflex
	oldbeta		old form of β. Cf perturbate
	oldsemibr[12]	old forms of semibreve. Cf prolation
	ormg		old form of g. Cf G
	para[12345]	form of ¶
	pauseo		musical pause sign
	pauseu		musical pause sign
	pharyng		Cf pharyngal
	ragr		Black letter ragged r
	repetn		musical repeat. Cf retort
	segno		musical segno sign
	semain[12]	semitic ain
	semhe		semitic he
	semheth		semitic heth
	semkaph		semitic kaph
	semlamed[12]	semitic lamed
	semmem		semitic mem
	semnum		semitic nun
	sempe		semitic pe
	semqoph[123]	semitic qoph
	semresh		semitic resh
	semtav[1234]	semitic tav
	semyod		semitic yod
	semzayin[123]	semitic zayin
	shtlong[12]	U with underbar. Cf glyconic
	sigmatau	σ,τ combination
	squaver		sixteenth note
	sqbreve		square musical breve note
	swast		swastika
	uhrbr		upper half of big }
	versicle1		Cf versicle
 */


static Rune normtab[128] = {
	/*0*/	/*1*/	/*2*/	/*3*/	/*4*/	/*5*/	/*6*/	/*7*/
/*00*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	' ',	NONE,	NONE,	NONE,	NONE,	NONE,
/*10*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*20*/	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	SPCS,	'\'',
	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
/*30*/  0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,
	0x38,	0x39,	0x3a,	0x3b,	TAGS,	0x3d,	TAGE,	0x3f,
/*40*/  0x40,	0x41,	0x42,	0x43,	0x44,	0x45,	0x46,	0x47,
	0x48,	0x49,	0x4a,	0x4b,'L',	0x4d,	0x4e,	0x4f,
/*50*/	0x50,	0x51,	0x52,	0x53,	0x54,	0x55,	0x56,	0x57,
	0x58,	0x59,	0x5a,	0x5b,'\\',	0x5d,	0x5e,	0x5f,
/*60*/	0x60,	0x61,	0x62,	0x63,	0x64,	0x65,	0x66,	0x67,
	0x68,	0x69,	0x6a,	0x6b,	0x6c,	0x6d,	0x6e,	0x6f,
/*70*/	0x70,	0x71,	0x72,	0x73,	0x74,	0x75,	0x76,	0x77,
	0x78,	0x79,	0x7a,	0x7b,	0x7c,	0x7d,	0x7e,	NONE
};
#if 0
static Rune phtab[128] = {
	/*0*/	/*1*/	/*2*/	/*3*/	/*4*/	/*5*/	/*6*/	/*7*/
/*00*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*10*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*20*/	0x20,	0x21,	0x2c8,	0x23,	0x24,	0x2cc,	0xe6,	'\'',
	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
/*30*/  0x30,	0x31,	0x32,	0x25c,	0x34,	0x35,	0x36,	0x37,
	0x38,	0xf8,	0x2d0,	0x3b,	TAGS,	0x3d,	TAGE,	0x3f,
/*40*/  0x259,	0x251,	0x42,	0x43,	0xf0,	0x25b,	0x46,	0x47,
	0x48,	0x26a,	0x4a,	0x4b,'L',	0x4d,	0x14b,	0x254,
/*50*/	0x50,	0x252,	0x52,	0x283,	0x3b8,	0x28a,	0x28c,	0x57,
	0x58,	0x59,	0x292,	0x5b,'\\',	0x5d,	0x5e,	0x5f,
/*60*/	0x60,	0x61,	0x62,	0x63,	0x64,	0x65,	0x66,	0x67,
	0x68,	0x69,	0x6a,	0x6b,	0x6c,	0x6d,	0x6e,	0x6f,
/*70*/	0x70,	0x71,	0x72,	0x73,	0x74,	0x75,	0x76,	0x77,
	0x78,	0x79,	0x7a,	0x7b,	0x7c,	0x7d,	0x7e,	NONE
};
static Rune grtab[128] = {
	/*0*/	/*1*/	/*2*/	/*3*/	/*4*/	/*5*/	/*6*/	/*7*/
/*00*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*10*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*20*/	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	SPCS,	'\'',
	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
/*30*/  0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,
	0x38,	0x39,	0x3a,	0x3b,	TAGS,	0x3d,	TAGE,	0x3f,
/*40*/  0x40,	0x391,	0x392,	0x39e,	0x394,	0x395,	0x3a6,	0x393,
	0x397,	0x399,	0x3da,	0x39a,	0x39b,	0x39c,	0x39d,	0x39f,
/*50*/	0x3a0,	0x398,	0x3a1,	0x3a3,	0x3a4,	0x3a5,	0x56,	0x3a9,
	0x3a7,	0x3a8,	0x396,	0x5b,'\\',	0x5d,	0x5e,	0x5f,
/*60*/	0x60,	0x3b1,	0x3b2,	0x3be,	0x3b4,	0x3b5,	0x3c6,	0x3b3,
	0x3b7,	0x3b9,	0x3c2,	0x3ba,	0x3bb,	0x3bc,	0x3bd,	0x3bf,
/*70*/	0x3c0,	0x3b8,	0x3c1,	0x3c3,	0x3c4,	0x3c5,	0x76,	0x3c9,
	0x3c7,	0x3c8,	0x3b6,	0x7b,	0x7c,	0x7d,	0x7e,	NONE
};
static Rune subtab[128] = {
	/*0*/	/*1*/	/*2*/	/*3*/	/*4*/	/*5*/	/*6*/	/*7*/
/*00*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*10*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*20*/	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	SPCS,	'\'',
	0x208d,	0x208e,	0x2a,	0x208a,	0x2c,	0x208b,	0x2e,	0x2f,
/*30*/  0x2080,	0x2081,	0x2082,	0x2083,	0x2084,	0x2085,	0x2086,	0x2087,
	0x2088,	0x2089,	0x3a,	0x3b,	TAGS,	0x208c,	TAGE,	0x3f,
/*40*/  0x40,	0x41,	0x42,	0x43,	0x44,	0x45,	0x46,	0x47,
	0x48,	0x49,	0x4a,	0x4b,'L',	0x4d,	0x4e,	0x4f,
/*50*/	0x50,	0x51,	0x52,	0x53,	0x54,	0x55,	0x56,	0x57,
	0x58,	0x59,	0x5a,	0x5b,'\\',	0x5d,	0x5e,	0x5f,
/*60*/	0x60,	0x61,	0x62,	0x63,	0x64,	0x65,	0x66,	0x67,
	0x68,	0x69,	0x6a,	0x6b,	0x6c,	0x6d,	0x6e,	0x6f,
/*70*/	0x70,	0x71,	0x72,	0x73,	0x74,	0x75,	0x76,	0x77,
	0x78,	0x79,	0x7a,	0x7b,	0x7c,	0x7d,	0x7e,	NONE
};
static Rune suptab[128] = {
	/*0*/	/*1*/	/*2*/	/*3*/	/*4*/	/*5*/	/*6*/	/*7*/
/*00*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*10*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*20*/	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	SPCS,	'\'',
	0x207d,	0x207e,	0x2a,	0x207a,	0x2c,	0x207b,	0x2e,	0x2f,
/*30*/  0x2070,	0x2071,	0x2072,	0x2073,	0x2074,	0x2075,	0x2076,	0x2077,
	0x2078,	0x2079,	0x3a,	0x3b,	TAGS,	0x207c,	TAGE,	0x3f,
/*40*/  0x40,	0x41,	0x42,	0x43,	0x44,	0x45,	0x46,	0x47,
	0x48,	0x49,	0x4a,	0x4b,'L',	0x4d,	0x4e,	0x4f,
/*50*/	0x50,	0x51,	0x52,	0x53,	0x54,	0x55,	0x56,	0x57,
	0x58,	0x59,	0x5a,	0x5b,'\\',	0x5d,	0x5e,	0x5f,
/*60*/	0x60,	0x61,	0x62,	0x63,	0x64,	0x65,	0x66,	0x67,
	0x68,	0x69,	0x6a,	0x6b,	0x6c,	0x6d,	0x6e,	0x6f,
/*70*/	0x70,	0x71,	0x72,	0x73,	0x74,	0x75,	0x76,	0x77,
	0x78,	0x79,	0x7a,	0x7b,	0x7c,	0x7d,	0x7e,	NONE
};
#endif

static int	tagstarts;
static char	tag[Buflen];
static char	spec[Buflen];
static Entry	curentry;
#define cursize (curentry.end-curentry.start)

static char	*getspec(char *, char *);
static char	*gettag(char *, char *);

/*
 * cmd is one of:
 *    'p': normal print
 *    'h': just print headwords
 *    'P': print raw
 */
void
pgwprintentry(Entry e, int cmd)
{
	char *p, *pe;
	int t;
	long r, rprev, rlig;
	Rune *transtab;

	p = e.start;
	pe = e.end;
	transtab = normtab;
	rprev = NONE;
	changett(0, 0, 0);
	curentry = e;
	if(cmd == 'h')
		outinhibit = 1;
	while(p < pe) {
		if(cmd == 'r') {
			outchar(*p++);
			continue;
		}
		r = transtab[(*p++)&0x7F];
		if(r < NONE) {
			/* Emit the rune, but buffer in case of ligature */
			if(rprev != NONE)
				outrune(rprev);
			rprev = r;
		} else if(r == SPCS) {
			/* Start of special character name */
			p = getspec(p, pe);
			r = lookassoc(spectab, asize(spectab), spec);
			if(r == -1) {
				if(debug)
					err("spec %ld %d %s",
						e.doff, cursize, spec);
				r = 0xfffd;
			}
			if(r >= LIGS && r < LIGE) {
				/* handle possible ligature */
				rlig = liglookup(r, rprev);
				if(rlig != NONE)
					rprev = rlig;	/* overwrite rprev */
				else {
					/* could print accent, but let's not */
					if(rprev != NONE) outrune(rprev);
					rprev = NONE;
				}
			} else if(r >= MULTI && r < MULTIE) {
				if(rprev != NONE) {
					outrune(rprev);
					rprev = NONE;
				}
				outrunes(multitab[r-MULTI]);
			} else if(r == PAR) {
				if(rprev != NONE) {
					outrune(rprev);
					rprev = NONE;
				}
				outnl(1);
			} else {
				if(rprev != NONE) outrune(rprev);
				rprev = r;
			}
		} else if(r == TAGS) {
			/* Start of tag name */
			if(rprev != NONE) {
				outrune(rprev);
				rprev = NONE;
			}
			p = gettag(p, pe);
			t = lookassoc(tagtab, asize(tagtab), tag);
			if(t == -1) {
				if(debug)
					err("tag %ld %d %s",
						e.doff, cursize, tag);
				continue;
			}
			switch(t){
			case Hw:
				if(cmd == 'h') {
					if(!tagstarts)
						outchar(' ');
					outinhibit = !tagstarts;
				}
				break;
			case Sn:
				if(tagstarts) {
					outnl(2);
				}
				break;
			case P:
				outnl(tagstarts);
				break;
			case Col:
			case Br:
			case Blockquote:
				if(tagstarts)
					outnl(1);
				break;
			case U:
				outchar('/');
			}
		}
	}
	if(cmd == 'h') {
		outinhibit = 0;
		outnl(0);
	}
}

/*
 * Return offset into bdict where next webster entry after fromoff starts.
 * Webster entries start with <p><hw>
 */
long
pgwnextoff(long fromoff)
{
	long a, n;
	int c;

	a = Bseek(bdict, fromoff, 0);
	if(a != fromoff)
		return -1;
	n = 0;
	for(;;) {
		c = Bgetc(bdict);
		if(c < 0)
			break;
		if(c == '<' && Bgetc(bdict) == 'p' && Bgetc(bdict) == '>') {
			c = Bgetc(bdict);
			if(c == '<') {
				if (Bgetc(bdict) == 'h' && Bgetc(bdict) == 'w'
					&& Bgetc(bdict) == '>')
						n = 7;
			}else if (c == '{')
				n = 4;
			if(n)
				break;
		}
	}
	return (Boffset(bdict)-n);
}

static char *prkey1 =
"KEY TO THE PRONUNCIATION\n"
"\n"
"I. CONSONANTS\n"
"b, d, f, k, l, m, n, p, t, v, z: usual English values\n"
"\n"
"g as in go (gəʊ)\n"
"h  ...  ho! (həʊ)\n"
"r  ...  run (rʌn), terrier (ˈtɛriə(r))\n"
"(r)...  her (hɜː(r))\n"
"s  ...  see (siː), success (səkˈsɜs)\n"
"w  ...  wear (wɛə(r))\n"
"hw ...  when (hwɛn)\n"
"j  ...  yes (jɛs)\n"
"θ  ...  thin (θin), bath (bɑːθ)\n"
"ð  ...  then (ðɛn), bathe (beɪð)\n"
"ʃ  ...  shop (ʃɒp), dish (dɪʃ)\n"
"tʃ ...  chop (tʃɒp), ditch (dɪtʃ)\n"
"ʒ  ...  vision (ˈvɪʒən), déjeuner (deʒøne)\n"
;
static char *prkey2 =
"dʒ ...  judge (dʒʌdʒ)\n"
"ŋ  ...  singing (ˈsɪŋɪŋ), think (θiŋk)\n"
"ŋg ...  finger (ˈfiŋgə(r))\n"
"\n"
"Foreign\n"
"ʎ as in It. seraglio (serˈraʎo)\n"
"ɲ  ...  Fr. cognac (kɔɲak)\n"
"x  ...  Ger. ach (ax), Sc. loch (lɒx)\n"
"ç  ...  Ger. ich (ɪç), Sc. nicht (nɪçt)\n"
"ɣ  ...  North Ger. sagen (ˈzaːɣən)\n"
"c  ...  Afrikaans baardmannetjie (ˈbaːrtmanəci)\n"
"ɥ  ...  Fr. cuisine (kɥizin)\n"
"\n"
;
static char *prkey3 =
"II. VOWELS AND DIPTHONGS\n"
"\n"
"Short\n"
"ɪ as in pit (pɪt), -ness (-nɪs)\n"
"ɛ  ...  pet (pɛt), Fr. sept (sɛt)\n"
"æ  ...  pat (pæt)\n"
"ʌ  ...  putt (pʌt)\n"
"ɒ  ...  pot (pɒt)\n"
"ʊ  ...  put (pʊt)\n"
"ə  ...  another (əˈnʌðə(r))\n"
"(ə)...  beaten (ˈbiːt(ə)n)\n"
"i  ...  Fr. si (si)\n"
"e  ...  Fr. bébé (bebe)\n"
"a  ...  Fr. mari (mari)\n"
"ɑ  ...  Fr. bâtiment (bɑtimã)\n"
"ɔ  ...  Fr. homme (ɔm)\n"
"o  ...  Fr. eau (o)\n"
"ø  ...  Fr. peu (pø)\n"
;
static char *prkey4 =
"œ  ...  Fr. boeuf (bœf), coeur (kœr)\n"
"u  ...  Fr. douce (dus)\n"
"ʏ  ...  Ger. Müller (ˈmʏlər)\n"
"y  ...  Fr. du (dy)\n"
"\n"
"Long\n"
"iː as in bean (biːn)\n"
"ɑː ...  barn (bɑːn)\n"
"ɔː ...  born (bɔːn)\n"
"uː ...  boon (buːn)\n"
"ɜː ...  burn (bɜːn)\n"
"eː ...  Ger. Schnee (ʃneː)\n"
"ɛː ...  Ger. Fähre (ˈfɛːrə)\n"
"aː ...  Ger. Tag (taːk)\n"
"oː ...  Ger. Sohn (zoːn)\n"
"øː ...  Ger. Goethe (gøːtə)\n"
"yː ...  Ger. grün (gryːn)\n"
"\n"
;
static char *prkey5 =
"Nasal\n"
"ɛ˜, æ˜ as in Fr. fin (fɛ˜, fæ˜)\n"
"ã  ...  Fr. franc (frã)\n"
"ɔ˜ ...  Fr. bon (bɔ˜n)\n"
"œ˜ ...  Fr. un (œ˜)\n"
"\n"
"Dipthongs, etc.\n"
"eɪ as in bay (beɪ)\n"
"aɪ ...  buy (baɪ)\n"
"ɔɪ ...  boy (bɔɪ)\n"
"əʊ ...  no (nəʊ)\n"
"aʊ ...  now (naʊ)\n"
"ɪə ...  peer (pɪə(r))\n"
"ɛə ...  pair (pɛə(r))\n"
"ʊə ...  tour (tʊə(r))\n"
"ɔə ...  boar (bɔə(r))\n"
"\n"
;
static char *prkey6 =
"III. STRESS\n"
"\n"
"Main stress: ˈ preceding stressed syllable\n"
"Secondary stress: ˌ preceding stressed syllable\n"
"\n"
"E.g.: pronunciation (prəˌnʌnsɪˈeɪʃ(ə)n)\n";
/* TODO: find transcriptions of foreign consonents, œ, ʏ, nasals */

void
pgwprintkey(void)
{
	Bprint(bout, "%s%s%s%s%s%s",
		prkey1, prkey2, prkey3, prkey4, prkey5, prkey6);
}

/*
 * f points just after a '&', fe points at end of entry.
 * Accumulate the special name, starting after the &
 * and continuing until the next ';', in spec[].
 * Return pointer to char after ';'.
 */
static char *
getspec(char *f, char *fe)
{
	char *t;
	int c, i;

	t = spec;
	i = sizeof spec;
	while(--i > 0) {
		c = *f++;
		if(c == ';' || f == fe)
			break;
		*t++ = c;
	}
	*t = 0;
	return f;
}

/*
 * f points just after '<'; fe points at end of entry.
 * Expect next characters from bin to match:
 *  [/][^ >]+( [^>=]+=[^ >]+)*>
 *      tag   auxname auxval
 * Accumulate the tag and its auxilliary information in
 * tag[], auxname[][] and auxval[][].
 * Set tagstarts=1 if the tag is 'starting' (has no '/'), else 0.
 * Set naux to the number of aux pairs found.
 * Return pointer to after final '>'.
 */
static char *
gettag(char *f, char *fe)
{
	char *t;
	int c, i;

	t = tag;
	c = *f++;
	if(c == '/')
		tagstarts = 0;
	else {
		tagstarts = 1;
		*t++ = c;
	}
	i = Buflen;
	while(--i > 0) {
		c = *f++;
		if(c == '>' || f == fe)
			break;
		*t++ = c;
	}
	*t = 0;
	return f;
}
