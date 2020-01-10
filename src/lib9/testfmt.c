#include <u.h>
#include <libc.h>
#include <stdio.h>

int failed;

/* Consume argument and ignore it */
int
Zflag(Fmt* f)
{
	if(va_arg(f->args, int))
		;
	return 1;	/* it's a flag */
}

void
verify(char *s, char *t)
{
	if(strcmp(s, t) != 0){
		failed = 1;
		fprintf(stderr, "error: (%s) != (%s)\n", s, t);
	}
	free(s);
}

Rune lightsmiley = 0x263a;
Rune darksmiley = 0x263b;

/* Test printer that loads unusual decimal point and separator */
char*
mysmprint(char *fmt, ...)
{
	Fmt f;

	if(fmtstrinit(&f) < 0)
		return 0;
	va_start(f.args, fmt);
	f.decimal = smprint("%C", lightsmiley);
	f.thousands = smprint("%C", darksmiley);
	f.grouping = "\1\2\3\4";
	if(dofmt(&f, fmt) < 0)
		return 0;
	va_end(f.args);
	return fmtstrflush(&f);
}

double near1[] = {
	0.5,
	0.95,
	0.995,
	0.9995,
	0.99995,
	0.999995,
	0.9999995,
	0.99999995,
	0.999999995,
};

void
main(int argc, char **argv)
{
	int i, j;

	quotefmtinstall();
	fmtinstall('Z', Zflag);
	fmtinstall(L'\x263a', Zflag);
#ifdef PLAN9PORT
{ extern int __ifmt(Fmt*);
	fmtinstall('i', __ifmt);
}
#endif

	verify(smprint("hello world"), "hello world");
#ifdef PLAN9PORT
	verify(smprint("x: %ux", 0x87654321), "x: 87654321");
#else
	verify(smprint("x: %x", 0x87654321), "x: 87654321");
#endif
	verify(smprint("d: %d", 0x87654321), "d: -2023406815");
	verify(smprint("s: %s", "hi there"), "s: hi there");
	verify(smprint("q: %q", "hi i'm here"), "q: 'hi i''m here'");
	verify(smprint("c: %c", '!'), "c: !");
	verify(smprint("g: %g %g %g", 3.14159, 3.14159e10, 3.14159e-10), "g: 3.14159 3.14159e+10 3.14159e-10");
	verify(smprint("e: %e %e %e", 3.14159, 3.14159e10, 3.14159e-10), "e: 3.141590e+00 3.141590e+10 3.141590e-10");
	verify(smprint("f: %f %f %f", 3.14159, 3.14159e10, 3.14159e-10), "f: 3.141590 31415900000.000000 0.000000");
	verify(smprint("smiley: %C", (Rune)0x263a), "smiley: \xe2\x98\xba");
	verify(smprint("%g %.18g", 2e25, 2e25), "2e+25 2e+25");
	verify(smprint("%2.18g", 1.0), " 1");
	verify(smprint("%f", 3.1415927/4), "0.785398");
	verify(smprint("%d", 23), "23");
	verify(smprint("%i", 23), "23");
	verify(smprint("%Zi", 1234, 23), "23");

	/* ANSI and their wacky corner cases */
	verify(smprint("%.0d", 0), "");
	verify(smprint("%.0o", 0), "");
	verify(smprint("%.0x", 0), "");
	verify(smprint("%#.0o", 0), "0");
	verify(smprint("%#.0x", 0), "");

	/* difficult floating point tests that many libraries get wrong */
	verify(smprint("%.100f", 1.0), "1.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
	verify(smprint("%.100g", 1.0), "1");
	verify(smprint("%0100f", 1.0), "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001.000000");
	for(i=1; i<9; i++)
		for(j=0; j<=i; j++)
			verify(smprint("%.*g", j, near1[i]), "1");

	/* test $ reorderings */
	verify(smprint("%3$d %4$06d %2$d %1$d", 444, 333, 111, 222), "111 000222 333 444");
	verify(smprint("%3$Zd %5$06d %2$d %1$d", 444, 333, 555, 111, 222), "111 000222 333 444");
	verify(smprint("%3$d %4$*5$06d %2$d %1$d", 444, 333, 111, 222, 20), "111               000222 333 444");
	verify(smprint("%3$hd %4$*5$06d %2$d %1$d", 444, 333, (short)111, 222, 20), "111               000222 333 444");
	verify(smprint("%3$\xe2\x98\xba""d %5$06d %2$d %1$d", 444, 333, 555, 111, 222), "111 000222 333 444");

	/* test %'d formats */
	verify(smprint("%'d %'d %'d", 1, 2222, 33333333), "1 2,222 33,333,333");
	verify(smprint("%'019d", 0), "000,000,000,000,000");
	verify(smprint("%'08d %'08d %'08d", 1, 2222, 33333333), "0,000,001 0,002,222 33,333,333");
#ifdef PLAN9PORT
	verify(smprint("%'ux %'uX %'ub", 0x11111111, 0xabcd1234, 12345), "1111:1111 ABCD:1234 11:0000:0011:1001");
#else
	verify(smprint("%'x %'X %'b", 0x11111111, 0xabcd1234, 12345), "1111:1111 ABCD:1234 11:0000:0011:1001");
#endif
	verify(smprint("%'lld %'lld %'lld", 1LL, 222222222LL, 3333333333333LL), "1 222,222,222 3,333,333,333,333");
	verify(smprint("%'019lld %'019lld %'019lld", 1LL, 222222222LL, 3333333333333LL), "000,000,000,000,001 000,000,222,222,222 003,333,333,333,333");
#ifdef PLAN9PORT
	verify(smprint("%'llux %'lluX %'llub", 0x111111111111LL, 0xabcd12345678LL, 112342345LL), "1111:1111:1111 ABCD:1234:5678 110:1011:0010:0011:0101:0100:1001");
#else
	verify(smprint("%'llx %'llX %'llb", 0x111111111111LL, 0xabcd12345678LL, 112342345LL), "1111:1111:1111 ABCD:1234:5678 110:1011:0010:0011:0101:0100:1001");
#endif

	/* test %'d with custom (utf-8!) separators */
	/* x and b still use : */
	verify(mysmprint("%'d %'d %'d", 1, 2222, 33333333), "1 2\xe2\x98\xbb""22\xe2\x98\xbb""2 33\xe2\x98\xbb""333\xe2\x98\xbb""33\xe2\x98\xbb""3");
#ifdef PLAN9PORT
	verify(mysmprint("%'ux %'uX %'ub", 0x11111111, 0xabcd1234, 12345), "1111:1111 ABCD:1234 11:0000:0011:1001");
#else
	verify(mysmprint("%'x %'X %'b", 0x11111111, 0xabcd1234, 12345), "1111:1111 ABCD:1234 11:0000:0011:1001");
#endif
	verify(mysmprint("%'lld %'lld %'lld", 1LL, 222222222LL, 3333333333333LL), "1 222\xe2\x98\xbb""222\xe2\x98\xbb""22\xe2\x98\xbb""2 333\xe2\x98\xbb""3333\xe2\x98\xbb""333\xe2\x98\xbb""33\xe2\x98\xbb""3");
	verify(mysmprint("%'llx %'llX %'llb", 0x111111111111LL, 0xabcd12345678LL, 112342345LL), "1111:1111:1111 ABCD:1234:5678 110:1011:0010:0011:0101:0100:1001");
	verify(mysmprint("%.4f", 3.14159), "3\xe2\x98\xba""1416");

	if(failed)
		sysfatal("tests failed");
	exits(0);
}
