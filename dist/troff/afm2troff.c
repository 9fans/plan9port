#include <u.h>
#include <libc.h>
#include <bio.h>

int didname;
int didfontname;
int offset;
void run(char*, int);
Biobuf bout;

void
usage(void)
{
	fprint(2, "usage: afm2troff [-h] [-o offset] [file...]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int i, fd;
	
	ARGBEGIN{
	case 'h':
		didname = 1;
		didfontname = 1;
		break;
	case 'o':
		offset = atoi(EARGF(usage()));
		break;
	default:
		usage();
	}ARGEND
	
	Binit(&bout, 1, OWRITE);
	if(argc == 0)
		run("<stdin>", 0);
	else{
		for(i=0; i<argc; i++){
			if((fd = open(argv[i], OREAD)) < 0)
				sysfatal("open %s: %r", argv[i]);
			run(argv[i], fd);
		}
	}
	Bflush(&bout);
}

void
run(char *name, int fd)
{
	char *p, *q, *f[100];
	int nf, code, wid, ad;
	Biobuf b;
	Fmt fmt;
	
	fmtstrinit(&fmt);
	Binit(&b, fd, OREAD);
	while((p = Brdline(&b, '\n')) != nil){
		p[Blinelen(&b)-1] = 0;
		q = strchr(p, ' ');
		if(q == nil)
			continue;
		*q++ = 0;
		while(*q == ' ' || *q == '\t')
			q++;
		if(*q == 0)
			continue;
		if(strcmp(p, "FontName") == 0 && didname++ == 0)
			 Bprint(&bout, "name %s\n", q);
		if(strcmp(p, "FullName") == 0 && didfontname++ == 0)
			 Bprint(&bout, "fontname %s\n", q);
		if(strcmp(p, "C") == 0){
			nf = getfields(q, f, nelem(f), 1, "\t\r\n\v ");
			if(nf < 5 || strcmp(f[1], ";") != 0 || strcmp(f[2], "WX") != 0)
				continue;
			code = strtol(f[0], 0, 10);
			wid = strtol(f[3], 0, 10);
			wid = (wid+5)/10;
			if(code == 0)
				continue;
			code += offset;
			ad = 0;
			if(nf < 6 || strcmp(f[nf-6], "B") != 0)
				continue;
			if(atoi(f[nf-4]) < -50)
				ad |= 1;
			if(atoi(f[nf-2]) > 600)
				ad |= 2;
			if(nf >= 7 && strcmp(f[5], "N") == 0 && strcmp(f[6], "space") == 0)
				code = ' ';
			if(code == ' ')
				Bprint(&bout, "spacewidth %d\ncharset\n", wid);
			else
				fmtprint(&fmt, "%C\t%d\t%d\t%d %04x\n",
					code, wid, ad, code, code);
		}
	}
	Bprint(&bout, "%s", fmtstrflush(&fmt));
}
