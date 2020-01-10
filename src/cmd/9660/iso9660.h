/*
 * iso9660.h
 *
 * Routines and data structures to support reading and writing
 * ISO 9660 CD images. See the ISO 9660 or ECMA 119 standards.
 *
 * Also supports Rock Ridge extensions for long file names and Unix stuff.
 * Also supports Microsoft's Joliet extensions for Unicode and long file names.
 * Also supports El Torito bootable CD spec.
 */

typedef struct Cdimg Cdimg;
typedef struct Cdinfo Cdinfo;
typedef struct Conform Conform;
typedef struct Direc Direc;
typedef struct Dumproot Dumproot;
typedef struct Voldesc Voldesc;
typedef struct XDir XDir;

#ifndef CHLINK
#define CHLINK 0
#endif

struct XDir {
	char	*name;
	char	*uid;
	char	*gid;
	char	*symlink;
	ulong   uidno;   /* Numeric uid */
	ulong   gidno;   /* Numeric gid */

	ulong	mode;
	ulong	atime;
	ulong	mtime;
	ulong   ctime;

        vlong   length;
};

/*
 * A directory entry in a ISO9660 tree.
 * The extra data (uid, etc.) here is put into the system use areas.
 */
struct Direc {
	char *name;	/* real name */
	char *confname;	/* conformant name */
	char *srcfile;	/* file to copy onto the image */

	ulong block;
	ulong length;
	int flags;

	char *uid;
	char *gid;
	char *symlink;
	ulong mode;
	long atime;
	long ctime;
	long mtime;

	ulong uidno;
	ulong gidno;

	Direc *child;
	int nchild;
};
enum {  /* Direc flags */
	Dbadname = 1<<0  /* Non-conformant name     */
};

/*
 * Data found in a volume descriptor.
 */
struct Voldesc {
	char *systemid;
	char *volumeset;
	char *publisher;
	char *preparer;
	char *application;

	/* file names for various parameters */
	char *abstract;
	char *biblio;
	char *notice;

	/* path table */
	ulong pathsize;
	ulong lpathloc;
	ulong mpathloc;

	/* root of file tree */
	Direc root;
};

/*
 * An ISO9660 CD image.  Various parameters are kept in memory but the
 * real image file is opened for reading and writing on fd.
 *
 * The bio buffers brd and bwr moderate reading and writing to the image.
 * The routines we use are careful to flush one before or after using the other,
 * as necessary.
 */
struct Cdimg {
	char *file;
	int fd;
	ulong dumpblock;
	ulong nextblock;
	ulong iso9660pvd;
	ulong jolietsvd;
	ulong pathblock;
	ulong rrcontin; /* rock ridge continuation offset */
	ulong nulldump;	/* next dump block */
	ulong nconform;	/* number of conform entries written already */
	ulong bootcatptr;
	ulong bootcatblock;
	ulong bootimageptr;
	Direc *bootdirec;
	char *bootimage;

	Biobuf brd;
	Biobuf bwr;

	int flags;

	Voldesc iso;
	Voldesc joliet;
};
enum {	/* Cdimg->flags, Cdinfo->flags */
	CDjoliet = 1<<0,
	CDplan9 = 1<<1,
	CDconform = 1<<2,
	CDrockridge = 1<<3,
	CDnew = 1<<4,
	CDdump = 1<<5,
	CDbootable = 1<<6
};

typedef struct Tx Tx;
struct Tx {
	char *bad;	/* atoms */
	char *good;
};

struct Conform {
	Tx *t;
	int nt;	/* delta = 32 */
};

struct Cdinfo {
	int flags;

	char *volumename;

	char *volumeset;
	char *publisher;
	char *preparer;
	char *application;
	char *bootimage;
};

enum {
	Blocklen = 2048
};

/*
 * This is a doubly binary tree.
 * We have a tree keyed on the MD5 values
 * as well as a tree keyed on the block numbers.
 */
typedef struct Dump Dump;
typedef struct Dumpdir Dumpdir;

struct Dump {
	Cdimg *cd;
	Dumpdir *md5root;
	Dumpdir *blockroot;
};

