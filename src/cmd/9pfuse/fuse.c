#include "a.h"

int fusefd;
int fuseeof;
int fusebufsize;
int fusemaxwrite;
FuseMsg *fusemsglist;
Lock fusemsglock;

int mountfuse(char *mtpt);
void unmountfuse(char *mtpt);

FuseMsg*
allocfusemsg(void)
{
	FuseMsg *m;
	void *vbuf;

	lock(&fusemsglock);
	if((m = fusemsglist) != nil){
		fusemsglist = m->next;
		unlock(&fusemsglock);
		return m;
	}
	unlock(&fusemsglock);
	m = emalloc(sizeof(*m) + fusebufsize);
	vbuf = m+1;
	m->buf = vbuf;
	m->nbuf = 0;
	m->hdr = vbuf;
	m->tx = m->hdr+1;
	return m;
}

void
freefusemsg(FuseMsg *m)
{
	lock(&fusemsglock);
	m->next = fusemsglist;
	fusemsglist = m;
	unlock(&fusemsglock);
}

FuseMsg*
readfusemsg(void)
{
	FuseMsg *m;
	int n, nn;

	m = allocfusemsg();
	/*
	 * The FUSE kernel device apparently guarantees
	 * that this read will return exactly one message.
	 * You get an error return if you ask for just the
	 * length (first 4 bytes).
	 * FUSE returns an ENODEV error, not EOF,
	 * when the connection is unmounted.
	 */
	do{
		errno = 0;
		n = read(fusefd, m->buf, fusebufsize);
	}while(n < 0 && errno == EINTR);
	if(n < 0){
		if(errno != ENODEV)
			sysfatal("readfusemsg: %r");
	}
	if(n <= 0){
		fuseeof = 1;
		freefusemsg(m);
		return nil;
	}
	m->nbuf = n;

	/*
	 * FreeBSD FUSE sends a short length in the header
	 * for FUSE_INIT even though the actual read length
	 * is correct.
	 */
	if(n == sizeof(*m->hdr)+sizeof(struct fuse_init_in)
	&& m->hdr->opcode == FUSE_INIT && m->hdr->len < n)
		m->hdr->len = n;

	if(m->hdr->len != n)
		sysfatal("readfusemsg: got %d wanted %d",
			n, m->hdr->len);
	m->hdr->len -= sizeof(*m->hdr);

	/*
	 * Paranoia.
	 * Make sure lengths are long enough.
	 * Make sure string arguments are NUL terminated.
	 * (I don't trust the kernel module.)
	 */
	switch(m->hdr->opcode){
	default:
		/*
		 * Could sysfatal here, but can also let message go
		 * and assume higher-level code will return an
		 * "I don't know what you mean" error and recover.
		 */
		break;
	case FUSE_LOOKUP:
	case FUSE_UNLINK:
	case FUSE_RMDIR:
	case FUSE_REMOVEXATTR:
		/* just a string */
		if(((char*)m->tx)[m->hdr->len-1] != 0)
		bad:
			sysfatal("readfusemsg: bad message");
		break;
	case FUSE_FORGET:
		if(m->hdr->len < sizeof(struct fuse_forget_in))
			goto bad;
		break;
	case FUSE_GETATTR:
		break;
	case FUSE_SETATTR:
		if(m->hdr->len < sizeof(struct fuse_setattr_in))
			goto bad;
		break;
	case FUSE_READLINK:
		break;
	case FUSE_SYMLINK:
		/* two strings */
		if(((char*)m->tx)[m->hdr->len-1] != 0
		|| memchr(m->tx, 0, m->hdr->len-1) == 0)
			goto bad;
		break;
	case FUSE_MKNOD:
		if(m->hdr->len <= sizeof(struct fuse_mknod_in)
		|| ((char*)m->tx)[m->hdr->len-1] != 0)
			goto bad;
		break;
	case FUSE_MKDIR:
		if(m->hdr->len <= sizeof(struct fuse_mkdir_in)
		|| ((char*)m->tx)[m->hdr->len-1] != 0)
			goto bad;
		break;
	case FUSE_RENAME:
		/* a struct and two strings */
		if(m->hdr->len <= sizeof(struct fuse_rename_in)
		|| ((char*)m->tx)[m->hdr->len-1] != 0
		|| memchr((uchar*)m->tx+sizeof(struct fuse_rename_in), 0, m->hdr->len-sizeof(struct fuse_rename_in)-1) == 0)
			goto bad;
		break;
	case FUSE_LINK:
		if(m->hdr->len <= sizeof(struct fuse_link_in)
		|| ((char*)m->tx)[m->hdr->len-1] != 0)
			goto bad;
		break;
	case FUSE_OPEN:
	case FUSE_OPENDIR:
		if(m->hdr->len < sizeof(struct fuse_open_in))
			goto bad;
		break;
	case FUSE_READ:
	case FUSE_READDIR:
		if(m->hdr->len < sizeof(struct fuse_read_in))
			goto bad;
		break;
	case FUSE_WRITE:
		/* no strings, but check that write length is sane */
		if(m->hdr->len < sizeof(struct fuse_write_in)+((struct fuse_write_in*)m->tx)->size)
			goto bad;
		break;
	case FUSE_STATFS:
		break;
	case FUSE_RELEASE:
	case FUSE_RELEASEDIR:
		if(m->hdr->len < sizeof(struct fuse_release_in))
			goto bad;
		break;
	case FUSE_FSYNC:
	case FUSE_FSYNCDIR:
		if(m->hdr->len < sizeof(struct fuse_fsync_in))
			goto bad;
		break;
	case FUSE_SETXATTR:
		/* struct, one string, and one binary blob */
		if(m->hdr->len <= sizeof(struct fuse_setxattr_in))
			goto bad;
		nn = ((struct fuse_setxattr_in*)m->tx)->size;
		if(m->hdr->len < sizeof(struct fuse_setxattr_in)+nn+1)
			goto bad;
		if(((char*)m->tx)[m->hdr->len-nn-1] != 0)
			goto bad;
		break;
	case FUSE_GETXATTR:
		/* struct and one string */
		if(m->hdr->len <= sizeof(struct fuse_getxattr_in)
		|| ((char*)m->tx)[m->hdr->len-1] != 0)
			goto bad;
		break;
	case FUSE_LISTXATTR:
		if(m->hdr->len < sizeof(struct fuse_getxattr_in))
			goto bad;
		break;
	case FUSE_FLUSH:
		if(m->hdr->len < sizeof(struct fuse_flush_in))
			goto bad;
		break;
	case FUSE_INIT:
		if(m->hdr->len < sizeof(struct fuse_init_in))
			goto bad;
		break;
	case FUSE_ACCESS:
		if(m->hdr->len < sizeof(struct fuse_access_in))
			goto bad;
		break;
	case FUSE_CREATE:
		if(m->hdr->len <= sizeof(struct fuse_open_in)
		|| ((char*)m->tx)[m->hdr->len-1] != 0)
			goto bad;
		break;
	}
	if(debug)
		fprint(2, "FUSE -> %G\n", m->hdr, m->tx);
	return m;
}

