#include <u.h>
#include <libc.h>
#include <bio.h>

#define	MAXBASE	36

void	usage(void);
void	put(int);
void	putn(int, int);
void	puttext(char *);
void	putnum(char *);
int	btoi(char *);
int	value(int, int);
int	isnum(char *);

char *str[256]={
	"nul",	"soh",	"stx",	"etx",	"eot",	"enq",	"ack",	"bel",
	"bs ",	"ht ",	"nl ",	"vt ",	"np ",	"cr ",	"so ",	"si ",
	"dle",	"dc1",	"dc2",	"dc3",	"dc4",	"nak",	"syn",	"etb",
	"can",	"em ",	"sub",	"esc",	"fs ",	"gs ",	"rs ",	"us ",
	"sp ",	" ! ",	" \" ",	" # ",	" $ ",	" % ",	" & ",	" ' ",
	" ( ",	" ) ",	" * ",	" + ",	" , ",	" - ",	" . ",	" / ",
	" 0 ",	" 1 ",	" 2 ",	" 3 ",	" 4 ",	" 5 ",	" 6 ",	" 7 ",
	" 8 ",	" 9 ",	" : ",	" ; ",	" < ",	" = ",	" > ",	" ? ",
	" @ ",	" A ",	" B ",	" C ",	" D ",	" E ",	" F ",	" G ",
	" H ",	" I ",	" J ",	" K ",	" L ",	" M ",	" N ",	" O ",
	" P ",	" Q ",	" R ",	" S ",	" T ",	" U ",	" V ",	" W ",
	" X ",	" Y ",	" Z ",	" [ ",	" \\ ",	" ] ",	" ^ ",	" _ ",
	" ` ",	" a ",	" b ",	" c ",	" d ",	" e ",	" f ",	" g ",
	" h ",	" i ",	" j ",	" k ",	" l ",	" m ",	" n ",	" o ",
	" p ",	" q ",	" r ",	" s ",	" t ",	" u ",	" v ",	" w ",
	" x ",	" y ",	" z ",	" { ",	" | ",	" } ",	" ~ ",	"del",
	"x80",	"x81",	"x82",	"x83",	"x84",	"x85",	"x86",	"x87",
	"x88",	"x89",	"x8a",	"x8b",	"x8c",	"x8d",	"x8e",	"x8f",
	"x90",	"x91",	"x92",	"x93",	"x94",	"x95",	"x96",	"x97",
	"x98",	"x99",	"x9a",	"x9b",	"x9c",	"x9d",	"x9e",	"x9f",
	"xa0",	" ¡ ",	" ¢ ",	" £ ",	" ¤ ",	" ¥ ",	" ¦ ",	" § ",
	" ¨ ",	" © ",	" ª ",	" « ",	" ¬ ",	" ­ ",	" ® ",	" ¯ ",
	" ° ",	" ± ",	" ² ",	" ³ ",	" ´ ",	" µ ",	" ¶ ",	" · ",
	" ¸ ",	" ¹ ",	" º ",	" » ",	" ¼ ",	" ½ ",	" ¾ ",	" ¿ ",
	" À ",	" Á ",	" Â ",	" Ã ",	" Ä ",	" Å ",	" Æ ",	" Ç ",
	" È ",	" É ",	" Ê ",	" Ë ",	" Ì ",	" Í ",	" Î ",	" Ï ",
	" Ð ",	" Ñ ",	" Ò ",	" Ó ",	" Ô ",	" Õ ",	" Ö ",	" × ",
	" Ø ",	" Ù ",	" Ú ",	" Û ",	" Ü ",	" Ý ",	" Þ ",	" ß ",
	" à ",	" á ",	" â ",	" ã ",	" ä ",	" å ",	" æ ",	" ç ",
	" è ",	" é ",	" ê ",	" ë ",	" ì ",	" í ",	" î ",	" ï ",
	" ð ",	" ñ ",	" ò ",	" ó ",	" ô ",	" õ ",	" ö ",	" ÷ ",
	" ø ",	" ù ",	" ú ",	" û ",	" ü ",	" ý ",	" þ ",	" ÿ "
};

char Ncol[]={
    0,0,7,5,4,4,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
};

int 	nchars=128;
int 	base=16;
int 	ncol;
int 	text=1;
int	strip=0;
Biobuf	bin;

void
main(int argc, char **argv)
{
	int i;

	Binit(&bin, 1, OWRITE);
	ARGBEGIN{
	case '8':
		nchars=256; break;
	case 'x':
		base=16; break;
	case 'o':
		base=8; break;
	case 'd':
		base=10; break;
	case 'b':
		base=strtoul(EARGF(usage()), 0, 0);
		if(base<2||base>MAXBASE)
			usage();
		break;
	case 'n':
		text=0; break;
	case 't':
		strip=1;
		/* fall through */
	case 'c':
		text=2; break;
	default:
		usage();
	}ARGEND

	ncol=Ncol[base];
	if(argc==0){
		for(i=0;i<nchars;i++){
			put(i);
			if((i&7)==7)
				Bprint(&bin, "|\n");
		}
	}else{
		if(text==1)
			text=isnum(argv[0]);
		while(argc--)
			if(text)
				puttext(*argv++);
			else
				putnum(*argv++);
	}
	Bputc(&bin, '\n');
	exits(0);
}
void
usage(void)
{
	fprint(2, "usage: %s [-8] [-xod | -b8] [-ncst] [--] [text]\n", argv0);
	exits("usage");
}
void
put(int i)
{
	Bputc(&bin, '|');
	putn(i, ncol);
	Bprint(&bin, " %s", str[i]);
}
char dig[]="0123456789abcdefghijklmnopqrstuvwxyz";
void
putn(int n, int ndig)
{
	if(ndig==0)
		return;
	putn(n/base, ndig-1);
	Bputc(&bin, dig[n%base]);
}
void
puttext(char *s)
{
	int n;
	n=btoi(s)&0377;
	if(strip)
		Bputc(&bin, n);
	else
		Bprint(&bin, "%s\n", str[n]);
}
void
putnum(char *s)
{
	while(*s){
		putn(*s++&0377, ncol);
		Bputc(&bin, '\n');
	}
}
int
btoi(char *s)
{
	int n;
	n=0;
	while(*s)
		n=n*base+value(*s++, 0);
	return(n);
}
int
value(int c, int f)
{
	char *s;
	for(s=dig; s<dig+base; s++)
		if(*s==c)
			return(s-dig);
	if(f)
		return(-1);
	fprint(2, "%s: bad input char %c\n", argv0, c);
	exits("bad");
	return 0;	/* to keep ken happy */
}
int
isnum(char *s)
{
	while(*s)
		if(value(*s++, 1)==-1)
			return(0);
	return(1);
}
