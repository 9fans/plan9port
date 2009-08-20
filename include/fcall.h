#ifndef _FCALL_H_
#define _FCALL_H_ 1
#ifdef __cplusplus
extern "C" {
#endif
/*
#pragma	src	"/sys/src/libc/9sys"
#pragma	lib	"libc.a"
*/

#define	VERSION9P	"9P2000"
#define	MAXWELEM	16

typedef
struct	Fcall
{
	uchar	type;
	u32int	fid;
	ushort	tag;
	u32int	msize;		/* Tversion, Rversion */
	char	*version;	/* Tversion, Rversion */
	ushort	oldtag;		/* Tflush */
	char	*ename;		/* Rerror */
	Qid	qid;		/* Rattach, Ropen, Rcreate */
	u32int	iounit;		/* Ropen, Rcreate */
	Qid	aqid;		/* Rauth */
	u32int	afid;		/* Tauth, Tattach */
	char	*uname;		/* Tauth, Tattach */
	char	*aname;		/* Tauth, Tattach */
	u32int	perm;		/* Tcreate */ 
	char	*name;		/* Tcreate */
	uchar	mode;		/* Tcreate, Topen */
	u32int	newfid;		/* Twalk */
	ushort	nwname;		/* Twalk */
	char	*wname[MAXWELEM];	/* Twalk */
	ushort	nwqid;		/* Rwalk */
	Qid	wqid[MAXWELEM];	/* Rwalk */
	vlong	offset;		/* Tread, Twrite */
	u32int	count;		/* Tread, Twrite, Rread */
	char	*data;		/* Twrite, Rread */
	ushort	nstat;		/* Twstat, Rstat */
	uchar	*stat;		/* Twstat, Rstat */
	int	unixfd;		/* Ropenfd */
	
	/* 9P2000.u extensions */
	int	errornum;	/* Rerror */
	int	uidnum;		/* Tattach, Tauth */
	char	*extension;	/* Tcreate */
} Fcall;


#define	GBIT8(p)	((p)[0])
#define	GBIT16(p)	((p)[0]|((p)[1]<<8))
#define	GBIT32(p)	((u32int)((p)[0]|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24)))
#define	GBIT64(p)	((u32int)((p)[0]|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24)) |\
				((vlong)((p)[4]|((p)[5]<<8)|((p)[6]<<16)|((p)[7]<<24)) << 32))

#define	PBIT8(p,v)	(p)[0]=(v)
#define	PBIT16(p,v)	(p)[0]=(v);(p)[1]=(v)>>8
#define	PBIT32(p,v)	(p)[0]=(v);(p)[1]=(v)>>8;(p)[2]=(v)>>16;(p)[3]=(v)>>24
#define	PBIT64(p,v)	(p)[0]=(v);(p)[1]=(v)>>8;(p)[2]=(v)>>16;(p)[3]=(v)>>24;\
			(p)[4]=(v)>>32;(p)[5]=(v)>>40;(p)[6]=(v)>>48;(p)[7]=(v)>>56

#define	BIT8SZ		1
#define	BIT16SZ		2
#define	BIT32SZ		4
#define	BIT64SZ		8
#define	QIDSZ	(BIT8SZ+BIT32SZ+BIT64SZ)

/* STATFIXLEN includes leading 16-bit count */
/* The count, however, excludes itself; total size is BIT16SZ+count */
#define STATFIXLEN	(BIT16SZ+QIDSZ+5*BIT16SZ+4*BIT32SZ+1*BIT64SZ)	/* amount of fixed length data in a stat buffer */
#define STATFIXLENU	(STATFIXLEN+BIT16SZ+3*BIT32SZ)	/* for 9P2000.u */

#define	NOTAG		(ushort)~0U	/* Dummy tag */
#define	NOFID		(u32int)~0U	/* Dummy fid */
#define	NOUID		(-1)	/* Dummy uid */
#define	IOHDRSZ		24	/* ample room for Twrite/Rread header (iounit) */

enum
{
	Tversion =	100,
	Rversion,
	Tauth =		102,
	Rauth,
	Tattach =	104,
	Rattach,
	Terror =	106,	/* illegal */
	Rerror,
	Tflush =	108,
	Rflush,
	Twalk =		110,
	Rwalk,
	Topen =		112,
	Ropen,
	Tcreate =	114,
	Rcreate,
	Tread =		116,
	Rread,
	Twrite =	118,
	Rwrite,
	Tclunk =	120,
	Rclunk,
	Tremove =	122,
	Rremove,
	Tstat =		124,
	Rstat,
	Twstat =	126,
	Rwstat,
	Tmax,

	Topenfd = 	98,
	Ropenfd
};

uint	convM2S(uchar*, uint, Fcall*);
uint	convS2M(Fcall*, uchar*, uint);
uint	sizeS2M(Fcall*);

int	statcheck(uchar *abuf, uint nbuf);
uint	convM2D(uchar*, uint, Dir*, char*);
uint	convD2M(Dir*, uchar*, uint);
uint	sizeD2M(Dir*);

uint	convM2Su(uchar*, uint, Fcall*, int);
uint	convS2Mu(Fcall*, uchar*, uint, int);
uint	sizeS2Mu(Fcall*, int);

int	statchecku(uchar *abuf, uint nbuf, int);
uint	convM2Du(uchar*, uint, Dir*, char*, int);
uint	convD2Mu(Dir*, uchar*, uint, int);
uint	sizeD2Mu(Dir*, int);

int	fcallfmt(Fmt*);
int	dirfmt(Fmt*);
int	dirmodefmt(Fmt*);

int	read9pmsg(int, void*, uint);

/*
#pragma	varargck	type	"F"	Fcall*
#pragma	varargck	type	"M"	ulong
#pragma	varargck	type	"D"	Dir*
*/

#ifdef __cplusplus
}
#endif
#endif