/*
 * Reply to FUSE request m using additonal
 * argument buffer arg of size narg bytes.
 * Perhaps should free the FuseMsg here?
 */
void
replyfuse(FuseMsg *m, void *arg, int narg)
{
	struct iovec vec[2];
	struct fuse_out_header hdr;
	int nvec;

	hdr.len = sizeof hdr + narg;
	hdr.error = 0;
	hdr.unique = m->hdr->unique;
	if(debug)
		fprint(2, "FUSE <- %#G\n", m->hdr, &hdr, arg);

	vec[0].iov_base = &hdr;
	vec[0].iov_len = sizeof hdr;
	nvec = 1;
	if(arg && narg){
		vec[1].iov_base = arg;
		vec[1].iov_len = narg;
		nvec++;
	}
	writev(fusefd, vec, nvec);
	freefusemsg(m);
}

/*
 * Reply to FUSE request m with errno e.
 */
void
replyfuseerrno(FuseMsg *m, int e)
{
	struct fuse_out_header hdr;

	hdr.len = sizeof hdr;
	hdr.error = -e;	/* FUSE sends negative errnos. */
	hdr.unique = m->hdr->unique;
	if(debug)
		fprint(2, "FUSE <- %#G\n", m->hdr, &hdr, 0);
	write(fusefd, &hdr, sizeof hdr);
	freefusemsg(m);
}

void
replyfuseerrstr(FuseMsg *m)
{
	replyfuseerrno(m, errstr2errno());
}

char *fusemtpt;
void
unmountatexit(void)
{
	if(fusemtpt)
		unmountfuse(fusemtpt);
}