struct Dumpdir {
	char *name;
	uchar md5[MD5dlen];
	ulong block;
	ulong length;
	Dumpdir *md5left;
	Dumpdir *md5right;
	Dumpdir *blockleft;
	Dumpdir *blockright;
};

struct Dumproot {
	char *name;
	int nkid;
	Dumproot *kid;
	Direc root;
	Direc jroot;
};

/*
 * ISO9660 on-CD structures.
 */
typedef struct Cdir Cdir;
typedef struct Cpath Cpath;
typedef struct Cvoldesc Cvoldesc;

/* a volume descriptor block */
struct Cvoldesc {
	uchar	magic[8];	/* 0x01, "CD001", 0x01, 0x00 */
	uchar	systemid[32];	/* system identifier */
	uchar	volumeid[32];	/* volume identifier */
	uchar	unused[8];	/* character set in secondary desc */
	uchar	volsize[8];	/* volume size */
	uchar	charset[32];
	uchar	volsetsize[4];	/* volume set size = 1 */
	uchar	volseqnum[4];	/* volume sequence number = 1 */
	uchar	blocksize[4];	/* logical block size */
	uchar	pathsize[8];	/* path table size */
	uchar	lpathloc[4];	/* Lpath */
	uchar	olpathloc[4];	/* optional Lpath */
	uchar	mpathloc[4];	/* Mpath */
	uchar	ompathloc[4];	/* optional Mpath */
	uchar	rootdir[34];	/* directory entry for root */
	uchar	volumeset[128];	/* volume set identifier */
	uchar	publisher[128];
	uchar	preparer[128];	/* data preparer identifier */
	uchar	application[128];	/* application identifier */
	uchar	notice[37];	/* copyright notice file */
	uchar	abstract[37];	/* abstract file */
	uchar	biblio[37];	/* bibliographic file */
	uchar	cdate[17];	/* creation date */
	uchar	mdate[17];	/* modification date */
	uchar	xdate[17];	/* expiration date */
	uchar	edate[17];	/* effective date */
	uchar	fsvers;		/* file system version = 1 */
};

/* a directory entry */
struct Cdir {
	uchar	len;
	uchar	xlen;
	uchar	dloc[8];
	uchar	dlen[8];
	uchar	date[7];
	uchar	flags;
	uchar	unitsize;
	uchar	gapsize;
	uchar	volseqnum[4];
	uchar	namelen;
	uchar	name[1];	/* chumminess */
};

/* a path table entry */
struct Cpath {
	uchar   namelen;
	uchar   xlen;
	uchar   dloc[4];
	uchar   parent[2];
	uchar   name[1];        /* chumminess */
};

enum { /* Rockridge flags */
	RR_PX = 1<<0,
	RR_PN = 1<<1,
	RR_SL = 1<<2,
	RR_NM = 1<<3,
	RR_CL = 1<<4,
	RR_PL = 1<<5,
	RR_RE = 1<<6,
	RR_TF = 1<<7
};

enum { /* CputrripTF type argument */
	TFcreation = 1<<0,
	TFmodify = 1<<1,
	TFaccess = 1<<2,
	TFattributes = 1<<3,
	TFbackup = 1<<4,
	TFexpiration = 1<<5,
	TFeffective = 1<<6,
	TFlongform = 1<<7
};

enum { /* CputrripNM flag types */
	NMcontinue = 1<<0,
	NMcurrent = 1<<1,
	NMparent = 1<<2,
	NMroot = 1<<3,
	NMvolroot = 1<<4,
	NMhost = 1<<5
};

/* boot.c */
void Cputbootvol(Cdimg*);
void Cputbootcat(Cdimg*);
void Cupdatebootvol(Cdimg*);
void Cupdatebootcat(Cdimg*);
void findbootimage(Cdimg*, Direc*);

