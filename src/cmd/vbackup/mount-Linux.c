#include <u.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < 0x020600
#	define __KERNEL__
#	include <linux/nfs.h>
#	undef __KERNEL__
#else
#	include <linux/nfs.h>
#endif
#include <linux/nfs2.h>
#include <linux/nfs_mount.h>
#include <libc.h>
#include "mountnfs.h"

void
mountnfs(int proto, struct sockaddr_in *sa, uchar *handle, int nhandle, char *mtpt)
{
	int mflag, fd;
	struct nfs_mount_data nfs;

	fd = socket(AF_INET, proto, proto==SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP);
	if(fd < 0)
		sysfatal("socket: %r");

	memset(&nfs, 0, sizeof nfs);
	nfs.version = NFS_MOUNT_VERSION;
	nfs.fd = fd;
	nfs.flags =
		NFS_MOUNT_SOFT|
		NFS_MOUNT_INTR|
		NFS_MOUNT_NOAC|
		NFS_MOUNT_NOCTO|
		NFS_MOUNT_VER3|
		NFS_MOUNT_NONLM;
	if(proto==SOCK_STREAM)
		nfs.flags |= NFS_MOUNT_TCP;
	nfs.rsize = 8192;
	nfs.wsize = 8192;
	nfs.timeo = 120;
	nfs.retrans = 2;
	nfs.acregmin = 60;
	nfs.acregmax = 600;
	nfs.acdirmin = 60;
	nfs.acdirmax = 600;
	nfs.addr = *sa;
	strcpy(nfs.hostname, "backup");
	nfs.namlen = 1024;
	nfs.bsize = 8192;
	memcpy(nfs.root.data, handle, nhandle);
	nfs.root.size = nhandle;
	mflag = MS_NOATIME|MS_NODEV|MS_NODIRATIME|
		MS_NOEXEC|MS_NOSUID|MS_RDONLY;

	if(mount("backup:/", mtpt, "nfs", mflag, &nfs) < 0)
		sysfatal("mount: %r");
}
