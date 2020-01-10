void		fsgetroot(Nfs3Handle*);
Nfs3Status	fsgetattr(SunAuthUnix*, Nfs3Handle*, Nfs3Attr*);
Nfs3Status	fslookup(SunAuthUnix*, Nfs3Handle*, char*, Nfs3Handle*);
Nfs3Status	fsaccess(SunAuthUnix*, Nfs3Handle*, u32int, u32int*, Nfs3Attr*);
Nfs3Status	fsreadlink(SunAuthUnix*, Nfs3Handle*, char**);
Nfs3Status	fsreadfile(SunAuthUnix*, Nfs3Handle*, u32int, u64int, uchar**, u32int*, u1int*);
Nfs3Status	fsreaddir(SunAuthUnix*, Nfs3Handle*, u32int, u64int, uchar**, u32int*, u1int*);

extern void nfs3proc(void*);
extern void mount3proc(void*);

extern int insecure;

enum
{
	MaxDataSize = 8192
};