/* cdrdwr.c */
Cdimg *createcd(char*, Cdinfo);
Cdimg *opencd(char*, Cdinfo);
void Creadblock(Cdimg*, void*, ulong, ulong);
ulong big(void*, int);
ulong little(void*, int);
int parsedir(Cdimg*, Direc*, uchar*, int, char *(*)(uchar*, int));
void setroot(Cdimg*, ulong, ulong, ulong);
void setvolsize(Cdimg*, ulong, ulong);
void setpathtable(Cdimg*, ulong, ulong, ulong, ulong);
void Cputc(Cdimg*, int);
void Cputnl(Cdimg*, ulong, int);
void Cputnm(Cdimg*, ulong, int);
void Cputn(Cdimg*, long, int);
void Crepeat(Cdimg*, int, int);
void Cputs(Cdimg*, char*, int);
void Cwrite(Cdimg*, void*, int);
void Cputr(Cdimg*, Rune);
void Crepeatr(Cdimg*, Rune, int);
void Cputrs(Cdimg*, Rune*, int);
void Cputrscvt(Cdimg*, char*, int);
void Cpadblock(Cdimg*);
void Cputdate(Cdimg*, ulong);
void Cputdate1(Cdimg*, ulong);
void Cread(Cdimg*, void*, int);
void Cwflush(Cdimg*);
void Cwseek(Cdimg*, ulong);
ulong Cwoffset(Cdimg*);
ulong Croffset(Cdimg*);
int Cgetc(Cdimg*);
void Crseek(Cdimg*, ulong);
char *Crdline(Cdimg*, int);
int Clinelen(Cdimg*);

/* conform.c */
void rdconform(Cdimg*);
char *conform(char*, int);
void wrconform(Cdimg*, int, ulong*, ulong*);

/* direc.c */
void mkdirec(Direc*, XDir*);
Direc *walkdirec(Direc*, char*);
Direc *adddirec(Direc*, char*, XDir*);
void copydirec(Direc*, Direc*);
void checknames(Direc*, int (*)(char*));
void convertnames(Direc*, char* (*)(char*, char*));
void dsort(Direc*, int (*)(const void*, const void*));
void setparents(Direc*);

/* dump.c */
ulong Cputdumpblock(Cdimg*);
int hasdump(Cdimg*);
Dump *dumpcd(Cdimg*, Direc*);
Dumpdir *lookupmd5(Dump*, uchar*);
void insertmd5(Dump*, char*, uchar*, ulong, ulong);

Direc readdumpdirs(Cdimg*, XDir*, char*(*)(uchar*,int));
char *adddumpdir(Direc*, ulong, XDir*);
void copybutname(Direc*, Direc*);

void readkids(Cdimg*, Direc*, char*(*)(uchar*,int));
void freekids(Direc*);
void readdumpconform(Cdimg*);
void rmdumpdir(Direc*, char*);

/* ichar.c */
char *isostring(uchar*, int);
int isbadiso9660(char*);
int isocmp(const void*, const void*);
int isisofrog(char);
void Cputisopvd(Cdimg*, Cdinfo);

/* jchar.c */
char *jolietstring(uchar*, int);
int isbadjoliet(char*);
int jolietcmp(const void*, const void*);
int isjolietfrog(Rune);
void Cputjolietsvd(Cdimg*, Cdinfo);

/* path.c */
void writepathtables(Cdimg*);

/* util.c */
void *emalloc(ulong);
void *erealloc(void*, ulong);
char *atom(char*);
char *struprcpy(char*, char*);
int chat(char*, ...);

/* unix.c, plan9.c */
void dirtoxdir(XDir*, Dir*);
void fdtruncate(int, ulong);
long uidno(char*);
long gidno(char*);

/* rune.c */
Rune *strtorune(Rune*, char*);
Rune *runechr(Rune*, Rune);
int runecmp(Rune*, Rune*);

/* sysuse.c */
int Cputsysuse(Cdimg*, Direc*, int, int, int);

/* write.c */
void writefiles(Dump*, Cdimg*, Direc*);
void writedirs(Cdimg*, Direc*, int(*)(Cdimg*, Direc*, int, int, int));
void writedumpdirs(Cdimg*, Direc*, int(*)(Cdimg*, Direc*, int, int, int));
int Cputisodir(Cdimg*, Direc*, int, int, int);
int Cputjolietdir(Cdimg*, Direc*, int, int, int);
void Cputendvd(Cdimg*);

enum {
	Blocksize = 2048,
	Ndirblock = 16,		/* directory blocks allocated at once */

	DTdot = 0,
	DTdotdot,
	DTiden,
	DTroot,
	DTrootdot
};

extern ulong now;
extern Conform *map;
extern int chatty;
extern int docolon;
extern int mk9660;
