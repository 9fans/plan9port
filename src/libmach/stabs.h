typedef struct StabSym StabSym;
typedef struct Stab Stab;	/* defined in mach.h */

struct StabSym
{
	char *name;
	uchar type;
	uchar other;
	u16int desc;
	u32int value;
};

enum
{
	EXT = 0x01,

	N_UNDEF = 0x00,
	N_ABS = 0x02,
	N_TEXT = 0x04,
	N_DATA = 0x06,
	N_BSS = 0x08,
	N_INDR = 0x0A,
	N_FN_SEQ = 0x0C,
	N_WEAKU = 0x0D,
	N_WEAKA = 0x0E,
	N_WEAKT = 0x0F,
	N_WEAKD = 0x10,
	N_WEAKB = 0x11,
	N_COMM = 0x12,
	N_SETA = 0x14,
	N_SETT = 0x16,

	N_GSYM = 0x20,
	N_FNAME = 0x22,
	N_FUN = 0x24,
	N_STSYM = 0x26,
	N_LCSYM = 0x28,
	N_MAIN = 0x2A,
	N_ROSYM = 0x2C,
	N_PC = 0x30,
	N_NSYMS = 0x32,
	N_NOMAP = 0x34,
	N_OBJ = 0x38,
	N_OPT = 0x3C,
	N_RSYM = 0x40,
	N_M2C = 0x42,
	N_SLINE = 0x44,
	N_DSLINE = 0x46,
	N_BSLINE = 0x48,
	N_BROWS = 0x48,
	N_DEFD = 0x4A,
	N_FLINE = 0x4C,
	N_EHDECL = 0x50,
	N_MOD2 = 0x50,
	N_CATCH = 0x54,
	N_SSYM = 0x60,
	N_ENDM = 0x62,
	N_SO = 0x64,
	N_ALIAS = 0x6C,
	N_LSYM = 0x80,
	N_BINCL = 0x82,
	N_SOL = 0x84,
	N_PSYM = 0xA0,
	N_EINCL = 0xA2,
	N_ENTRY = 0xA4,
	N_LBRAC = 0xC0,
	N_EXCL = 0xC2,
	N_SCOPE = 0xC4,
	N_RBRAC = 0xE0,
	N_BCOMM = 0xE2,
	N_ECOMM = 0xE4,
	N_ECOML = 0xE8,
	N_WITH = 0xEA,
	N_LENG = 0xFE
};

/*
 symbol descriptors

[(0-9\-]	variable on stack
:		C++ nested symbol
a		parameter by reference
b		based variable
c		constant
C		conformant array bound
		name of caught exception (N_CATCH)
d		fp register variable
D		fp parameter
f		file scope function
F		global function
G		global variable
i		register parameter?
I		nested procedure
J		nested function
L		label name
m		module
p		arg list parameter
pP
pF
P		register param (N_PSYM)
		proto of ref fun (N_FUN)
Q		static procedure
R		register param
r		register variable
S		file scope variable
s		local variable
t		type name
T		sue tag
v		param by reference
V		procedure scope static variable
x		conformant array
X		function return variable

*/

int stabsym(Stab*, int, StabSym*);
