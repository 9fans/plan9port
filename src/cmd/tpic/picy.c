
#line	2	"/usr/local/plan9/src/cmd/tpic/picy.y"
#include <stdio.h>
#include "pic.h"
#include <math.h>
YYSTYPE	y;
int yylex(void);
extern	int	yyerrflag;
#ifndef	YYMAXDEPTH
#define	YYMAXDEPTH	150
#endif
YYSTYPE	yylval;
YYSTYPE	yyval;
#define	BOX	1
#define	LINE	2
#define	ARROW	3
#define	CIRCLE	4
#define	ELLIPSE	5
#define	ARC	6
#define	SPLINE	7
#define	BLOCK	8
#define	TEXT	9
#define	TROFF	10
#define	MOVE	11
#define	BLOCKEND	12
#define	PLACE	13
#define	PRINT	57359
#define	RESET	57360
#define	THRU	57361
#define	UNTIL	57362
#define	FOR	57363
#define	IF	57364
#define	COPY	57365
#define	THENSTR	57366
#define	ELSESTR	57367
#define	DOSTR	57368
#define	PLACENAME	57369
#define	VARNAME	57370
#define	SPRINTF	57371
#define	DEFNAME	57372
#define	ATTR	57373
#define	TEXTATTR	57374
#define	LEFT	57375
#define	RIGHT	57376
#define	UP	57377
#define	DOWN	57378
#define	FROM	57379
#define	TO	57380
#define	AT	57381
#define	BY	57382
#define	WITH	57383
#define	HEAD	57384
#define	CW	57385
#define	CCW	57386
#define	THEN	57387
#define	HEIGHT	57388
#define	WIDTH	57389
#define	RADIUS	57390
#define	DIAMETER	57391
#define	LENGTH	57392
#define	SIZE	57393
#define	CORNER	57394
#define	HERE	57395
#define	LAST	57396
#define	NTH	57397
#define	SAME	57398
#define	BETWEEN	57399
#define	AND	57400
#define	EAST	57401
#define	WEST	57402
#define	NORTH	57403
#define	SOUTH	57404
#define	NE	57405
#define	NW	57406
#define	SE	57407
#define	SW	57408
#define	START	57409
#define	END	57410
#define	DOTX	57411
#define	DOTY	57412
#define	DOTHT	57413
#define	DOTWID	57414
#define	DOTRAD	57415
#define	NUMBER	57416
#define	LOG	57417
#define	EXP	57418
#define	SIN	57419
#define	COS	57420
#define	ATAN2	57421
#define	SQRT	57422
#define	RAND	57423
#define	MIN	57424
#define	MAX	57425
#define	INT	57426
#define	DIR	57427
#define	DOT	57428
#define	DASH	57429
#define	CHOP	57430
#define	FILL	57431
#define	ST	57432
#define	OROR	57433
#define	ANDAND	57434
#define	GT	57435
#define	LT	57436
#define	LE	57437
#define	GE	57438
#define	EQ	57439
#define	NEQ	57440
#define	UMINUS	57441
#define	NOT	57442
#define YYEOFCODE 1
#define YYERRCODE 2
static	const	short	yyexca[] =
{-1, 0,
	1, 2,
	-2, 0,
-1, 1,
	1, -1,
	-2, 0,
-1, 203,
	94, 0,
	95, 0,
	96, 0,
	97, 0,
	98, 0,
	99, 0,
	-2, 156,
-1, 210,
	94, 0,
	95, 0,
	96, 0,
	97, 0,
	98, 0,
	99, 0,
	-2, 155,
-1, 211,
	94, 0,
	95, 0,
	96, 0,
	97, 0,
	98, 0,
	99, 0,
	-2, 157,
-1, 212,
	94, 0,
	95, 0,
	96, 0,
	97, 0,
	98, 0,
	99, 0,
	-2, 158,
-1, 213,
	94, 0,
	95, 0,
	96, 0,
	97, 0,
	98, 0,
	99, 0,
	-2, 159,
-1, 214,
	94, 0,
	95, 0,
	96, 0,
	97, 0,
	98, 0,
	99, 0,
	-2, 160,
-1, 266,
	94, 0,
	95, 0,
	96, 0,
	97, 0,
	98, 0,
	99, 0,
	-2, 156,
};
#define	YYNPROD	175
#define	YYPRIVATE 57344
#define	YYLAST	1551
static	const	short	yyact[] =
{
 171, 330, 137,  52, 316,  67, 270, 123, 124, 308,
 315,  42, 269, 239, 108,  32, 135, 160, 135, 159,
 158, 157,  94, 224, 130, 131, 132, 133, 134,  43,
 156, 155,  91,  50, 154, 153, 152, 151, 135,  97,
  80, 104, 295, 294, 243, 232, 230,  40, 121, 126,
 129,  82, 123, 124, 312, 150, 147, 109, 110, 111,
 112, 113, 271,  50, 121, 225,  71, 106,  41, 162,
 101, 164, 128,  40, 331, 332, 333, 334, 136, 127,
 243, 167, 191, 187,  72,  73,  74,  75,  76,  77,
  78,  79, 272, 200, 197, 109, 110, 111, 112, 113,
 136, 125, 121, 123, 124, 123, 124, 201, 203, 104,
 205, 206, 207, 208, 209, 210, 211, 212, 213, 214,
 215, 216, 217,  38, 218, 221, 231, 111, 112, 113,
  50,  50, 121, 317, 123, 124, 192, 202, 204, 123,
 124, 195, 196, 166,  84, 229, 220, 223, 165,  95,
  96,  35, 233, 234, 235, 236, 237, 238,  34, 240,
 241, 242, 189, 168, 283, 244, 246, 281,  36,  44,
 122, 249, 248, 250, 104, 104, 104, 104, 104,  89,
 123, 124, 258, 259, 260, 261,   4,  70,  85,  37,
  92, 296, 263, 264, 227, 266,  50,  50,  50,  50,
  50,  80, 265, 251, 252, 253, 254, 257, 119, 114,
 194, 115, 116, 117, 118, 109, 110, 111, 112, 113,
 274, 169, 121, 276, 283, 284,  37,  99, 188, 279,
 114, 194, 115, 116, 117, 118, 109, 110, 111, 112,
 113, 262,  85, 121, 281, 282, 190,  35, 277, 130,
 131, 132, 133, 134,  86,  87, 198, 227, 228, 162,
 193, 164,   2,  83,  36,  69,   1,   5,  37,  39,
 161, 301, 104, 104, 304,  26, 306,   6, 185,  24,
  12,  24,  13, 147,  14,  24, 300, 199,  88,  81,
 309,  90, 310, 311,  50,  50, 278,  68, 163, 313,
 314, 302, 303,   0,   0,  24, 318,   0, 319, 140,
 144, 145, 141, 142, 143, 146, 247, 327,  24,  24,
   0,  65,  66,  68, 280,   0,   0, 335,   0, 297,
   0, 336,   0,   0,   0,   0, 337,   0,   0,  16,
  20,  21,  17,  18,  19,  22,   0,  35,  25,  23,
  51,  46,  10,  11, 267, 268,  30,  31,  29, 149,
  24,   0, 102,  46,  36,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,  65,  66,  68,  53,  24,
   0,   0,   0,   0,   0,   0,   0,  65,  66,  68,
  53,   0,   0,   0,   0,   0,   0,  45,  55,  56,
  57,  58,  59,  60,  61,  63,  62,  64,   0,  45,
  55,  56,  57,  58,  59,  60,  61,  63,  62,  64,
   9,   0,   0,   0,  48, 100,   0,   0, 299,  54,
   0,   0,   0,   0,   0,   0,  48,  35,  93,   0,
   0,  54,   0,   0,   0,   0,  27,   0,  33,   0,
  49,   0,  51,  46,  36,   0, 170, 179,   0,   0,
   0,   0, 173, 174, 175, 176, 177, 180, 140, 144,
 145, 141, 142, 143, 146, 245,   0,  65,  66,  68,
  53, 178, 120, 119, 114, 194, 115, 116, 117, 118,
 109, 110, 111, 112, 113,   0,   0, 121,   0,  45,
  55,  56,  57,  58,  59,  60,  61,  63,  62,  64,
 172, 181, 182, 183, 184,   0,   0,  35, 139,   0,
   0,   0,  47,   8,   0,   8,  48,   0,  35,   8,
   0,  54,  51,  46,  36,   0,   0,   0,   0,   0,
  93,   0,   0,  51,  46,  36,   0,   0,   0,   8,
   0,   0,   0,   0,   0,   0,   0,  65,  66,  68,
  53,   0,   8, 103,   0,   0, 339,   0,  65,  66,
  68,  53,   0,   0,   0,   0,   0,   0,   0,  45,
  55,  56,  57,  58,  59,  60,  61,  63,  62,  64,
  45,  55,  56,  57,  58,  59,  60,  61,  63,  62,
  64,  51,  46,   0,   8,   0,  48,   0,   0,   0,
   0,  54,   0,   0,   0,   0,   0,  48,   0,   0,
  93,   0,  54,   8,   0,   0, 255,  66,  68,  53,
   0,  49, 120, 119, 114, 194, 115, 116, 117, 118,
 109, 110, 111, 112, 113,   0,   0, 121,  45,  55,
  56,  57,  58,  59,  60,  61,  63,  62,  64,  16,
  20,  21,  17,  18,  19,  22,   0,  35,  25,  23,
   0,   0,  10,  11,   0,  48,  30,  31,  29,   0,
  54,   0,   7,  28,  36,   0,   0,   0, 256,  49,
  16,  20,  21,  17,  18,  19,  22,   0,  35,  25,
  23,   0,   0,  10,  11,   0,   0,  30,  31,  29,
   0,   0,   0,   7,  28,  36,   0,   3,   0,  16,
  20,  21,  17,  18,  19,  22,   0,  35,  25,  23,
  51,  46,  10,  11,   0,   0,  30,  31,  29,   0,
   9,   0,   7,  28,  36,  15, 140, 144, 145, 141,
 142, 143, 146, 148,   0,  65,  66,  68,  53,   0,
   0,   0,   0,   0,   0,   0,  27, 186,  33,   0,
   0,   9,   0,   0,   0,   0,  15,  45,  55,  56,
  57,  58,  59,  60,  61,  63,  62,  64,  51,  46,
   0,   0,   0,   0,  98,   0, 149,  27,   0,  33,
   9,   0,   0,   0,  48,  15,   0,   0,   0,  54,
   0,   0,   0,  65,  66,  68,  53,   0,  49,   0,
   0,   0,   0,   0,   0,   0,  27,   0,  33,   0,
  51,  46,   0,   0,   0,  45,  55,  56,  57,  58,
  59,  60,  61,  63,  62,  64,   0,   0,   0,   0,
   0,   0,   0,   0,   0,  65,  66,  68,  53,   0,
   0,   0,  48,   0,   0,   0,   0,  54,   0,   0,
   0,   0,   0,   0,   0,   0, 222,  45,  55,  56,
  57,  58,  59,  60,  61,  63,  62,  64,  16,  20,
  21,  17,  18,  19,  22, 108,  35,  25,  23,   0,
   0,  10,  11,   0,  48,  30,  31,  29,   0,  54,
   0,   7,  28,  36,   0,   0,   0,   0, 219,   0,
   0, 140, 144, 145, 141, 142, 143, 146, 138,   0,
 120, 119, 114, 107, 115, 116, 117, 118, 109, 110,
 111, 112, 113,   0,   0, 121,   0,   0, 106,   0,
   0,   0,   0,   0, 226, 120, 119, 114, 194, 115,
 116, 117, 118, 109, 110, 111, 112, 113,   0,   9,
 121, 139,   0, 307,  15,   0,   0,   0,   0, 226,
   0, 120, 119, 114, 194, 115, 116, 117, 118, 109,
 110, 111, 112, 113,   0,  27, 121,  33,   0, 305,
   0,   0,   0,   0,   0, 226, 120, 119, 114, 194,
 115, 116, 117, 118, 109, 110, 111, 112, 113,   0,
   0, 121,   0,   0,   0,   0,   0,   0,   0,   0,
 329, 120, 119, 114, 194, 115, 116, 117, 118, 109,
 110, 111, 112, 113,   0,   0, 121,   0,   0,   0,
   0,   0,   0,   0,   0, 328, 120, 119, 114, 194,
 115, 116, 117, 118, 109, 110, 111, 112, 113,   0,
   0, 121,   0,   0,   0,   0,   0,   0,   0,   0,
 322, 120, 119, 114, 194, 115, 116, 117, 118, 109,
 110, 111, 112, 113,   0,   0, 121,   0,   0,   0,
   0,   0,   0,   0,   0, 321, 120, 119, 114, 194,
 115, 116, 117, 118, 109, 110, 111, 112, 113,   0,
   0, 121,   0,   0,   0,   0,   0,   0,   0,   0,
 320, 120, 119, 114, 194, 115, 116, 117, 118, 109,
 110, 111, 112, 113,   0,   0, 121,   0,   0,   0,
   0,   0,   0,   0,   0, 293, 120, 119, 114, 194,
 115, 116, 117, 118, 109, 110, 111, 112, 113,   0,
   0, 121,   0,   0,   0,   0,   0,   0,   0,   0,
 290, 120, 119, 114, 194, 115, 116, 117, 118, 109,
 110, 111, 112, 113,   0,   0, 121,   0,   0,   0,
   0,   0,   0,   0,   0, 288, 120, 119, 114, 194,
 115, 116, 117, 118, 109, 110, 111, 112, 113,   0,
   0, 121,   0,   0,   0,   0,   0,   0,   0,   0,
 287, 120, 119, 114, 194, 115, 116, 117, 118, 109,
 110, 111, 112, 113,   0,   0, 121,   0,   0,   0,
   0,   0,   0,   0,   0, 286, 120, 119, 114, 194,
 115, 116, 117, 118, 109, 110, 111, 112, 113,   0,
   0, 121,   0, 108,   0,   0,   0,   0,   0,   0,
 285, 120, 119, 114, 194, 115, 116, 117, 118, 109,
 110, 111, 112, 113, 108,   0, 121,   0,   0,   0,
   0,   0,   0,   0,   0, 226, 105,   0, 120, 119,
 114, 107, 115, 116, 117, 118, 109, 110, 111, 112,
 113,   0,   0, 121,   0,   0, 106,   0,   0, 120,
 119, 114, 107, 115, 116, 117, 118, 109, 110, 111,
 112, 113,   0,   0, 121,   0,   0, 106, 120, 119,
 114, 194, 115, 116, 117, 118, 109, 110, 111, 112,
 113,   0,   0, 121,   0,   0, 292, 120, 119, 114,
 194, 115, 116, 117, 118, 109, 110, 111, 112, 113,
   0,   0, 121,   0,   0, 291, 120, 119, 114, 194,
 115, 116, 117, 118, 109, 110, 111, 112, 113,   0,
   0, 121, 338,   0, 289, 120, 119, 114, 194, 115,
 116, 117, 118, 109, 110, 111, 112, 113,   0,   0,
 121,   0,   0, 275, 120, 119, 114, 194, 115, 116,
 117, 118, 109, 110, 111, 112, 113, 326,   0, 121,
   0,   0, 273,   0,   0,   0,   0,   0,   0,   0,
   0, 325,   0, 324,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0, 323, 120, 119,
 114, 194, 115, 116, 117, 118, 109, 110, 111, 112,
 113, 298,   0, 121, 120, 119, 114, 194, 115, 116,
 117, 118, 109, 110, 111, 112, 113,   0,   0, 121,
   0,   0,   0, 120, 119, 114, 194, 115, 116, 117,
 118, 109, 110, 111, 112, 113,   0,   0, 121, 120,
 119, 114, 194, 115, 116, 117, 118, 109, 110, 111,
 112, 113,   0,   0, 121, 120, 119, 114, 194, 115,
 116, 117, 118, 109, 110, 111, 112, 113,   0,   0,
 121
};
static	const	short	yypact[] =
{
 715,-1000, 884,-1000,-1000,  33, 884, -62, -22,-1000,
 516, 159,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000, 139,-1000, 884,-1000, -40, 235,
 151, 505, 117,-1000, 118,-1000, -76,-1000,-1000, 686,
 335,-1000,1216,  80,  11,-1000, -40,-1000, 323, 703,
 180, -14, 917, 742, 323, -78, -79, -80, -81, -84,
 -85, -94, -95, -96, -98, 243,-1000,  96,-1000,  53,
-1000, 425, 425, 425, 425, 425, 425, 425, 425, 425,
 117, 655, 323, 235,-1000,-1000, 132, 139,  45,-1000,
 236,1392,  43, 323, 180,-1000,-1000, 139,-1000,-1000,
 884,   3, -36, -22,1237,-1000, 323, 703, 703, 323,
 323, 323, 323, 323, 323, 323, 323, 323, 323, 323,
 323, 323,-1000, 803, 761,-1000, -59, -93, -45, 838,
-1000,-1000,-1000,-1000,-1000,-1000, 230,  93, -68,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,  74, -69,-1000,
 -59, 323, 323, 323, 323, 323, 323,-103, 323, 323,
 323, -70, 464, 305,-1000,-1000,-1000,-1000, 144,-1000,
 323,1392, 323, 703, 703, 703, 703, 574,-1000,-1000,
-1000, 323, 323, 323, 323, 139,-1000,1392,-1000,-1000,
-1000, 323, 323, 177, 323, 139, 139,1189,-104,-1000,
-1000,1392, -48, -43,  34,  25,  25, -59, -59, -59,
  -5,  -5,  -5,  -5,  -5, 136, 115, -59,1332, 323,
 180,1313, 323, 180,-1000, 269,-1000,-1000,-1000,-1000,
 217,-1000, 197,1164,1139,1114,1089,1294,1064,-1000,
1275,1256,1039, 167,-1000, -71,-1000, -72,-1000,1392,
1392,   5,   5,   5,   5, 243, 164,   5,1392,1392,
1392,1392,-1000,1443, 390,-1000,  -5,-1000,-1000,-1000,
 323, 703, 703, 323, 889, 323, 863,-107, -34, 464,
 305,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000, 323,
-1000, 323, 323,-1000, 140, 137,   2, 425, 323, 323,
-106,1392,  39,   5,1392, 323,1392, 323,-1000,1014,
 989, 964,-1000,1427,1411,-1000, 323,-1000, 939, 914,
-1000,-1000,-1000, -26,-1000, -26,-1000,1392,-1000,-1000,
 323,-1000,-1000,-1000,-1000, 323,1376, 540,-1000,-1000
};
static	const	short	yypgo[] =
{
   0,   0, 291, 522, 288, 158,   1, 286, 284, 282,
 280, 277, 186, 262,  29, 275, 267,  22,   5, 278,
  15,   3,   2, 266, 265, 263, 144,  66, 241, 221
};
static	const	short	yyr1[] =
{
   0,  23,  23,  23,  13,  13,  12,  12,  12,  12,
  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,
  12,  24,  24,  24,  24,   3,  10,  25,  25,  26,
  26,  26,   9,   9,   9,   9,   8,   8,   2,   2,
   2,   4,   6,   6,   6,   6,   6,  11,  16,  16,
  16,  16,  16,  16,  16,  16,  16,  16,  28,  16,
  15,  27,  27,  29,  29,  29,  29,  29,  29,  29,
  29,  29,  29,  29,  29,  29,  29,  29,  29,  29,
  29,  29,  29,  29,  29,  29,  29,  29,  19,  19,
  20,  20,  20,   5,   5,   5,   7,   7,  14,  14,
  14,  14,  14,  14,  14,  14,  14,  14,  14,  14,
  17,  17,  17,  17,  17,  17,  17,  17,  17,  17,
  17,  17,  17,  18,  18,  18,  21,  21,  21,  22,
  22,  22,  22,  22,  22,  22,  22,   1,   1,   1,
   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
   1,   1,   1,   1,   1
};
static	const	short	yyr2[] =
{
   0,   1,   0,   1,   1,   2,   2,   3,   3,   4,
   4,   2,   1,   3,   3,   3,   3,   1,   1,   1,
   1,   0,   1,   2,   3,   3,   2,   1,   2,   1,
   2,   2,  10,   7,  10,   7,   4,   3,   1,   3,
   3,   1,   1,   1,   1,   1,   0,   1,   2,   2,
   2,   2,   2,   2,   2,   2,   2,   1,   0,   5,
   1,   2,   0,   2,   1,   1,   2,   1,   2,   2,
   2,   2,   2,   3,   4,   2,   1,   1,   1,   2,
   1,   2,   1,   2,   1,   2,   1,   1,   1,   2,
   1,   2,   2,   1,   4,   6,   1,   3,   1,   3,
   3,   5,   5,   7,   7,   3,   3,   5,   6,   5,
   1,   2,   2,   1,   2,   3,   3,   2,   3,   3,
   1,   2,   2,   4,   4,   3,   2,   2,   1,   1,
   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
   3,   3,   3,   3,   3,   2,   3,   2,   2,   2,
   2,   2,   3,   4,   4,   3,   3,   3,   3,   3,
   3,   3,   3,   2,   4,   4,   3,   4,   4,   6,
   4,   3,   6,   6,   4
};
static	const	short	yychk[] =
{
-1000, -23, -13,   2, -12, -16, -11,  27,  -3,  85,
  17,  18, -10,  -9,  -8,  90,   4,   7,   8,   9,
   5,   6,  10,  14, -19,  13, -15, 111,  28,  23,
  21,  22, -20, 113,  -5,  12,  29, -12,  90, -13,
 109,  90,  -1, -14,  -5,  74,  28,  -3, 101, 115,
 -17,  27, -21,  55, 106,  75,  76,  77,  78,  79,
  80,  81,  83,  82,  84,  52,  53, -18,  54, -24,
  28, -27, -27, -27, -27, -27, -27, -27, -27, -27,
 -20, -13,  91, -25, -26,  -5,  19,  20,  -4,  28,
  -2,  -1,  -5, 115, -17,  32,  32, 115, 108, -12,
  90, -14,  27,  -3,  -1,  90, 110,  95,  57, 100,
 101, 102, 103, 104,  94,  96,  97,  98,  99,  93,
  92, 107,  90, 100, 101,  90,  -1, -14, -17,  -1,
  69,  70,  71,  72,  73,  52, 114, -22,  11,  54,
   4,   7,   8,   9,   5,   6,  10, -22,  11,  54,
  -1, 115, 115, 115, 115, 115, 115, 115, 115, 115,
 115,  27, -21,  55, -18,  52,  90,  28, 110, -29,
  31,  -1,  85,  37,  38,  39,  40,  41,  56,  32,
  42,  86,  87,  88,  89, -19, 112,  -1, -26,  30,
  -5,  37,  91,  24,  95,  98,  99,  -1,  -5, -12,
  90,  -1, -14,  -1, -14,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 115,
 -17,  -1, 115, -17, 116, 110, 116,  27,  28,  52,
 114,  52, 114,  -1,  -1,  -1,  -1,  -1,  -1, 116,
  -1,  -1,  -1, 114, -22,  11, -22,  11,  28,  -1,
  -1, -14, -14, -14, -14,  52, 114, -14,  -1,  -1,
  -1,  -1, -28,  -1,  -1,  25,  -1,  -5,  -5, 116,
 110, 110,  58, 110,  -1, 110,  -1, -17,  27, -21,
  55,  27,  28,  27,  28, 116, 116, 116, 116, 110,
 116, 110, 110, 116, 114, 114,  27, -27,  38,  38,
  -7,  -1, -14, -14,  -1, 110,  -1, 110, 116,  -1,
  -1,  -1,  52,  -1,  -1, 116, 110,  94,  -1,  -1,
 116, 116, 116,  40,  26,  40,  26,  -1, 116, 116,
  -6, 100, 101, 102, 103,  -6,  -1,  -1,  26,  26
};
static	const	short	yydef[] =
{
  -2,  -2,   1,   3,   4,   0,   0,   0,   0,  12,
   0,  21,  17,  18,  19,  20,  62,  62,  62,  62,
  62,  62,  62,  62,  62,  57,   0,  47,   0,   0,
   0,   0,  88,  60,  90,  93,   0,   5,   6,   0,
   0,  11,   0,   0,   0, 137, 138, 139,   0,   0,
  98, 110,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0, 113, 120, 128,   0,
  22,  48,  49,  50,  51,  52,  53,  54,  55,  56,
  89,   0,   0,  26,  27,  29,   0,   0,   0,  41,
   0,  38,   0,   0,   0,  92,  91,   0,   7,   8,
  20,   0, 110, 139,   0,  13,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,  14,   0,   0,  15, 145,   0,  98,   0,
 147, 148, 149, 150, 151, 111,   0, 114, 136, 126,
 129, 130, 131, 132, 133, 134, 135, 117, 136, 127,
 163,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0, 112,   0,   0, 122, 121,  16,  23,   0,  61,
  64,  65,  67,   0,   0,   0,   0,   0,  76,  77,
  78,  80,  82,  84,  86,  87,  58,  25,  28,  30,
  31,   0,   0,  37,   0,   0,   0,   0,   0,   9,
  10, 100,   0,  -2,   0, 140, 141, 142, 143, 144,
  -2,  -2,  -2,  -2,  -2, 161, 162, 166,   0,   0,
 105,   0,   0, 106,  99,   0, 146, 125, 152, 115,
   0, 118,   0,   0,   0,   0,   0,   0,   0, 171,
   0,   0,   0,   0, 116, 136, 119, 136,  24,  63,
  66,  68,  69,  70,  71,  72,   0,  75,  79,  81,
  83,  85,  62,   0,   0,  36,  -2,  39,  40,  94,
   0,   0,   0,   0,   0,   0,   0,   0, 110,   0,
   0, 123, 153, 124, 154, 164, 165, 167, 168,   0,
 170,   0,   0, 174,   0,   0,  73,  59,   0,   0,
   0,  96,   0, 109, 101,   0, 102,   0, 107,   0,
   0,   0,  74,   0,   0,  95,   0, 108,   0,   0,
 169, 172, 173,  46,  33,  46,  35,  97, 103, 104,
   0,  42,  43,  44,  45,   0,   0,   0,  32,  34
};
static	const	short	yytok1[] =
{
   1,   4,   5,   6,   7,   8,   9,  10,  11,  12,
  13,  14,  15,  16,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0, 104,   0,   0,
 115, 116, 102, 100, 110, 101, 114, 103,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0, 109,   0,
   0,  91,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0, 113,   0, 112, 107,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0, 111,   0, 108
};
static	const	short	yytok2[] =
{
   2,   3,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,  17,  18,  19,  20,  21,
  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,
  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,
  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,
  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,
  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,
  82,  83,  84,  85,  86,  87,  88,  89,  90,  92,
  93,  94,  95,  96,  97,  98,  99, 105, 106
};
static	const	long	yytok3[] =
{
   0
};
#define YYFLAG 		-1000
#define YYERROR		goto yyerrlab
#define YYACCEPT	return(0)
#define YYABORT		return(1)
#define	yyclearin	yychar = -1
#define	yyerrok		yyerrflag = 0

