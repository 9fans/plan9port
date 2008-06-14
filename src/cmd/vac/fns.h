int	mbunpack(MetaBlock *mb, uchar *p, int n);
void	mbinsert(MetaBlock *mb, int i, MetaEntry*);
void	mbdelete(MetaBlock *mb, int i, MetaEntry*);
void	mbpack(MetaBlock *mb);
uchar	*mballoc(MetaBlock *mb, int n);

int	meunpack(MetaEntry*, MetaBlock *mb, int i);
int	mecmp(MetaEntry*, char *s);
int	mecmpnew(MetaEntry*, char *s);

enum {
	VacDirVersion = 8,
	FossilDirVersion = 9,
};
int	vdsize(VacDir *dir, int);
int	vdunpack(VacDir *dir, MetaEntry*);
void	vdpack(VacDir *dir, MetaEntry*, int);

VacFile *_vacfileroot(VacFs *fs, VtFile *file);

int	_vacfsnextqid(VacFs *fs, uvlong *qid);