void
initfuse(char *mtpt)
{
	FuseMsg *m;
	struct fuse_init_in *tx;
	struct fuse_init_out rx;

	fusemtpt = mtpt;

	/*
	 * The 4096 is for the message headers.
	 * It's a lot, but it's what the FUSE libraries ask for.
	 */
	fusemaxwrite = getpagesize();
	fusebufsize = 4096 + fusemaxwrite;

	if((fusefd = mountfuse(mtpt)) < 0)
		sysfatal("mountfuse: %r");

	if((m = readfusemsg()) == nil)
		sysfatal("readfusemsg: %r");
	if(m->hdr->opcode != FUSE_INIT)
		sysfatal("fuse: expected FUSE_INIT (26) got %d", m->hdr->opcode);
	tx = m->tx;

	/*
	 * Complain if the kernel is too new.
	 * We could forge ahead, but at least the one time I tried,
	 * the kernel rejected the newer version by making the
	 * writev fail in replyfuse, which is a much more confusing
	 * error message.  In the future, might be nice to try to
	 * support older versions that differ only slightly.
	 */
	if(tx->major < FUSE_KERNEL_VERSION
	|| (tx->major == FUSE_KERNEL_VERSION && tx->minor < FUSE_KERNEL_MINOR_VERSION))
		sysfatal("fuse: too kernel version %d.%d older than program version %d.%d",
			tx->major, tx->minor, FUSE_KERNEL_VERSION, FUSE_KERNEL_MINOR_VERSION);

	memset(&rx, 0, sizeof rx);
	rx.major = FUSE_KERNEL_VERSION;
	rx.minor = FUSE_KERNEL_MINOR_VERSION;
	rx.max_write = fusemaxwrite;
	replyfuse(m, &rx, sizeof rx);
}

/*
 * Print FUSE messages.  Assuming it is installed as %G,
 * use %G with hdr, arg arguments to format a request,
 * and %#G with reqhdr, hdr, arg arguments to format a response.
 * The reqhdr is necessary in the %#G form because the
 * response does not contain an opcode tag.
 */