#ifdef	yydebug
#include	"y.debug"
#else
#define	yydebug		0
static	const	char*	yytoknames[1];		/* for debugging */
static	const	char*	yystates[1];		/* for debugging */
#endif

/*	parser for yacc output	*/
#ifdef YYARG
#define	yynerrs		yyarg->yynerrs
#define	yyerrflag	yyarg->yyerrflag
#define yyval		yyarg->yyval
#define yylval		yyarg->yylval
#else
int	yynerrs = 0;		/* number of errors */
int	yyerrflag = 0;		/* error recovery flag */
#endif

static const char*
yytokname(int yyc)
{
	static char x[10];

	if(yyc > 0 && yyc <= sizeof(yytoknames)/sizeof(yytoknames[0]))
	if(yytoknames[yyc-1])
		return yytoknames[yyc-1];
	sprintf(x, "<%d>", yyc);
	return x;
}

static const char*
yystatname(int yys)
{
	static char x[10];

	if(yys >= 0 && yys < sizeof(yystates)/sizeof(yystates[0]))
	if(yystates[yys])
		return yystates[yys];
	sprintf(x, "<%d>\n", yys);
	return x;
}

static long
#ifdef YYARG
yylex1(struct Yyarg *yyarg)
#else
yylex1(void)
#endif
{
	long yychar;
	const long *t3p;
	int c;

#ifdef YYARG	
	yychar = yylex(yyarg);
#else
	yychar = yylex();
#endif
	if(yychar <= 0) {
		c = yytok1[0];
		goto out;
	}
	if(yychar < sizeof(yytok1)/sizeof(yytok1[0])) {
		c = yytok1[yychar];
		goto out;
	}
	if(yychar >= YYPRIVATE)
		if(yychar < YYPRIVATE+sizeof(yytok2)/sizeof(yytok2[0])) {
			c = yytok2[yychar-YYPRIVATE];
			goto out;
		}
	for(t3p=yytok3;; t3p+=2) {
		c = t3p[0];
		if(c == yychar) {
			c = t3p[1];
			goto out;
		}
		if(c == 0)
			break;
	}
	c = 0;

out:
	if(c == 0)
		c = yytok2[1];	/* unknown char */
	if(yydebug >= 3)
		printf("lex %.4lX %s\n", yychar, yytokname(c));
	return c;
}

