typedef struct Macho Macho;
typedef struct MachoCmd MachoCmd;

enum
{
	MachoCpuVax = 1,
	MachoCpu68000 = 6,
	MachoCpu386 = 7,
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

	MachoFileObject = 1,
	MachoFileExecutable = 2,
	MachoFileFvmlib = 3,
	MachoFileCore = 4,
	MachoFilePreload = 5,
};

struct MachoCmd
{
	int type;
	ulong off;
	ulong size;
	struct {
		char name[16+1];
		ulong vmaddr;
		ulong vmsize;
		ulong fileoff;
		ulong filesz;
		ulong maxprot;
		ulong initprot;
		ulong nsect;
		ulong flags;
	} seg;
	struct {
		ulong symoff;
		ulong nsyms;
		ulong stroff;
		ulong strsize;
	} sym;
};

struct Macho
{
	int fd;
	uint cputype;
	uint subcputype;
	ulong filetype;
	ulong flags;
	MachoCmd *cmd;
	uint ncmd;
	u32int (*e4)(uchar*);
	int (*coreregs)(Macho*, uchar**);
};

Macho *machoopen(char*);
Macho *machoinit(int);
void machoclose(Macho*);
int coreregsmachopower(Macho*, uchar**);