int
fusefmt(Fmt *fmt)
{
	struct fuse_in_header *hdr = va_arg(fmt->args, void*);
	if((fmt->flags&FmtSharp) == 0){  /* "%G", hdr, arg */
		void *a = va_arg(fmt->args, void*);
		fmtprint(fmt, "len %d unique %#llux uid %d gid %d pid %d ",
			hdr->len, hdr->unique, hdr->uid, hdr->gid, hdr->pid);

		switch(hdr->opcode){
			default: {
				fmtprint(fmt, "??? opcode %d", hdr->opcode);
				break;
			}
			case FUSE_LOOKUP: {
				fmtprint(fmt, "Lookup nodeid %#llux name %#q",
					hdr->nodeid, a);
				break;
			}
			case FUSE_FORGET: {
				struct fuse_forget_in *tx = a;
				/* nlookup (a ref count) is a vlong! */
				fmtprint(fmt, "Forget nodeid %#llux nlookup %lld",
					hdr->nodeid, tx->nlookup);
				break;
			}
			case FUSE_GETATTR: {
				fmtprint(fmt, "Getattr nodeid %#llux", hdr->nodeid);
				break;
			}
			case FUSE_SETATTR: {
				struct fuse_setattr_in *tx = a;
				fmtprint(fmt, "Setattr nodeid %#llux", hdr->nodeid);
				if(tx->valid&FATTR_FH)
					fmtprint(fmt, " fh %#llux", tx->fh);
				if(tx->valid&FATTR_SIZE)
					fmtprint(fmt, " size %lld", tx->size);
				if(tx->valid&FATTR_ATIME)
					fmtprint(fmt, " atime %.20g", tx->atime+tx->atimensec*1e-9);
				if(tx->valid&FATTR_MTIME)
					fmtprint(fmt, " mtime %.20g", tx->mtime+tx->mtimensec*1e-9);
				if(tx->valid&FATTR_MODE)
					fmtprint(fmt, " mode %#uo", tx->mode);
				if(tx->valid&FATTR_UID)
					fmtprint(fmt, " uid %d", tx->uid);
				if(tx->valid&FATTR_GID)
					fmtprint(fmt, " gid %d", tx->gid);
				break;
			}
			case FUSE_READLINK: {
				fmtprint(fmt, "Readlink nodeid %#llux", hdr->nodeid);
				break;
			}
			case FUSE_SYMLINK: {
				char *old, *new;

				old = a;
				new = a + strlen(a) + 1;
				fmtprint(fmt, "Symlink nodeid %#llux old %#q new %#q",
					hdr->nodeid, old, new);
				break;
			}
			case FUSE_MKNOD: {
				struct fuse_mknod_in *tx = a;
				fmtprint(fmt, "Mknod nodeid %#llux mode %#uo rdev %#ux name %#q",
					hdr->nodeid, tx->mode, tx->rdev, tx+1);
				break;
			}
			case FUSE_MKDIR: {
				struct fuse_mkdir_in *tx = a;
				fmtprint(fmt, "Mkdir nodeid %#llux mode %#uo name %#q",
					hdr->nodeid, tx->mode, tx+1);
				break;
			}
			case FUSE_UNLINK: {
				fmtprint(fmt, "Unlink nodeid %#llux name %#q",
					hdr->nodeid, a);
				break;
			}
			case FUSE_RMDIR: {
				fmtprint(fmt, "Rmdir nodeid %#llux name %#q",
					hdr->nodeid, a);
				break;
			}
			case FUSE_RENAME: {
				struct fuse_rename_in *tx = a;
				char *old = (char*)(tx+1);
				char *new = old + strlen(old) + 1;
				fmtprint(fmt, "Rename nodeid %#llux old %#q newdir %#llux new %#q",
					hdr->nodeid, old, tx->newdir, new);
				break;
			}
			case FUSE_LINK: {
				struct fuse_link_in *tx = a;
				fmtprint(fmt, "Link oldnodeid %#llux nodeid %#llux name %#q",
					tx->oldnodeid, hdr->nodeid, tx+1);
				break;
			}
			case FUSE_OPEN: {
				struct fuse_open_in *tx = a;
				/* Should one or both of flags and mode be octal? */
				fmtprint(fmt, "Open nodeid %#llux flags %#ux mode %#ux",
					hdr->nodeid, tx->flags, tx->mode);
				break;
			}
			case FUSE_READ: {
				struct fuse_read_in *tx = a;
				fmtprint(fmt, "Read nodeid %#llux fh %#llux offset %lld size %ud",
					hdr->nodeid, tx->fh, tx->offset, tx->size);
				break;
			}
			case FUSE_WRITE: {
				struct fuse_write_in *tx = a;
				fmtprint(fmt, "Write nodeid %#llux fh %#llux offset %lld size %ud flags %#ux",
					hdr->nodeid, tx->fh, tx->offset, tx->size, tx->write_flags);
				break;
			}
			case FUSE_STATFS: {
				fmtprint(fmt, "Statfs");
				break;
			}
			case FUSE_RELEASE: {
				struct fuse_release_in *tx = a;
				fmtprint(fmt, "Release nodeid %#llux fh %#llux flags %#ux",
					hdr->nodeid, tx->fh, tx->flags);
				break;
			}
			case FUSE_FSYNC: {
				struct fuse_fsync_in *tx = a;
				fmtprint(fmt, "Fsync nodeid %#llux fh %#llux flags %#ux",
					hdr->nodeid, tx->fh, tx->fsync_flags);
				break;
			}
			case FUSE_SETXATTR: {
				struct fuse_setxattr_in *tx = a;
				char *name = (char*)(tx+1);
				char *value = name + strlen(name) + 1;
				fmtprint(fmt, "Setxattr nodeid %#llux size %d flags %#ux name %#q value %#q",
					hdr->nodeid, tx->size, tx->flags, name, value);
				break;
			}
			case FUSE_GETXATTR: {
				struct fuse_getxattr_in *tx = a;
				fmtprint(fmt, "Getxattr nodeid %#llux size %d name %#q",
					hdr->nodeid, tx->size, tx+1);
				break;
			}
			case FUSE_LISTXATTR: {
				struct fuse_getxattr_in *tx = a;
				fmtprint(fmt, "Listxattr nodeid %#llux size %d",
					hdr->nodeid, tx->size);
				break;
			}
			case FUSE_REMOVEXATTR: {
				fmtprint(fmt, "Removexattr nodeid %#llux name %#q",
					hdr->nodeid, a);
				break;
			}
			case FUSE_FLUSH: {
				struct fuse_flush_in *tx = a;
				fmtprint(fmt, "Flush nodeid %#llux fh %#llux flags %#ux",
					hdr->nodeid, tx->fh, tx->flush_flags);
				break;
			}
			case FUSE_INIT: {
				struct fuse_init_in *tx = a;
				fmtprint(fmt, "Init major %d minor %d",
					tx->major, tx->minor);
				break;
			}
			case FUSE_OPENDIR: {
				struct fuse_open_in *tx = a;
				fmtprint(fmt, "Opendir nodeid %#llux flags %#ux mode %#ux",
					hdr->nodeid, tx->flags, tx->mode);
				break;
			}
			case FUSE_READDIR: {
				struct fuse_read_in *tx = a;
				fmtprint(fmt, "Readdir nodeid %#llux fh %#llux offset %lld size %ud",
					hdr->nodeid, tx->fh, tx->offset, tx->size);
				break;
			}
			case FUSE_RELEASEDIR: {
				struct fuse_release_in *tx = a;
				fmtprint(fmt, "Releasedir nodeid %#llux fh %#llux flags %#ux",
					hdr->nodeid, tx->fh, tx->flags);
				break;
			}
			case FUSE_FSYNCDIR: {
				struct fuse_fsync_in *tx = a;
				fmtprint(fmt, "Fsyncdir nodeid %#llux fh %#llux flags %#ux",
					hdr->nodeid, tx->fh, tx->fsync_flags);
				break;
			}
			case FUSE_ACCESS: {
				struct fuse_access_in *tx  = a;
				fmtprint(fmt, "Access nodeid %#llux mask %#ux",
					hdr->nodeid, tx->mask);
				break;
			}
			case FUSE_CREATE: {
				struct fuse_open_in *tx = a;
				fmtprint(fmt, "Create nodeid %#llx flags %#ux mode %#ux name %#q",
					hdr->nodeid, tx->flags, tx->mode, tx+1);
				break;
			}
		}
	}else{  /* "%#G", reqhdr, hdr, arg - use reqhdr only for type */
		struct fuse_out_header *ohdr = va_arg(fmt->args, void*);
		void *a = va_arg(fmt->args, void*);
		int len = ohdr->len - sizeof *ohdr;
		fmtprint(fmt, "unique %#llux ", ohdr->unique);
		if(ohdr->error){
			fmtprint(fmt, "error %d %s", ohdr->error, strerror(-ohdr->error));
		}else
		switch(hdr->opcode){
			default: {
				fmtprint(fmt, "??? opcode %d", hdr->opcode);
				break;
			}
			case FUSE_LOOKUP: {
				/*
				 * For a negative entry, can send back ENOENT
				 * or rx->ino == 0.
				 * In protocol version 7.4 and before, can only use
				 * the ENOENT method.
				 * Presumably the benefit of sending rx->ino == 0
				 * is that you can specify the length of time to cache
				 * the negative result.
				 */
				struct fuse_entry_out *rx;
				fmtprint(fmt, "(Lookup) ");
			fmt_entry_out:
				rx = a;
				fmtprint(fmt, "nodeid %#llux gen %#llux entry_valid %.20g attr_valid %.20g ",
					rx->nodeid, rx->generation,
					rx->entry_valid+rx->entry_valid_nsec*1e-9,
					rx->attr_valid+rx->attr_valid_nsec*1e-9);
				fmtprint(fmt, " ino %#llux size %lld blocks %lld atime %.20g mtime %.20g ctime %.20g mode %#uo nlink %d uid %d gid %d rdev %#ux",
					rx->attr.ino, rx->attr.size, rx->attr.blocks,
					rx->attr.atime+rx->attr.atimensec*1e-9,
					rx->attr.mtime+rx->attr.mtimensec*1e-9,
					rx->attr.ctime+rx->attr.ctimensec*1e-9,
					rx->attr.mode, rx->attr.nlink, rx->attr.uid,
					rx->attr.gid, rx->attr.rdev);
				break;
			}
			case FUSE_FORGET: {
				/* Can't happen! No reply. */
				fmtprint(fmt, "(Forget) can't happen");
				break;
			}
			case FUSE_GETATTR: {
				struct fuse_attr_out *rx;
				fmtprint(fmt, "(Getattr) ");
			fmt_attr_out:
				rx = a;
				fmtprint(fmt, "attr_valid %.20g",
					rx->attr_valid+rx->attr_valid_nsec*1e-9);
				fmtprint(fmt, " ino %#llux size %lld blocks %lld atime %.20g mtime %.20g ctime %.20g mode %#uo nlink %d uid %d gid %d rdev %#ux",
					rx->attr.ino, rx->attr.size, rx->attr.blocks,
					rx->attr.atime+rx->attr.atimensec*1e-9,
					rx->attr.mtime+rx->attr.mtimensec*1e-9,
					rx->attr.ctime+rx->attr.ctimensec*1e-9,
					rx->attr.mode, rx->attr.nlink, rx->attr.uid,
					rx->attr.gid, rx->attr.rdev);
				break;
			}
			case FUSE_SETATTR: {
				fmtprint(fmt, "(Setattr) ");
				goto fmt_attr_out;
				break;
			}
			case FUSE_READLINK: {
				fmtprint(fmt, "(Readlink) %#.*q",
					utfnlen(a, len), a);
				break;
			}
			case FUSE_SYMLINK: {
				fmtprint(fmt, "(Symlink) ");
				goto fmt_entry_out;
				break;
			}
			case FUSE_MKNOD: {
				fmtprint(fmt, "(Mknod) ");
				goto fmt_entry_out;
				break;
			}
			case FUSE_MKDIR: {
				fmtprint(fmt, "(Mkdir) ");
				goto fmt_entry_out;
				break;
			}
			case FUSE_UNLINK: {
				fmtprint(fmt, "(Unlink)");
				break;
			}
			case FUSE_RMDIR: {
				fmtprint(fmt, "(Rmdir)");
				break;
			}
			case FUSE_RENAME: {
				fmtprint(fmt, "(Rename)");
				break;
			}
			case FUSE_LINK: {
				fmtprint(fmt, "(Link) ");
				goto fmt_entry_out;
				break;
			}
			case FUSE_OPEN: {
				struct fuse_open_out *rx;
				fmtprint(fmt, "(Open) ");
			fmt_open_out:
				rx = a;
				fmtprint(fmt, "fh %#llux flags %#ux", rx->fh, rx->open_flags);
				break;
			}
			case FUSE_READ: {
				fmtprint(fmt, "(Read) size %d", len);
				break;
			}
			case FUSE_WRITE: {
				struct fuse_write_out *rx = a;
				fmtprint(fmt, "(Write) size %d", rx->size);
				break;
			}
			case FUSE_STATFS: {
				/*
				 * Before protocol version 7.4, only first 48 bytes are used.
				 */
				struct fuse_statfs_out *rx = a;
				fmtprint(fmt, "(Statfs) blocks %lld bfree %lld bavail %lld files %lld ffree %lld bsize %ud namelen %ud frsize %ud",
					rx->st.blocks, rx->st.bfree, rx->st.bavail,
					rx->st.files, rx->st.ffree, rx->st.bsize,
					rx->st.namelen, rx->st.frsize);
				break;
			}
			case FUSE_RELEASE: {
				fmtprint(fmt, "(Release)");
				break;
			}
			case FUSE_FSYNC: {
				fmtprint(fmt, "(Fsync)");
				break;
			}
			case FUSE_SETXATTR: {
				fmtprint(fmt, "(Setxattr)");
				break;
			}
			case FUSE_GETXATTR: {
				fmtprint(fmt, "(Getxattr) size %d", len);
				break;
			}
			case FUSE_LISTXATTR: {
				fmtprint(fmt, "(Lisrxattr) size %d", len);
				break;
			}
			case FUSE_REMOVEXATTR: {
				fmtprint(fmt, "(Removexattr)");
				break;
			}
			case FUSE_FLUSH: {
				fmtprint(fmt, "(Flush)");
				break;
			}
			case FUSE_INIT: {
				struct fuse_init_out *rx = a;
				fmtprint(fmt, "(Init) major %d minor %d max_write %d",
					rx->major, rx->minor, rx->max_write);
				break;
			}
			case FUSE_OPENDIR: {
				fmtprint(fmt, "(Opendir) ");
				goto fmt_open_out;
				break;
			}
			case FUSE_READDIR: {
				fmtprint(fmt, "(Readdir) size %d", len);
				break;
			}
			case FUSE_RELEASEDIR: {
				fmtprint(fmt, "(Releasedir)");
				break;
			}
			case FUSE_FSYNCDIR: {
				fmtprint(fmt, "(Fsyncdir)");
				break;
			}
			case FUSE_ACCESS: {
				fmtprint(fmt, "(Access)");
				break;
			}
			case FUSE_CREATE: {
				struct fuse_create_out *rx = a;
				fmtprint(fmt, "(Create) ");
				fmtprint(fmt, "nodeid %#llux gen %#llux entry_valid %.20g attr_valid %.20g ",
					rx->e.nodeid, rx->e.generation,
					rx->e.entry_valid+rx->e.entry_valid_nsec*1e-9,
					rx->e.attr_valid+rx->e.attr_valid_nsec*1e-9);
				fmtprint(fmt, " ino %#llux size %lld blocks %lld atime %.20g mtime %.20g ctime %.20g mode %#uo nlink %d uid %d gid %d rdev %#ux",
					rx->e.attr.ino, rx->e.attr.size, rx->e.attr.blocks,
					rx->e.attr.atime+rx->e.attr.atimensec*1e-9,
					rx->e.attr.mtime+rx->e.attr.mtimensec*1e-9,
					rx->e.attr.ctime+rx->e.attr.ctimensec*1e-9,
					rx->e.attr.mode, rx->e.attr.nlink, rx->e.attr.uid,
					rx->e.attr.gid, rx->e.attr.rdev);
				fmtprint(fmt, " fh %#llux flags %#ux", rx->o.fh, rx->o.open_flags);
				break;
			}
		}
	}
	return 0;
}

