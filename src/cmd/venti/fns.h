/*
 * sorted by 4,/^$/|sort -bd +1
 */
int		addarena(Arena *name);
ZBlock		*alloczblock(u32int size, int zeroed);
Arena		*amapitoa(Index *index, u64int a, u64int *aa);
u64int		arenadirsize(Arena *arena, u32int clumps);
void		arenaupdate(Arena *arena, u32int size, u8int *score);
void		backsumarena(Arena *arena);
u32int		buildbucket(Index *ix, IEStream *ies, IBucket *ib);
void		checkdcache(void);
void		checklumpcache(void);
int		clumpinfoeq(ClumpInfo *c, ClumpInfo *d);
int		clumpinfoeq(ClumpInfo *c, ClumpInfo *d);
u32int		clumpmagic(Arena *arena, u64int aa);
int		delarena(Arena *arena);
void		*emalloc(ulong);
void		*erealloc(void *, ulong);
char		*estrdup(char*);
void		*ezmalloc(ulong);
Arena		*findarena(char *name);
ISect		*findisect(Index *ix, u32int buck);
int		flushciblocks(Arena *arena);
void		fmtzbinit(Fmt *f, ZBlock *b);
void		freearena(Arena *arena);
void		freearenapart(ArenaPart *ap, int freearenas);
void		freeiestream(IEStream *ies);
void		freeifile(IFile *f);
void		freeisect(ISect *is);
void		freeindex(Index *index);
void		freepart(Part *part);
void		freezblock(ZBlock *b);
DBlock		*getdblock(Part *part, u64int addr, int read);
u32int		hashbits(u8int *score, int nbits);
int		httpdinit(char *address);
int		iaddrcmp(IAddr *ia1, IAddr *ia2);
int		ientrycmp(const void *vie1, const void *vie2);
char		*ifileline(IFile *f);
int		ifilename(IFile *f, char *dst);
int		ifileu32int(IFile *f, u32int *r);
int		indexsect(Index *ix, u8int *score);
Arena		*initarena(Part *part, u64int base, u64int size, u32int blocksize);
ArenaPart	*initarenapart(Part *part);
int		initarenasum(void);
void		initdcache(u32int mem);
void		initicache(int bits, int depth);
IEStream	*initiestream(Part *part, u64int off, u64int clumps, u32int size);
ISect		*initisect(Part *part);
Index		*initindex(char *name, ISect **sects, int n);
void		initlumpcache(u32int size, u32int nblocks);
int		initlumpqueues(int nq);
Part*		initpart(char *name, int writable);
int		initventi(char *config);
void		insertlump(Lump *lump, Packet *p);
int		insertscore(u8int *score, IAddr *ia, int write);
ZBlock		*loadclump(Arena *arena, u64int aa, int blocks, Clump *cl, u8int *score, int verify);
int		loadientry(Index *index, u8int *score, int type, IEntry *ie);
void		logerr(int severity, char *fmt, ...);
Lump		*lookuplump(u8int *score, int type);
int		lookupscore(u8int *score, int type, IAddr *ia, int *rac);
int		maparenas(AMap *am, Arena **arenas, int n, char *what);
int		namecmp(char *s, char *t);
void		namecp(char *dst, char *src);
int		nameok(char *name);
Arena		*newarena(Part *part, char *name, u64int base, u64int size, u32int blocksize);
ArenaPart	*newarenapart(Part *part, u32int blocksize, u32int tabsize);
ISect		*newisect(Part *part, char *name, u32int blocksize, u32int tabsize);
Index		*newindex(char *name, ISect **sects, int n);
u32int		now(void);
int		okamap(AMap *am, int n, u64int start, u64int stop, char *what);
int		outputamap(Fmt *f, AMap *am, int n);
int		outputindex(Fmt *f, Index *ix);
int		packarena(Arena *arena, u8int *buf);
int		packarenahead(ArenaHead *head, u8int *buf);
int		packarenapart(ArenaPart *as, u8int *buf);
int		packclump(Clump *c, u8int *buf);
void		packclumpinfo(ClumpInfo *ci, u8int *buf);
void		packibucket(IBucket *b, u8int *buf);
void		packientry(IEntry *i, u8int *buf);
int		packisect(ISect *is, u8int *buf);
void		packmagic(u32int magic, u8int *buf);
ZBlock		*packet2zblock(Packet *p, u32int size);
int		parseamap(IFile *f, AMapN *amn);
int		parseindex(IFile *f, Index *ix);
void		partblocksize(Part *part, u32int blocksize);
int		partifile(IFile *f, Part *part, u64int start, u32int size);
void		printarenapart(int fd, ArenaPart *ap);
void		printarena(int fd, Arena *arena);
void		printindex(int fd, Index *ix);
void		printstats(void);
void		putdblock(DBlock *b);
void		putlump(Lump *b);
void		queueflush(void);
int		queuewrite(Lump *b, Packet *p, int creator);
u32int		readarena(Arena *arena, u64int aa, u8int *buf, long n);
int		readarenamap(AMapN *amn, Part *part, u64int base, u32int size);
int		readclumpinfo(Arena *arena, int clump, ClumpInfo *ci);
int		readclumpinfos(Arena *arena, int clump, ClumpInfo *cis, int n);
ZBlock		*readfile(char *name);
int		readifile(IFile *f, char *name);
Packet		*readlump(u8int *score, int type, u32int size);
int		readpart(Part *part, u64int addr, u8int *buf, u32int n);
int		runconfig(char *config, Config*);
int		scorecmp(u8int *, u8int *);
void		scoremem(u8int *score, u8int *buf, int size);
void		seterr(int severity, char *fmt, ...);
u64int		sortrawientries(Index *ix, Part *tmp, u64int *tmpoff);
void		statsinit(void);
int		storeclump(Index *index, ZBlock *b, u8int *score, int type, u32int creator, IAddr *ia);
int		storeientry(Index *index, IEntry *m);
int		strscore(char *s, u8int *score);
int		stru32int(char *s, u32int *r);
int		stru64int(char *s, u64int *r);
void		sumarena(Arena *arena);
int		syncarena(Arena *arena, u32int n, int zok, int fix);
int		syncarenaindex(Index *ix, Arena *arena, u32int clump, u64int a, int fix);
int		syncindex(Index *ix, int fix);
int		u64log2(u64int v);
u64int		unittoull(char *s);
int		unpackarena(Arena *arena, u8int *buf);
int		unpackarenahead(ArenaHead *head, u8int *buf);
int		unpackarenapart(ArenaPart *as, u8int *buf);
int		unpackclump(Clump *c, u8int *buf);
void		unpackclumpinfo(ClumpInfo *ci, u8int *buf);
void		unpackibucket(IBucket *b, u8int *buf);
void		unpackientry(IEntry *i, u8int *buf);
int		unpackisect(ISect *is, u8int *buf);
u32int		unpackmagic(u8int *buf);
int		vtproc(void(*)(void*), void*);
int		vttypevalid(int type);
int		wbarena(Arena *arena);
int		wbarenahead(Arena *arena);
int		wbarenamap(AMap *am, int n, Part *part, u64int base, u64int size);
int		wbarenapart(ArenaPart *ap);
int		wbisect(ISect *is);
int		wbindex(Index *ix);
int		whackblock(u8int *dst, u8int *src, int ssize);
u64int		writeaclump(Arena *a, Clump *c, u8int *clbuf);
u32int		writearena(Arena *arena, u64int aa, u8int *clbuf, u32int n);
int		writeclumpinfo(Arena *arean, int clump, ClumpInfo *ci);
u64int		writeiclump(Index *ix, Clump *c, u8int *clbuf);
int		writelump(Packet *p, u8int *score, int type, u32int creator);
int		writepart(Part *part, u64int addr, u8int *buf, u32int n);
int		writeqlump(Lump *u, Packet *p, int creator);
Packet		*zblock2packet(ZBlock *zb, u32int size);
void		zeropart(Part *part, int blocksize);

/*
#pragma	varargck	argpos	sysfatal		1
#pragma	varargck	argpos	logerr		2
#pragma	varargck	argpos	SetErr		2
*/

#define scorecmp(h1,h2)		memcmp((h1),(h2),VtScoreSize)
#define scorecp(h1,h2)		memmove((h1),(h2),VtScoreSize)

#define MK(t)			((t*)emalloc(sizeof(t)))
#define MKZ(t)			((t*)ezmalloc(sizeof(t)))
#define MKN(t,n)		((t*)emalloc((n)*sizeof(t)))
#define MKNZ(t,n)		((t*)ezmalloc((n)*sizeof(t)))
#define MKNA(t,at,n)		((t*)emalloc(sizeof(t) + (n)*sizeof(at)))