int
#ifdef YYARG
yyparse(struct Yyarg *yyarg)
#else
yyparse(void)
#endif
{
	struct
	{
		YYSTYPE	yyv;
		int	yys;
	} yys[YYMAXDEPTH], *yyp, *yypt;
	const short *yyxi;
	int yyj, yym, yystate, yyn, yyg;
	long yychar;
#ifndef YYARG
	YYSTYPE save1, save2;
	int save3, save4;

	save1 = yylval;
	save2 = yyval;
	save3 = yynerrs;
	save4 = yyerrflag;
#endif

	yystate = 0;
	yychar = -1;
	yynerrs = 0;
	yyerrflag = 0;
	yyp = &yys[-1];
	goto yystack;

ret0:
	yyn = 0;
	goto ret;

ret1:
	yyn = 1;
	goto ret;

ret:
#ifndef YYARG
	yylval = save1;
	yyval = save2;
	yynerrs = save3;
	yyerrflag = save4;
#endif
	return yyn;

yystack:
	/* put a state and value onto the stack */
	if(yydebug >= 4)
		printf("char %s in %s", yytokname(yychar), yystatname(yystate));

	yyp++;
	if(yyp >= &yys[YYMAXDEPTH]) {
		yyerror("yacc stack overflow");
		goto ret1;
	}
	yyp->yys = yystate;
	yyp->yyv = yyval;

yynewstate:
	yyn = yypact[yystate];
	if(yyn <= YYFLAG)
		goto yydefault; /* simple state */
	if(yychar < 0)
#ifdef YYARG
		yychar = yylex1(yyarg);
#else
		yychar = yylex1();
#endif
	yyn += yychar;
	if(yyn < 0 || yyn >= YYLAST)
		goto yydefault;
	yyn = yyact[yyn];
	if(yychk[yyn] == yychar) { /* valid shift */
		yychar = -1;
		yyval = yylval;
		yystate = yyn;
		if(yyerrflag > 0)
			yyerrflag--;
		goto yystack;
	}

yydefault:
	/* default state action */
	yyn = yydef[yystate];
	if(yyn == -2) {
		if(yychar < 0)
#ifdef YYARG
		yychar = yylex1(yyarg);
#else
		yychar = yylex1();
#endif

		/* look through exception table */
		for(yyxi=yyexca;; yyxi+=2)
			if(yyxi[0] == -1 && yyxi[1] == yystate)
				break;
		for(yyxi += 2;; yyxi += 2) {
			yyn = yyxi[0];
			if(yyn < 0 || yyn == yychar)
				break;
		}
		yyn = yyxi[1];
		if(yyn < 0)
			goto ret0;
	}
	if(yyn == 0) {
		/* error ... attempt to resume parsing */
		switch(yyerrflag) {
		case 0:   /* brand new error */
			yyerror("syntax error");
			if(yydebug >= 1) {
				printf("%s", yystatname(yystate));
				printf("saw %s\n", yytokname(yychar));
			}
			goto yyerrlab;
		yyerrlab:
			yynerrs++;

		case 1:
		case 2: /* incompletely recovered error ... try again */
			yyerrflag = 3;

			/* find a state where "error" is a legal shift action */
			while(yyp >= yys) {
				yyn = yypact[yyp->yys] + YYERRCODE;
				if(yyn >= 0 && yyn < YYLAST) {
					yystate = yyact[yyn];  /* simulate a shift of "error" */
					if(yychk[yystate] == YYERRCODE)
						goto yystack;
				}

				/* the current yyp has no shift onn "error", pop stack */
				if(yydebug >= 2)
					printf("error recovery pops state %d, uncovers %d\n",
						yyp->yys, (yyp-1)->yys );
				yyp--;
			}
			/* there is no state on the stack with an error shift ... abort */
			goto ret1;

		case 3:  /* no shift yet; clobber input char */
			if(yydebug >= YYEOFCODE)
				printf("error recovery discards %s\n", yytokname(yychar));
			if(yychar == YYEOFCODE)
				goto ret1;
			yychar = -1;
			goto yynewstate;   /* try again in the same state */
		}
	}

	/* reduction by production yyn */
	if(yydebug >= 2)
		printf("reduce %d in:\n\t%s", yyn, yystatname(yystate));

	yypt = yyp;
	yyp -= yyr2[yyn];
	yyval = (yyp+1)->yyv;
	yym = yyn;

	/* consult goto table to find next state */
	yyn = yyr1[yyn];
	yyg = yypgo[yyn];
	yyj = yyg + yyp->yys + 1;

	if(yyj >= YYLAST || yychk[yystate=yyact[yyj]] != -yyn)
		yystate = yyact[yyg];
	switch(yym) {
		
case 3:
#line	63	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ ERROR "syntax error" WARNING; } break;
case 6:
#line	72	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ codegen = 1; makeiattr(0, 0); } break;
case 7:
#line	73	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ rightthing(yypt[-2].yyv.o, '}'); yyval.o = yypt[-1].yyv.o; } break;
case 8:
#line	74	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ y.o=yypt[-0].yyv.o; makevar(yypt[-2].yyv.p,PLACENAME,y); yyval.o = yypt[-0].yyv.o; } break;
case 9:
#line	75	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ y.o=yypt[-0].yyv.o; makevar(yypt[-3].yyv.p,PLACENAME,y); yyval.o = yypt[-0].yyv.o; } break;
case 10:
#line	76	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ y.o=yypt[-1].yyv.o; makevar(yypt[-3].yyv.p,PLACENAME,y); yyval.o = yypt[-1].yyv.o; } break;
case 11:
#line	77	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ y.f = yypt[-1].yyv.f; yyval.o = y.o; yyval.o = makenode(PLACE, 0); } break;
case 12:
#line	78	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ setdir(yypt[-0].yyv.i); yyval.o = makenode(PLACE, 0); } break;
case 13:
#line	79	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ printexpr(yypt[-1].yyv.f); yyval.o = makenode(PLACE, 0); } break;
case 14:
#line	80	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ printpos(yypt[-1].yyv.o); yyval.o = makenode(PLACE, 0); } break;
case 15:
#line	81	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ printf("%s\n", yypt[-1].yyv.p); free(yypt[-1].yyv.p); yyval.o = makenode(PLACE, 0); } break;
case 16:
#line	82	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ resetvar(); makeiattr(0, 0); yyval.o = makenode(PLACE, 0); } break;
case 22:
#line	91	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makevattr(yypt[-0].yyv.p); } break;
case 23:
#line	92	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makevattr(yypt[-0].yyv.p); } break;
case 24:
#line	93	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makevattr(yypt[-0].yyv.p); } break;
case 25:
#line	97	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f=y.f=yypt[-0].yyv.f; makevar(yypt[-2].yyv.p,VARNAME,y); checkscale(yypt[-2].yyv.p); } break;
case 26:
#line	101	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ copy(); } break;
case 29:
#line	108	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ copyfile(yypt[-0].yyv.p); } break;
case 30:
#line	109	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ copydef(yypt[-0].yyv.st); } break;
case 31:
#line	110	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ copyuntil(yypt[-0].yyv.p); } break;
case 32:
#line	115	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ forloop(yypt[-8].yyv.p, yypt[-6].yyv.f, yypt[-4].yyv.f, yypt[-2].yyv.i, yypt[-1].yyv.f, yypt[-0].yyv.p); } break;
case 33:
#line	117	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ forloop(yypt[-5].yyv.p, yypt[-3].yyv.f, yypt[-1].yyv.f, '+', 1.0, yypt[-0].yyv.p); } break;
case 34:
#line	119	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ forloop(yypt[-8].yyv.p, yypt[-6].yyv.f, yypt[-4].yyv.f, yypt[-2].yyv.i, yypt[-1].yyv.f, yypt[-0].yyv.p); } break;
case 35:
#line	121	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ forloop(yypt[-5].yyv.p, yypt[-3].yyv.f, yypt[-1].yyv.f, '+', 1.0, yypt[-0].yyv.p); } break;
case 36:
#line	125	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ ifstat(yypt[-2].yyv.f, yypt[-1].yyv.p, yypt[-0].yyv.p); } break;
case 37:
#line	126	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ ifstat(yypt[-1].yyv.f, yypt[-0].yyv.p, (char *) 0); } break;
case 39:
#line	130	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = strcmp(yypt[-2].yyv.p,yypt[-0].yyv.p) == 0; free(yypt[-2].yyv.p); free(yypt[-0].yyv.p); } break;
case 40:
#line	131	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = strcmp(yypt[-2].yyv.p,yypt[-0].yyv.p) != 0; free(yypt[-2].yyv.p); free(yypt[-0].yyv.p); } break;
case 41:
#line	135	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ y.f = 0; makevar(yypt[-0].yyv.p, VARNAME, y); } break;
case 42:
#line	138	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.i = '+'; } break;
case 43:
#line	139	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.i = '-'; } break;
case 44:
#line	140	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.i = '*'; } break;
case 45:
#line	141	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.i = '/'; } break;
case 46:
#line	142	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.i = ' '; } break;
case 47:
#line	147	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = leftthing('{'); } break;
case 48:
#line	151	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = boxgen(); } break;
case 49:
#line	152	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = circgen(yypt[-1].yyv.i); } break;
case 50:
#line	153	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = circgen(yypt[-1].yyv.i); } break;
case 51:
#line	154	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = arcgen(yypt[-1].yyv.i); } break;
case 52:
#line	155	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = linegen(yypt[-1].yyv.i); } break;
case 53:
#line	156	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = linegen(yypt[-1].yyv.i); } break;
case 54:
#line	157	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = linegen(yypt[-1].yyv.i); } break;
case 55:
#line	158	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = movegen(); } break;
case 56:
#line	159	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = textgen(); } break;
case 57:
#line	160	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = troffgen(yypt[-0].yyv.p); } break;
case 58:
#line	161	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o=rightthing(yypt[-2].yyv.o,']'); } break;
case 59:
#line	162	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = blockgen(yypt[-4].yyv.o, yypt[-1].yyv.o); } break;
case 60:
#line	166	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = leftthing('['); } break;
case 63:
#line	175	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makefattr(yypt[-1].yyv.i, !DEFAULT, yypt[-0].yyv.f); } break;
case 64:
#line	176	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makefattr(yypt[-0].yyv.i, DEFAULT, 0.0); } break;
case 65:
#line	177	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makefattr(curdir(), !DEFAULT, yypt[-0].yyv.f); } break;
case 66:
#line	178	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makefattr(yypt[-1].yyv.i, !DEFAULT, yypt[-0].yyv.f); } break;
case 67:
#line	179	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makefattr(yypt[-0].yyv.i, DEFAULT, 0.0); } break;
case 68:
#line	180	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makeoattr(yypt[-1].yyv.i, yypt[-0].yyv.o); } break;
case 69:
#line	181	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makeoattr(yypt[-1].yyv.i, yypt[-0].yyv.o); } break;
case 70:
#line	182	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makeoattr(yypt[-1].yyv.i, yypt[-0].yyv.o); } break;
case 71:
#line	183	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makeoattr(yypt[-1].yyv.i, yypt[-0].yyv.o); } break;
case 72:
#line	184	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makeiattr(WITH, yypt[-0].yyv.i); } break;
case 73:
#line	185	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makeoattr(PLACE, getblock(getlast(1,BLOCK), yypt[-0].yyv.p)); } break;
case 74:
#line	187	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makeoattr(PLACE, getpos(getblock(getlast(1,BLOCK), yypt[-1].yyv.p), yypt[-0].yyv.i)); } break;
case 75:
#line	188	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makeoattr(PLACE, yypt[-0].yyv.o); } break;
case 76:
#line	189	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makeiattr(SAME, yypt[-0].yyv.i); } break;
case 77:
#line	190	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ maketattr(yypt[-0].yyv.i, (char *) 0); } break;
case 78:
#line	191	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makeiattr(HEAD, yypt[-0].yyv.i); } break;
case 79:
#line	192	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makefattr(DOT, !DEFAULT, yypt[-0].yyv.f); } break;
case 80:
#line	193	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makefattr(DOT, DEFAULT, 0.0); } break;
case 81:
#line	194	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makefattr(DASH, !DEFAULT, yypt[-0].yyv.f); } break;
case 82:
#line	195	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makefattr(DASH, DEFAULT, 0.0); } break;
case 83:
#line	196	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makefattr(CHOP, !DEFAULT, yypt[-0].yyv.f); } break;
case 84:
#line	197	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makefattr(CHOP, DEFAULT, 0.0); } break;
case 85:
#line	198	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makefattr(FILL, !DEFAULT, yypt[-0].yyv.f); } break;
case 86:
#line	199	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ makefattr(FILL, DEFAULT, 0.0); } break;
case 90:
#line	208	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ maketattr(CENTER, yypt[-0].yyv.p); } break;
case 91:
#line	209	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ maketattr(yypt[-0].yyv.i, yypt[-1].yyv.p); } break;
case 92:
#line	210	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ addtattr(yypt[-0].yyv.i); } break;
case 94:
#line	214	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.p = sprintgen(yypt[-1].yyv.p); } break;
case 95:
#line	215	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.p = sprintgen(yypt[-3].yyv.p); } break;
case 96:
#line	219	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ exprsave(yypt[-0].yyv.f); yyval.i = 0; } break;
case 97:
#line	220	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ exprsave(yypt[-0].yyv.f); } break;
case 99:
#line	225	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = yypt[-1].yyv.o; } break;
case 100:
#line	226	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = makepos(yypt[-2].yyv.f, yypt[-0].yyv.f); } break;
case 101:
#line	227	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = fixpos(yypt[-4].yyv.o, yypt[-2].yyv.f, yypt[-0].yyv.f); } break;
case 102:
#line	228	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = fixpos(yypt[-4].yyv.o, -yypt[-2].yyv.f, -yypt[-0].yyv.f); } break;
case 103:
#line	229	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = fixpos(yypt[-6].yyv.o, yypt[-3].yyv.f, yypt[-1].yyv.f); } break;
case 104:
#line	230	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = fixpos(yypt[-6].yyv.o, -yypt[-3].yyv.f, -yypt[-1].yyv.f); } break;
case 105:
#line	231	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = addpos(yypt[-2].yyv.o, yypt[-0].yyv.o); } break;
case 106:
#line	232	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = subpos(yypt[-2].yyv.o, yypt[-0].yyv.o); } break;
case 107:
#line	233	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = makepos(getcomp(yypt[-3].yyv.o,DOTX), getcomp(yypt[-1].yyv.o,DOTY)); } break;
case 108:
#line	234	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = makebetween(yypt[-5].yyv.f, yypt[-3].yyv.o, yypt[-1].yyv.o); } break;
case 109:
#line	235	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = makebetween(yypt[-4].yyv.f, yypt[-2].yyv.o, yypt[-0].yyv.o); } break;
case 110:
#line	239	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ y = getvar(yypt[-0].yyv.p); yyval.o = y.o; } break;
case 111:
#line	240	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ y = getvar(yypt[-1].yyv.p); yyval.o = getpos(y.o, yypt[-0].yyv.i); } break;
case 112:
#line	241	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ y = getvar(yypt[-0].yyv.p); yyval.o = getpos(y.o, yypt[-1].yyv.i); } break;
case 113:
#line	242	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = gethere(); } break;
case 114:
#line	243	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = getlast(yypt[-1].yyv.i, yypt[-0].yyv.i); } break;
case 115:
#line	244	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = getpos(getlast(yypt[-2].yyv.i, yypt[-1].yyv.i), yypt[-0].yyv.i); } break;
case 116:
#line	245	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = getpos(getlast(yypt[-1].yyv.i, yypt[-0].yyv.i), yypt[-2].yyv.i); } break;
case 117:
#line	246	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = getfirst(yypt[-1].yyv.i, yypt[-0].yyv.i); } break;
case 118:
#line	247	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = getpos(getfirst(yypt[-2].yyv.i, yypt[-1].yyv.i), yypt[-0].yyv.i); } break;
case 119:
#line	248	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = getpos(getfirst(yypt[-1].yyv.i, yypt[-0].yyv.i), yypt[-2].yyv.i); } break;
case 121:
#line	250	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = getpos(yypt[-1].yyv.o, yypt[-0].yyv.i); } break;
case 122:
#line	251	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = getpos(yypt[-0].yyv.o, yypt[-1].yyv.i); } break;
case 123:
#line	255	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = getblock(getlast(yypt[-3].yyv.i,yypt[-2].yyv.i), yypt[-0].yyv.p); } break;
case 124:
#line	256	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.o = getblock(getfirst(yypt[-3].yyv.i,yypt[-2].yyv.i), yypt[-0].yyv.p); } break;
case 125:
#line	257	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ y = getvar(yypt[-2].yyv.p); yyval.o = getblock(y.o, yypt[-0].yyv.p); } break;
case 126:
#line	261	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.i = yypt[-1].yyv.i + 1; } break;
case 127:
#line	262	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.i = yypt[-1].yyv.i; } break;
case 128:
#line	263	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.i = 1; } break;
case 138:
#line	279	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = getfval(yypt[-0].yyv.p); } break;
case 140:
#line	281	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-2].yyv.f + yypt[-0].yyv.f; } break;
case 141:
#line	282	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-2].yyv.f - yypt[-0].yyv.f; } break;
case 142:
#line	283	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-2].yyv.f * yypt[-0].yyv.f; } break;
case 143:
#line	284	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ if (yypt[-0].yyv.f == 0.0) {
					ERROR "division by 0" WARNING; yypt[-0].yyv.f = 1; }
				  yyval.f = yypt[-2].yyv.f / yypt[-0].yyv.f; } break;
