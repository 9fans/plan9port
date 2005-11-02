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
#if defined(__FreeBSD_version) && __FreeBSD_version >= 500000
#	include <nfsclient/nfs.h>
#else
#	include <nfs/nfs.h>
#endif
#ifdef __NetBSD__
#	include <nfs/nfsmount.h>
#endif
#include <libc.h>
#include "mountnfs.h"
#ifndef MNT_NOATIME
#	define MNT_NOATIME 0
#endif

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
	na.timeo = 200;
	na.retrans = NFS_RETRANS;
	na.maxgrouplist = NFS_MAXGRPS;
	na.hostname = "backup";
#ifndef __NetBSD__
	na.acregmin = 60;
	na.acregmax = 600;
	na.acdirmin = 60;
	na.acdirmax = 600;
#endif
	mflag = MNT_RDONLY|MNT_NOSUID|MNT_NOATIME|MNT_NODEV;
	if(mount("nfs", mtpt, mflag, &na) < 0)
		sysfatal("mount: %r");
}
