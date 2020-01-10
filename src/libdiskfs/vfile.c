#include <u.h>
#include <libc.h>
#include <diskfs.h>
#include <venti.h>

extern void vtLibThread(void);

typedef struct DiskVenti DiskVenti;
struct DiskVenti
{
	TvCache *c;
	Entry e;
};

Disk*
diskOpenVenti(TvCache *c, uchar score[VtScoreSize])
{
	vtLibThread();

	fetch vtroot
	fetch dir block
	copy e
}

Block*
diskVentiRead(Disk *dd, u32int len, u64int offset)
{
	DiskVenti *d = (DiskVenti*)dd;

	make offset list
	walk down blocks
	return the one
}
