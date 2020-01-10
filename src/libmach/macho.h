typedef struct Macho Macho;
typedef struct MachoCmd MachoCmd;
typedef struct MachoSeg MachoSeg;
typedef struct MachoSect MachoSect;
typedef struct MachoRel MachoRel;
typedef struct MachoSymtab MachoSymtab;
typedef struct MachoSym MachoSym;
typedef struct MachoDysymtab MachoDysymtab;

enum
{
	MachoCpuVax = 1,
	MachoCpu68000 = 6,
	MachoCpu386 = 7,
	MachoCpuAmd64 = 0x1000007,
	MachoCpuMips = 8,
	MachoCpu98000 = 10,
	MachoCpuHppa = 11,
	MachoCpuArm = 12,
	MachoCpu88000 = 13,
	MachoCpuSparc = 14,
	MachoCpu860 = 15,
	MachoCpuAlpha = 16,
	MachoCpuPower = 18,

	MachoCmdSegment = 1,
	MachoCmdSymtab = 2,
	MachoCmdSymseg = 3,
	MachoCmdThread = 4,
	MachoCmdDysymtab = 11,
	MachoCmdSegment64 = 25,

	MachoFileObject = 1,
	MachoFileExecutable = 2,
	MachoFileFvmlib = 3,
	MachoFileCore = 4,
	MachoFilePreload = 5
};

struct MachoSeg
{
	char name[16+1];
	uint64 vmaddr;
	uint64 vmsize;
	uint32 fileoff;
	uint32 filesz;
	uint32 maxprot;
	uint32 initprot;
	uint32 nsect;
	uint32 flags;
	MachoSect *sect;
};

struct MachoSect
{
	char	name[16+1];
	char	segname[16+1];
	uint64 addr;
	uint64 size;
	uint32 offset;
	uint32 align;
	uint32 reloff;
	uint32 nreloc;
	uint32 flags;

	MachoRel *rel;
};

struct MachoRel
{
	uint32 addr;
	uint32 symnum;
	uint8 pcrel;
	uint8 length;
	uint8 extrn;
	uint8 type;
};

struct MachoSymtab
{
	uint32 symoff;
	uint32 nsym;
	uint32 stroff;
	uint32 strsize;

	char *str;
	MachoSym *sym;
};

struct MachoSym
{
	char *name;
	uint8 type;
	uint8 sectnum;
	uint16 desc;
	char kind;
	uint64 value;
};

struct MachoDysymtab
{
	uint32 ilocalsym;
	uint32 nlocalsym;
	uint32 iextdefsym;
	uint32 nextdefsym;
	uint32 iundefsym;
	uint32 nundefsym;
	uint32 tocoff;
	uint32 ntoc;
	uint32 modtaboff;
	uint32 nmodtab;
	uint32 extrefsymoff;
	uint32 nextrefsyms;
	uint32 indirectsymoff;
	uint32 nindirectsyms;
	uint32 extreloff;
	uint32 nextrel;
	uint32 locreloff;
	uint32 nlocrel;
};

struct MachoCmd
{
	int type;
	uint32 off;
	uint32 size;
	MachoSeg seg;
	MachoSymtab sym;
	MachoDysymtab dsym;
};

struct Macho
{
	int fd;
	int is64;
	uint cputype;
	uint subcputype;
	uint32 filetype;
	uint32 flags;
	MachoCmd *cmd;
	uint ncmd;
	uint16 (*e2)(uchar*);
	uint32 (*e4)(uchar*);
	uint64 (*e8)(uchar*);
	int (*coreregs)(Macho*, uchar**);
};

Macho *machoopen(char*);
Macho *machoinit(int);
void machoclose(Macho*);
int coreregsmachopower(Macho*, uchar**);
int macholoadrel(Macho*, MachoSect*);
int macholoadsym(Macho*, MachoSymtab*);
