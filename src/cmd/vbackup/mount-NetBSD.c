#include <u.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/syslog.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <libc.h>
#include "mountnfs.h"

void
mountnfs(int proto, struct sockaddr_in *sa,
	uchar *handle, int nhandle, char *mtpt)
{
	int mflag;
	struct nfs_args na;

	memset(&na, 0, sizeof na);
	na.version = NFS_ARGSVERSION;
	na.addr = (struct sockaddr*)sa;
	na.addrlen = sizeof *sa;
	na.sotype = proto;
	na.proto = (proto == SOCK_STREAM) ? IPPROTO_TCP : IPPROTO_UDP;
	na.fh = handle;
	na.fhsize = nhandle;
	na.flags = NFSMNT_RESVPORT|NFSMNT_NFSV3|NFSMNT_INT;
	na.wsize = NFS_WSIZE;
	na.rsize = NFS_RSIZE;
	na.readdirsize = NFS_READDIRSIZE;
	na.timeo = 2;
	na.retrans = NFS_RETRANS;
	na.maxgrouplist = NFS_MAXGRPS;
	na.readahead = 0;
	na.leaseterm = 0;
	na.deadthresh = 0;
	na.hostname = "backup";
	na.acregmin = 60;
	na.acregmax = 600;
	na.acdirmin = 60;
	na.acdirmax = 600;

	mflag = MNT_RDONLY|MNT_NOSUID|MNT_NOATIME|MNT_NODEV;
	if(mount("nfs", mtpt, mflag, &na) < 0)
		sysfatal("mount: %r");
}
