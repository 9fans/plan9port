#include <u.h>
#include <libc.h>

#define		BUF		65536

int sflag = 0;
int lflag = 0;
int Lflag = 0;

static void usage(void);

void
main(int argc, char *argv[])
{
	int n, i;
	uchar *p, *q;
	uchar buf1[BUF], buf2[BUF];
	int f1, f2;
	vlong nc = 1, o, l = 1;
	char *name1, *name2;
	uchar *b1s, *b1e, *b2s, *b2e;

	ARGBEGIN{
	case 's':	sflag = 1; break;
	case 'l':	lflag = 1; break;
	case 'L':	Lflag = 1; break;
	default:	usage();
	}ARGEND
	if(argc < 2)
		usage();
	if((f1 = open(name1 = *argv++, OREAD)) == -1){
		if(!sflag) perror(name1);
		exits("open");
	}
	if((f2 = open(name2 = *argv++, OREAD)) == -1){
		if(!sflag) perror(name2);
		exits("open");
	}
	if(*argv){
		o = strtoll(*argv++, 0, 0);
		if(seek(f1, o, 0) < 0){
			if(!sflag) perror("cmp: seek by offset1");
			exits("seek 1");
		}
	}
	if(*argv){
		o = strtoll(*argv++, 0, 0);
		if(seek(f2, o, 0) < 0){
			if(!sflag) perror("cmp: seek by offset2");
			exits("seek 2");
		}
	}
	if(*argv)
		usage();
	b1s = b1e = buf1;
	b2s = b2e = buf2;
	for(;;){
		if(b1s >= b1e){
			if(b1s >= &buf1[BUF])
				b1s = buf1;
			n = read(f1, b1s,  &buf1[BUF] - b1s);
			b1e = b1s + n;
		}
		if(b2s >= b2e){
			if(b2s >= &buf2[BUF])
				b2s = buf2;
			n = read(f2, b2s,  &buf2[BUF] - b2s);
			b2e = b2s + n;
		}
		n = b2e - b2s;
		if(n > b1e - b1s)
			n = b1e - b1s;
		if(n <= 0)
			break;
		if(memcmp((void *)b1s, (void *)b2s, n) != 0){
			if(sflag)
				exits("differ");
			for(p = b1s, q = b2s, i = 0; i < n; p++, q++, i++) {
				if(*p == '\n')
					l++;
				if(*p != *q){
					if(!lflag){
						print("%s %s differ: char %lld",
						    name1, name2, nc+i);
						print(Lflag?" line %lld\n":"\n", l);
						exits("differ");
					}
					print("%6lld 0x%.2x 0x%.2x\n", nc+i, *p, *q);
				}
			}
		}
		if(Lflag)
			for(p = b1s; p < b1e;)
				if(*p++ == '\n')
					l++;
		nc += n;
		b1s += n;
		b2s += n;
	}
	if(b1e - b1s == b2e - b2s)
		exits((char *)0);
	if(!sflag)
		print("EOF on %s\n", (b1e - b1s > b2e - b2s)? name2 : name1);
	exits("EOF");
}

static void
usage(void)
{
	print("Usage: cmp [-lsL] file1 file2 [offset1 [offset2] ]\n");
	exits("usage");
}
