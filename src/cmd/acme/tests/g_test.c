#include <u.h>
#include <libc.h>
#include <thread.h>

/*
 * Integration tests G1-G4: verify rxcompile/rxexecute behavior
 * as used by emphrecompute. Calls rxexecute(nil, rune_array, ...)
 * to avoid the textreadc dependency (only needed when t != nil).
 */

typedef struct Range Range;
typedef struct Rangeset Rangeset;
typedef struct Text Text;
typedef struct Mntdir Mntdir;

enum { NRange = 10, Infinity = 0x7FFFFFFF };

struct Range   { int q0; int q1; };
struct Rangeset { Range r[NRange]; };

/* Stubs for symbols referenced by regx.o */
void *emalloc(ulong n)            { void *p = malloc(n); if(!p) sysfatal("malloc: %r"); return p; }
void *erealloc(void *v, ulong n)  { v = realloc(v, n); if(!v) sysfatal("realloc: %r"); return v; }
void  error(char *s)              { fprint(2, "error: %s\n", s); threadexitsall("error"); }
void  warning(Mntdir *md, char *s, ...) { USED(md); USED(s); }
Rune  textreadc(Text *t, uint q)  { USED(t); USED(q); return 0; }

int
runeeq(Rune *s1, uint n1, Rune *s2, uint n2)
{
	if(n1 != n2)
		return 0;
	if(n1 == 0)
		return 1;
	return memcmp(s1, s2, n1*sizeof(Rune)) == 0;
}

extern void rxinit(void);
extern int  rxcompile(Rune *r);
extern int  rxexecute(Text *t, Rune *r, uint startp, uint eof, Rangeset *rp);

static int npass, nfail;

static void
check(char *name, int ok)
{
	if(ok){
		print("PASS: %s\n", name);
		npass++;
	} else {
		print("FAIL: %s\n", name);
		nfail++;
	}
}

/*
 * Collect all non-overlapping matches of the compiled pattern in text[0..textlen).
 * Uses the same loop as emphrecompute.
 * Returns number of matches, -1 on compile error.
 */
static int
collect(Rune *pat, Rune *text, uint textlen, Range *out, int maxout)
{
	Rangeset sel;
	uint p;
	int n;

	if(!rxcompile(pat))
		return -1;
	p = 0;
	n = 0;
	while(n < maxout && p <= textlen && rxexecute(nil, text, p, textlen, &sel)){
		out[n].q0 = sel.r[0].q0;
		out[n].q1 = sel.r[0].q1;
		n++;
		p = (sel.r[0].q1 > sel.r[0].q0) ? sel.r[0].q1 : sel.r[0].q0 + 1;
	}
	return n;
}

static void
test_g1_multi_match(void)
{
	/*
	 * G1: "foo" on "foofoofoo" -> 3 matches at [0,3] [3,6] [6,9].
	 * Verifies that emphrecompute-style loop yields all non-overlapping matches.
	 */
	Rune pat[]  = {'f','o','o',0};
	Rune text[] = {'f','o','o','f','o','o','f','o','o',0};
	Range out[16];
	int n;

	n = collect(pat, text, 9, out, 16);
	check("G1: match count == 3", n == 3);
	check("G1: match[0] == [0,3]", n>=1 && out[0].q0==0 && out[0].q1==3);
	check("G1: match[1] == [3,6]", n>=2 && out[1].q0==3 && out[1].q1==6);
	check("G1: match[2] == [6,9]", n>=3 && out[2].q0==6 && out[2].q1==9);
}

static void
test_g2_bol_anchor(void)
{
	/*
	 * G2: "^" on "a\nb\nc" -> matches [0,0] [2,2] [4,4].
	 * Verifies BOL matching at start and after each '\n'; no infinite loop
	 * on zero-length matches (the +1 advance in collect handles that).
	 */
	Rune pat[]  = {'^',0};
	Rune text[] = {'a','\n','b','\n','c',0};
	Range out[16];
	int n;

	n = collect(pat, text, 5, out, 16);
	check("G2: match count == 3", n == 3);
	check("G2: match[0] == [0,0]", n>=1 && out[0].q0==0 && out[0].q1==0);
	check("G2: match[1] == [2,2]", n>=2 && out[1].q0==2 && out[1].q1==2);
	check("G2: match[2] == [4,4]", n>=3 && out[2].q0==4 && out[2].q1==4);
}

static void
test_g3_no_match(void)
{
	/*
	 * G3: "xyz" on "abc" -> 0 matches.
	 * Verifies that emphrecompute produces an empty emphmatch array when
	 * the pattern does not appear in the text.
	 */
	Rune pat[]  = {'x','y','z',0};
	Rune text[] = {'a','b','c',0};
	Range out[16];
	int n;

	n = collect(pat, text, 3, out, 16);
	check("G3: match count == 0", n == 0);
}

static void
test_g4_eol_anchor(void)
{
	/*
	 * G4: "$" on "abc\n" -> 1 match at [3,3].
	 * In regx.c, EOL matches only when the current character is '\n'.
	 * A trailing '\n' (standard acme file convention) is required.
	 */
	Rune pat[]  = {'$',0};
	Rune text[] = {'a','b','c','\n',0};
	Range out[16];
	int n;

	n = collect(pat, text, 4, out, 16);
	check("G4: match count == 1", n == 1);
	check("G4: match[0] == [3,3]", n>=1 && out[0].q0==3 && out[0].q1==3);
}

void
threadmain(int argc, char **argv)
{
	USED(argc); USED(argv);

	rxinit();

	print("=== Regex integration tests (G1-G4) ===\n\n");

	print("G1 - multi-match:\n");
	test_g1_multi_match();

	print("\nG2 - BOL anchor:\n");
	test_g2_bol_anchor();

	print("\nG3 - no match:\n");
	test_g3_no_match();

	print("\nG4 - EOL anchor:\n");
	test_g4_eol_anchor();

	print("\n=== Summary ===\n");
	print("%d PASS / %d FAIL\n", npass, nfail);

	threadexitsall(nfail ? "FAIL" : nil);
}