#if defined(__APPLE__)
#include <sys/param.h>
#include <sys/mount.h>
#endif

/*
 * Mounts a fuse file system on mtpt and returns
 * a file descriptor for the corresponding fuse
 * message conversation.
 */
int
mountfuse(char *mtpt)
{
#if defined(__linux__)
	int p[2], pid, fd;
	char buf[20];

	if(socketpair(AF_UNIX, SOCK_STREAM, 0, p) < 0)
		return -1;
	pid = fork();
	if(pid < 0)
		return -1;
	if(pid == 0){
		close(p[1]);
		snprint(buf, sizeof buf, "%d", p[0]);
		putenv("_FUSE_COMMFD", buf);
		execlp("fusermount", "fusermount", "--", mtpt, nil);
		fprint(2, "exec fusermount: %r\n");
		_exit(1);
	}
	close(p[0]);
	fd = recvfd(p[1]);
	close(p[1]);
	return fd;
#elif defined(__FreeBSD__) && !defined(__APPLE__)
	int pid, fd;
	char buf[20];

	if((fd = open("/dev/fuse", ORDWR)) < 0)
		return -1;
	snprint(buf, sizeof buf, "%d", fd);

	pid = fork();
	if(pid < 0)
		return -1;
	if(pid == 0){
		execlp("mount_fusefs", "mount_fusefs", buf, mtpt, nil);
		fprint(2, "exec mount_fusefs: %r\n");
		_exit(1);
	}
	return fd;
#elif defined(__APPLE__)
	int i, pid, fd, r, p[2];
	char buf[20];
	struct vfsconf vfs;
	char *f, *v;

	if(getvfsbyname(v="osxfusefs", &vfs) < 0 &&
	   getvfsbyname(v="macfuse", &vfs) < 0 &&
	   getvfsbyname(v="osxfuse", &vfs) < 0 &&
	   getvfsbyname(v="fusefs", &vfs) < 0){
		if(access((v="osxfusefs", f="/Library/Filesystems/osxfusefs.fs"
			"/Support/load_osxfusefs"), 0) < 0 &&
		   access((v="macfuse", f="/Library/Filesystems/macfuse.fs"
			"/Contents/Resources/load_macfuse"), 0) < 0 &&
		   access((v="osxfuse", f="/Library/Filesystems/osxfuse.fs"
			"/Contents/Resources/load_osxfuse"), 0) < 0 &&
		   access((v="osxfuse", f="/opt/local/Library/Filesystems/osxfuse.fs"
			"/Contents/Resources/load_osxfuse"), 0) < 0 &&
		   access((v="fusefs", f="/System/Library/Extensions/fusefs.kext"
			"/Contents/Resources/load_fusefs"), 0) < 0 &&
		   access(f="/Library/Extensions/fusefs.kext"
		   	"/Contents/Resources/load_fusefs", 0) < 0 &&
		   access(f="/Library/Filesystems"
				  "/fusefs.fs/Support/load_fusefs", 0) < 0 &&
		   access(f="/System/Library/Filesystems"
		         "/fusefs.fs/Support/load_fusefs", 0) < 0){
		   	werrstr("cannot find load_fusefs");
		   	return -1;
		}
		if((r=system(f)) < 0){
			werrstr("%s: %r", f);
			return -1;
		}
		if(r != 0){
			werrstr("load_fusefs failed: exit %d", r);
			return -1;
		}
		if(getvfsbyname(v, &vfs) < 0){
			werrstr("getvfsbyname %s: %r", v);
			return -1;
		}
	}

	/* MacFUSE >=4 dropped support for passing fd */
	if (strcmp(v, "macfuse") == 0) {
		if(socketpair(AF_UNIX, SOCK_STREAM, 0, p) < 0)
			return -1;
		pid = fork();
		if(pid < 0)
			return -1;
		if(pid == 0){
			close(p[1]);
			snprint(buf, sizeof buf, "%d", p[0]);
			putenv("_FUSE_COMMFD", buf);
			putenv("_FUSE_COMMVERS", "2");
			putenv("_FUSE_CALL_BY_LIB", "1");
			putenv("_FUSE_DAEMON_PATH",
				"/Library/Filesystems/macfuse.fs/Contents/Resources/mount_macfus");
			execl("/Library/Filesystems/macfuse.fs/Contents/Resources/mount_macfuse",
				"mount_macfuse", mtpt, nil);
			fprint(2, "exec mount_macfuse: %r\n");
			_exit(1);
		}
		close(p[0]);
		fd = recvfd(p[1]);
		close(p[1]);
		return fd;
	}

	/* Look for available FUSE device. */
	/*
	 * We need to truncate `fs` from the end of the vfs name if
	 * it's present
	 */
	int len;
	if (strcmp(v, "osxfuse") == 0) {
		len = strlen(v);
	} else {
		len = strlen(v)-2;
	}
	for(i=0;; i++){
		snprint(buf, sizeof buf, "/dev/%.*s%d", len, v, i);
		if(access(buf, 0) < 0){
			werrstr("no available fuse devices");
			return -1;
		}
		if((fd = open(buf, ORDWR)) >= 0)
			break;
	}

	pid = fork();
	if(pid < 0)
		return -1;
	if(pid == 0){
		snprint(buf, sizeof buf, "%d", fd);
		/* OSXFUSE >=3.3 changed the name of the environment variable, set both */
		putenv("MOUNT_FUSEFS_CALL_BY_LIB", "");
		putenv("MOUNT_OSXFUSE_CALL_BY_LIB", "");
		/*
		 * Different versions of OSXFUSE and MacFUSE put the
		 * mount_fusefs binary in different places.  Try all.
		 */
		/*  OSXFUSE >=3.3  greater location */
		putenv("MOUNT_OSXFUSE_DAEMON_PATH",
			   "/Library/Filesystems/osxfuse.fs/Contents/Resources/mount_osxfuse");
		execl("/Library/Filesystems/osxfuse.fs/Contents/Resources/mount_osxfuse",
			  "mount_osxfuse", buf, mtpt, nil);

		/* OSXFUSE >=3.3 from macports */
		putenv("MOUNT_OSXFUSE_DAEMON_PATH",
			"/opt/local/Library/Filesystems/osxfuse.fs/Contents/Resources/mount_osxfuse");
		execl("/opt/local/Library/Filesystems/osxfuse.fs/Contents/Resources/mount_osxfuse",
			"mount_osxfuse", buf, mtpt, nil);

		/* Lion OSXFUSE location */
		putenv("MOUNT_FUSEFS_DAEMON_PATH",
			   "/Library/Filesystems/osxfusefs.fs/Support/mount_osxfusefs");
		execl("/Library/Filesystems/osxfusefs.fs/Support/mount_osxfusefs",
			  "mount_osxfusefs", buf, mtpt, nil);

		/* Leopard location */
		putenv("MOUNT_FUSEFS_DAEMON_PATH",
			   "/Library/Filesystems/fusefs.fs/Support/mount_fusefs");
		execl("/Library/Filesystems/fusefs.fs/Support/mount_fusefs",
			  "mount_fusefs", buf, mtpt, nil);

		/* possible Tiger locations */
		execl("/System/Library/Filesystems/fusefs.fs/mount_fusefs",
			"mount_fusefs", buf, mtpt, nil);
		execl("/System/Library/Filesystems/fusefs.fs/Support/mount_fusefs",
			"mount_fusefs", buf, mtpt, nil);
		fprint(2, "exec mount_fusefs: %r\n");
		_exit(1);
	}
	return fd;

#else
	werrstr("cannot mount fuse on this system");
	return -1;
#endif
}

void
waitfuse(void)
{
	waitpid();
}

void
unmountfuse(char *mtpt)
{
	int pid;

	pid = fork();
	if(pid < 0)
		return;
	if(pid == 0){
#if defined(__linux__)
		execlp("fusermount", "fusermount", "-u", "-z", "--", mtpt, nil);
		fprint(2, "exec fusermount -u: %r\n");
#else
		execlp("umount", "umount", mtpt, nil);
		fprint(2, "exec umount: %r\n");
#endif
		_exit(1);
	}
	waitpid();
}
