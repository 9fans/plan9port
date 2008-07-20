typedef	struct Ioclust	Ioclust;
typedef	struct Iobuf	Iobuf;
typedef	struct Isofile	Isofile;
typedef struct Xdata	Xdata;
typedef struct Xfile	Xfile;
typedef struct Xfs	Xfs;
typedef struct Xfsub	Xfsub;

#pragma incomplete Isofile

enum
{
	Sectorsize = 2048,
	Maxname = 256,
};

struct Iobuf
{
	Ioclust* clust;
	long	addr;
	uchar*	iobuf;
};

struct Ioclust
{
	long	addr;			/* in sectors; good to 8TB */
	Xdata*	dev;
	Ioclust* next;
	Ioclust* prev;
	int	busy;
	int	nbuf;
	Iobuf*	buf;
	uchar*	iobuf;
};

struct Xdata
{
	Xdata*	next;
	char*	name;		/* of underlying file */
	Qid	qid;
	short	type;
	short	fdev;
	int	ref;		/* attach count */
	int	dev;		/* for read/write */
};

struct Xfsub
{
	void	(*reset)(void);
	int	(*attach)(Xfile*);
	void	(*clone)(Xfile*, Xfile*);
	void	(*walkup)(Xfile*);
	void	(*walk)(Xfile*, char*);
	void	(*open)(Xfile*, int);
	void	(*create)(Xfile*, char*, long, int);
	long	(*readdir)(Xfile*, uchar*, long, long);
	long	(*read)(Xfile*, char*, vlong, long);
	long	(*write)(Xfile*, char*, vlong, long);
	void	(*clunk)(Xfile*);
	void	(*remove)(Xfile*);
	void	(*stat)(Xfile*, Dir*);
	void	(*wstat)(Xfile*, Dir*);
};

struct Xfs
{
	Xdata*	d;		/* how to get the bits */
	Xfsub*	s;		/* how to use them */
	int	ref;
	int	issusp;	/* follows system use sharing protocol */
	long	suspoff;	/* if so, offset at which SUSP area begins */
	int	isrock;	/* Rock Ridge format */
	int	isplan9;	/* has Plan 9-specific directory info */
	Qid	rootqid;
	Isofile*	ptr;		/* private data */
};

struct Xfile
{
	Xfile*	next;		/* in fid hash bucket */
	Xfs*	xf;
	long	fid;
	ulong	flags;
	Qid	qid;
	int	len;		/* of private data */
	Isofile*	ptr;
};

enum
{
	Asis,
	Clean,
	Clunk
};

enum
{
	Oread = 1,
	Owrite = 2,
	Orclose = 4,
	Omodes = 3,
};

extern char	Enonexist[];	/* file does not exist */
extern char	Eperm[];	/* permission denied */
extern char	Enofile[];	/* no file system specified */
extern char	Eauth[];	/* authentication failed */

extern char	*srvname;
extern char	*deffile;
extern int	chatty;
extern jmp_buf	err_lab[];
extern int	nerr_lab;
extern char	err_msg[];

extern int nojoliet;
extern int noplan9;
extern int norock;
