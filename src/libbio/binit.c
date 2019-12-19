#include	"lib9.h"
#include	<bio.h>

enum
{
	MAXBUFS	= 20
};

static	Biobuf*	wbufs[MAXBUFS];
static	int		atexitflag;

static
void
batexit(void)
{
	Biobuf *bp;
	int i;

	for(i=0; i<MAXBUFS; i++) {
		bp = wbufs[i];
		if(bp != 0) {
			wbufs[i] = 0;
			Bflush(bp);
		}
	}
}

static
void
deinstall(Biobuf *bp)
{
	int i;

	for(i=0; i<MAXBUFS; i++)
		if(wbufs[i] == bp)
			wbufs[i] = 0;
}

static
void
install(Biobuf *bp)
{
	int i;

	deinstall(bp);
	for(i=0; i<MAXBUFS; i++)
		if(wbufs[i] == 0) {
			wbufs[i] = bp;
			break;
		}
	if(atexitflag == 0) {
		atexitflag = 1;
		atexit(batexit);
	}
}

int
Binits(Biobuf *bp, int f, int mode, unsigned char *p, int size)
{

	p += Bungetsize;	/* make room for Bungets */
	size -= Bungetsize;

	switch(mode&~(OCEXEC|ORCLOSE|OTRUNC)) {
	default:
		fprint(2, "Bopen: unknown mode %d\n", mode);
		return Beof;

	case OREAD:
		bp->state = Bractive;
		bp->ocount = 0;
		break;

	case OWRITE:
		install(bp);
		bp->state = Bwactive;
		bp->ocount = -size;
		break;
	}
	bp->bbuf = p;
	bp->ebuf = p+size;
	bp->bsize = size;
	bp->icount = 0;
	bp->gbuf = bp->ebuf;
	bp->fid = f;
	bp->flag = 0;
	bp->rdline = 0;
	bp->offset = 0;
	bp->runesize = 0;
	return 0;
}


int
Binit(Biobuf *bp, int f, int mode)
{
	return Binits(bp, f, mode, bp->b, sizeof(bp->b));
}

Biobuf*
Bfdopen(int f, int mode)
{
	Biobuf *bp;

	bp = malloc(sizeof(Biobuf));
	if(bp == 0)
		return 0;
	Binits(bp, f, mode, bp->b, sizeof(bp->b));
	bp->flag = Bmagic;
	return bp;
}

Biobuf*
Bopen(char *name, int mode)
{
	Biobuf *bp;
	int f;

	switch(mode&~(OCEXEC|ORCLOSE|OTRUNC)) {
	default:
		fprint(2, "Bopen: unknown mode %d\n", mode);
		return 0;

	case OREAD:
		f = open(name, mode);
		if(f < 0)
			return 0;
		break;

	case OWRITE:
		f = create(name, mode, 0666);
		if(f < 0)
			return 0;
	}
	bp = Bfdopen(f, mode);
	if(bp == 0)
		close(f);
	return bp;
}

int
Bterm(Biobuf *bp)
{
	int ret;

	deinstall(bp);
	ret = Bflush(bp);
	if(bp->flag == Bmagic) {
		bp->flag = 0;
		if(close(bp->fid) < 0)
			ret = -1;
		free(bp);
	}
	return ret;
}
