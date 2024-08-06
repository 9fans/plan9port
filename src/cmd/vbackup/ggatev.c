#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/bio.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syslog.h>

#include <geom/gate/g_gate.h>
#include "/usr/src/sbin/ggate/shared/ggate.h"

#include <u.h>
#include <libc.h>
#include <venti.h>
#include <diskfs.h>
#include <thread.h>

static enum { UNSET, CREATE, DESTROY, LIST, TEST } action = UNSET;

char *argscore;
char *pref;
uchar score[VtScoreSize];
Disk *disk;
Fsys *fsys;
VtCache *c;
VtConn *z;
Block *b;

static int unit = G_GATE_UNIT_AUTO;
static unsigned flags = 0;
static int force = 0;
static unsigned sectorsize = 0;
static unsigned timeout = G_GATE_TIMEOUT;

static void
usage(void)
{

	fprintf(stderr, "usage: %s create [-v] [-o <ro|wo|rw>] "
	    "[-s sectorsize] [-t timeout] [-u unit] <score>\n", getprogname());
	fprintf(stderr, "       %s rescue [-v] [-o <ro|wo|rw>] <-u unit> "
	    "<score>\n", getprogname());
	fprintf(stderr, "       %s destroy [-f] <-u unit>\n", getprogname());
	fprintf(stderr, "       %s list [-v] [-u unit]\n", getprogname());
	fprintf(stderr, "       %s test [-v] [-u unit]\n", getprogname());
	exit(EXIT_FAILURE);
}

static int
g_gate_openflags(unsigned ggflags)
{

	if ((ggflags & G_GATE_FLAG_READONLY) != 0)
		return (O_RDONLY);
	else if ((ggflags & G_GATE_FLAG_WRITEONLY) != 0)
		return (O_WRONLY);
	return (O_RDWR);
}

static void rventi( int blkno , char * buff, long int len, int off) {
	assert((off&0xfff) == 0);
	while( len > 0 ) {
		if((b = fsysreadblock(fsys, blkno)) != nil){
			memcpy(buff, b->data+off, fsys->blocksize-off);
			blockput(b);
		} else bzero(buff, fsys->blocksize-off);
	len -= fsys->blocksize-off;
	buff += fsys->blocksize-off;
	off = 0;
	blkno++;
	}
}

static void
g_gatev_serve()
{
	struct g_gate_ctl_io ggio;
	size_t bsize;
	long int blkno;

	if (g_gate_verbose == 0) {
		if (daemon(0, 0) == -1) {
			g_gate_destroy(unit, 1);
			err(EXIT_FAILURE, "Cannot daemonize");
		}
	}

	g_gate_log(LOG_DEBUG, "Worker created: %u.", getpid());
	ggio.gctl_version = G_GATE_VERSION;
	ggio.gctl_unit = unit;
	bsize = fsys->blocksize;
	ggio.gctl_data = malloc(bsize);
	for (;;) {
		int error;
once_again:
		ggio.gctl_length = bsize;
		ggio.gctl_error = 0;
		g_gate_ioctl(G_GATE_CMD_START, &ggio);
		error = ggio.gctl_error;
		switch (error) {
		case 0:
			break;
		case ECANCELED:
			/* Exit gracefully. */
			free(ggio.gctl_data);
			g_gate_close_device();
//			close(fd);
			exit(EXIT_SUCCESS);
		case ENOMEM:
			/* Buffer too small. */
			assert(ggio.gctl_cmd == BIO_DELETE ||
			    ggio.gctl_cmd == BIO_WRITE);
			ggio.gctl_data = realloc(ggio.gctl_data,
			    ggio.gctl_length);
			if (ggio.gctl_data != NULL) {
				bsize = ggio.gctl_length;
				goto once_again;
			}
			/* FALLTHROUGH */
		case ENXIO:
		default:
			g_gate_xlog("ioctl(/dev/%s): %s.", G_GATE_CTL_NAME,
			    strerror(error));
		}

		error = 0;
		switch (ggio.gctl_cmd) {
		case BIO_READ:
			blkno = ggio.gctl_offset>>15;
			if(g_gate_verbose)
				fprintf(stderr, "blockno %ld \n", blkno);
			if(g_gate_verbose && ggio.gctl_length!=bsize)
				fprintf(stderr, "blocklen %ld offset %lx\n",
				ggio.gctl_length, ggio.gctl_offset);
                        if ((size_t)ggio.gctl_length > bsize) {
                                ggio.gctl_data = realloc(ggio.gctl_data,
                                    ggio.gctl_length);
                                if (ggio.gctl_data != NULL)
                                        bsize = ggio.gctl_length;
                                else
                                        error = ENOMEM;
                        }
			rventi( blkno, ggio.gctl_data, ggio.gctl_length, ggio.gctl_offset & (0x8000-1));
			break;
		case BIO_DELETE:
		case BIO_WRITE:
		default:
			error = EOPNOTSUPP;
		}

		ggio.gctl_error = error;
		g_gate_ioctl(G_GATE_CMD_DONE, &ggio);
	}
}