case 144:
#line	287	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ if ((long)yypt[-0].yyv.f == 0) {
					ERROR "mod division by 0" WARNING; yypt[-0].yyv.f = 1; }
				  yyval.f = (long)yypt[-2].yyv.f % (long)yypt[-0].yyv.f; } break;
case 145:
#line	290	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = -yypt[-0].yyv.f; } break;
case 146:
#line	291	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-1].yyv.f; } break;
case 147:
#line	292	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = getcomp(yypt[-1].yyv.o, yypt[-0].yyv.i); } break;
case 148:
#line	293	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = getcomp(yypt[-1].yyv.o, yypt[-0].yyv.i); } break;
case 149:
#line	294	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = getcomp(yypt[-1].yyv.o, yypt[-0].yyv.i); } break;
case 150:
#line	295	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = getcomp(yypt[-1].yyv.o, yypt[-0].yyv.i); } break;
case 151:
#line	296	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = getcomp(yypt[-1].yyv.o, yypt[-0].yyv.i); } break;
case 152:
#line	297	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ y = getvar(yypt[-2].yyv.p); yyval.f = getblkvar(y.o, yypt[-0].yyv.p); } break;
case 153:
#line	298	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = getblkvar(getlast(yypt[-3].yyv.i,yypt[-2].yyv.i), yypt[-0].yyv.p); } break;
case 154:
#line	299	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = getblkvar(getfirst(yypt[-3].yyv.i,yypt[-2].yyv.i), yypt[-0].yyv.p); } break;
case 155:
#line	300	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-2].yyv.f > yypt[-0].yyv.f; } break;
case 156:
#line	301	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-2].yyv.f < yypt[-0].yyv.f; } break;
case 157:
#line	302	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-2].yyv.f <= yypt[-0].yyv.f; } break;
case 158:
#line	303	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-2].yyv.f >= yypt[-0].yyv.f; } break;
case 159:
#line	304	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-2].yyv.f == yypt[-0].yyv.f; } break;
case 160:
#line	305	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-2].yyv.f != yypt[-0].yyv.f; } break;
case 161:
#line	306	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-2].yyv.f && yypt[-0].yyv.f; } break;
case 162:
#line	307	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-2].yyv.f || yypt[-0].yyv.f; } break;
case 163:
#line	308	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = !(yypt[-0].yyv.f); } break;
case 164:
#line	309	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = Log10(yypt[-1].yyv.f); } break;
case 165:
#line	310	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = Exp(yypt[-1].yyv.f * log(10.0)); } break;
case 166:
#line	311	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = pow(yypt[-2].yyv.f, yypt[-0].yyv.f); } break;
case 167:
#line	312	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = sin(yypt[-1].yyv.f); } break;
case 168:
#line	313	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = cos(yypt[-1].yyv.f); } break;
case 169:
#line	314	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = atan2(yypt[-3].yyv.f, yypt[-1].yyv.f); } break;
case 170:
#line	315	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = Sqrt(yypt[-1].yyv.f); } break;
case 171:
#line	316	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = (float)rand() / 32767.0; /* might be 2^31-1 */ } break;
case 172:
#line	317	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-3].yyv.f >= yypt[-1].yyv.f ? yypt[-3].yyv.f : yypt[-1].yyv.f; } break;
case 173:
#line	318	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = yypt[-3].yyv.f <= yypt[-1].yyv.f ? yypt[-3].yyv.f : yypt[-1].yyv.f; } break;
case 174:
#line	319	"/usr/local/plan9/src/cmd/tpic/picy.y"
{ yyval.f = (long) yypt[-1].yyv.f; } break;
	}
	goto yystack;  /* stack new state and value */
}
