#include <u.h>
#include <libc.h>
#include <bio.h>

#define LB 2048
int	one;
int	two;
int	three;

char	ldr[3][4] = { "", "\t", "\t\t" };

Biobuf *ib1;
Biobuf *ib2;
Biobuf *openfil(char*);
int	rd(Biobuf*, char*);
void	wr(char*, int);
void	copy(Biobuf*, char*, int);
int	compare(char*, char*);

void
main(int argc, char *argv[])
{
	int l;
	char	lb1[LB],lb2[LB];

	l = 1;
	ARGBEGIN{
	case '1':
		if(!one) {
			one = 1;
			ldr[1][0] =
			ldr[2][l--] = '\0';
		}
		break;

	case '2':
		if(!two) {
			two = 1;
			ldr[2][l--] = '\0';
		}
		break;

	case '3':
		three = 1;
		break;

	default:
		goto Usage;

	}ARGEND

	if(argc < 2) {
    Usage:
		fprint(2, "usage: comm [-123] file1 file2\n");
		exits("usage");
	}

	ib1 = openfil(argv[0]);
	ib2 = openfil(argv[1]);


	if(rd(ib1,lb1) < 0){
		if(rd(ib2,lb2) < 0)
			exits(0);
		copy(ib2,lb2,2);
	}
	if(rd(ib2,lb2) < 0)
		copy(ib1,lb1,1);

	for(;;){
		switch(compare(lb1,lb2)) {
		case 0:
			wr(lb1,3);
			if(rd(ib1,lb1) < 0) {
				if(rd(ib2,lb2) < 0)
					exits(0);
				copy(ib2,lb2,2);
			}
			if(rd(ib2,lb2) < 0)
				copy(ib1,lb1,1);
			continue;

		case 1:
			wr(lb1,1);
			if(rd(ib1,lb1) < 0)
				copy(ib2,lb2,2);
			continue;

		case 2:
			wr(lb2,2);
			if(rd(ib2,lb2) < 0)
				copy(ib1,lb1,1);
			continue;
		}
	}
}

int
rd(Biobuf *file, char *buf)
{
	int i, c;

	i = 0;
	while((c = Bgetc(file)) != Beof) {
		*buf = c;
		if(c == '\n' || i > LB-2) {
			*buf = '\0';
			return 0;
		}
		i++;
		buf++;
	}
	return -1;
}

void
wr(char *str, int n)
{

	switch(n){
		case 1:
			if(one)
				return;
			break;

		case 2:
			if(two)
				return;
			break;

		case 3:
			if(three)
				return;
	}
	print("%s%s\n", ldr[n-1],str);
}

void
copy(Biobuf *ibuf, char *lbuf, int n)
{
	do
		wr(lbuf,n);
	while(rd(ibuf,lbuf) >= 0);
	exits(0);
}

int
compare(char *a, char *b)
{
	while(*a == *b){
		if(*a == '\0')
			return 0;
		a++;
		b++;
	}
	if(*a < *b)
		return 1;
	return 2;
}

Biobuf*
openfil(char *s)
{
	Biobuf *b;

	if(s[0]=='-' && s[1]==0)
		s = unsharp("#d/0");
	b = Bopen(s, OREAD);
	if(b)
		return b;
	fprint(2,"comm: cannot open %s: %r\n",s);
	exits("open");
	return 0;	/* shut up ken */
}