static void
g_gatev_create(void)
{
	struct g_gate_ctl_create ggioc;
//	fd = open(path, g_gate_openflags(flags) | O_DIRECT | O_FSYNC);
	memset(&ggioc, 0, sizeof(ggioc));
	ggioc.gctl_version = G_GATE_VERSION;
	ggioc.gctl_unit = unit;
	ggioc.gctl_mediasize = (unsigned long int)fsys->nblock*(long int)fsys->blocksize;
	if (sectorsize == 0)
		sectorsize = 4096;
	ggioc.gctl_sectorsize = sectorsize;
	ggioc.gctl_timeout = timeout;
	ggioc.gctl_flags = flags;
	ggioc.gctl_maxcount = 0;
	strlcpy(ggioc.gctl_info, argscore, sizeof(ggioc.gctl_info));
	g_gate_ioctl(G_GATE_CMD_CREATE, &ggioc);
	if (unit == -1)
		printf("%s%u\n", G_GATE_PROVIDER_NAME, ggioc.gctl_unit);
	unit = ggioc.gctl_unit;
	g_gatev_serve();
}

void oventi() {
	if(vtparsescore(argscore, &pref, score) < 0)
		sysfatal("bad score '%s'", argscore);
	if((z = vtdial(nil)) == nil)
		sysfatal("vtdial: %r");
	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");
	if((c = vtcachealloc(z, 32768*32)) == nil)
		sysfatal("vtcache: %r");
	if((disk = diskopenventi(c, score)) == nil)
		sysfatal("diskopenventi: %r");
	if((fsys = fsysopen(disk)) == nil)
			sysfatal("fsysopen: %r");
}

void
threadmain(int argc, char *argv[])
{

	if (argc < 2)
		usage();
	if (strcasecmp(argv[1], "create") == 0)
		action = CREATE;
	else if (strcasecmp(argv[1], "destroy") == 0)
		action = DESTROY;
	else if (strcasecmp(argv[1], "list") == 0)
		action = LIST;
	else if (strcasecmp(argv[1], "test") == 0)
		action = TEST;
	else
		usage();
	argc -= 1;
	argv += 1;
	for (;;) {
		int ch;

		ch = getopt(argc, argv, "fo:s:t:u:v");
		if (ch == -1)
			break;
		switch (ch) {
		case 'f':
			if (action != DESTROY)
				usage();
			force = 1;
			break;
		case 'o':
			if (action != CREATE)
				usage();
			if (strcasecmp("ro", optarg) == 0)
				flags = G_GATE_FLAG_READONLY;
			else if (strcasecmp("wo", optarg) == 0)
				flags = G_GATE_FLAG_WRITEONLY;
			else if (strcasecmp("rw", optarg) == 0)
				flags = 0;
			else {
				errx(EXIT_FAILURE,
				    "Invalid argument for '-o' option.");
			}
			break;
		case 's':
			if (action != CREATE)
				usage();
			errno = 0;
			sectorsize = strtoul(optarg, NULL, 10);
			if (sectorsize == 0 && errno != 0)
				errx(EXIT_FAILURE, "Invalid sectorsize.");
			break;
		case 't':
//			if (action != CREATE)
//				usage();
			errno = 0;
			timeout = strtoul(optarg, NULL, 10);
			if (timeout == 0 && errno != 0)
				errx(EXIT_FAILURE, "Invalid timeout.");
			break;
		case 'u':
			errno = 0;
			unit = strtol(optarg, NULL, 10);
			if (unit == 0 && errno != 0)
				errx(EXIT_FAILURE, "Invalid unit number.");
			break;
		case 'v':
			if (action == DESTROY)
				usage();
			g_gate_verbose++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	argscore = argv[0];

	switch (action) {
	case CREATE:
		if (argc != 1)
			usage();
		oventi();
		g_gate_load_module();
		g_gate_open_device();
		g_gatev_create();
		break;
	case DESTROY:
		if (unit == -1) {
			fprintf(stderr, "Required unit number.\n");
			usage();
		}
		g_gate_verbose = 1;
		g_gate_open_device();
		g_gate_destroy(unit, force);
		break;
	case LIST:
//		g_gate_list(unit, g_gate_verbose);
		break;
	case TEST:
		oventi();
		if((b = fsysreadblock(fsys, timeout)) != nil){
			write(1, b->data, 32768);
			blockput(b);
		}
		break;
	case UNSET:
	default:
		usage();
	}
	g_gate_close_device();
	threadexitsall(nil);
}
