#include <u.h>
#include <libc.h>
#include <bio.h>
#include "diff.h"

/*	diff - differential file comparison
*
*	Uses an algorithm due to Harold Stone, which finds
*	a pair of longest identical subsequences in the two
*	files.
*
*	The major goal is to generate the match vector J.
*	J[i] is the index of the line in file1 corresponding
*	to line i file0. J[i] = 0 if there is no
*	such line in file1.
*
*	Lines are hashed so as to work in core. All potential
*	matches are located by sorting the lines of each file
*	on the hash (called value). In particular, this
*	collects the equivalence classes in file1 together.
*	Subroutine equiv replaces the value of each line in
*	file0 by the index of the first element of its
*	matching equivalence in (the reordered) file1.
*	To save space equiv squeezes file1 into a single
*	array member in which the equivalence classes
*	are simply concatenated, except that their first
*	members are flagged by changing sign.
*
*	Next the indices that point into member are unsorted into
*	array class according to the original order of file0.
*
*	The cleverness lies in routine stone. This marches
*	through the lines of file0, developing a vector klist
*	of "k-candidates". At step i a k-candidate is a matched
*	pair of lines x,y (x in file0 y in file1) such that
*	there is a common subsequence of lenght k
*	between the first i lines of file0 and the first y
*	lines of file1, but there is no such subsequence for
*	any smaller y. x is the earliest possible mate to y
*	that occurs in such a subsequence.
*
*	Whenever any of the members of the equivalence class of
*	lines in file1 matable to a line in file0 has serial number
*	less than the y of some k-candidate, that k-candidate
*	with the smallest such y is replaced. The new
*	k-candidate is chained (via pred) to the current
*	k-1 candidate so that the actual subsequence can
*	be recovered. When a member has serial number greater
*	that the y of all k-candidates, the klist is extended.
*	At the end, the longest subsequence is pulled out
*	and placed in the array J by unravel.
*
*	With J in hand, the matches there recorded are
*	check'ed against reality to assure that no spurious
*	matches have crept in due to hashing. If they have,
*	they are broken, and "jackpot " is recorded--a harmless
*	matter except that a true match for a spuriously
*	mated line may now be unnecessarily reported as a change.
*
*	Much of the complexity of the program comes simply
*	from trying to minimize core utilization and
*	maximize the range of doable problems by dynamically
*	allocating what is needed and reusing what is not.
*	The core requirements for problems larger than somewhat
*	are (in words) 2*length(file0) + length(file1) +
*	3*(number of k-candidates installed),  typically about
*	6n words for files of length n.
*/

#define class diffclass

/* TIDY THIS UP */
struct cand {
	int x;
	int y;
	int pred;
} cand;
struct line {
	int serial;
	int value;
} *file[2], line;
int len[2];
int binary;
struct line *sfile[2];	/*shortened by pruning common prefix and suffix*/
int slen[2];
int pref, suff;	/*length of prefix and suffix*/
int *class;	/*will be overlaid on file[0]*/
int *member;	/*will be overlaid on file[1]*/
int *klist;		/*will be overlaid on file[0] after class*/
struct cand *clist;	/* merely a free storage pot for candidates */
int clen;
int *J;		/*will be overlaid on class*/
long *ixold;	/*will be overlaid on klist*/
long *ixnew;	/*will be overlaid on file[1]*/
/* END OF SOME TIDYING */

static void
sort(struct line *a, int n)	/*shellsort CACM #201*/
{
	int m;
	struct line *ai, *aim, *j, *k;
	struct line w;
	int i;

	m = 0;
	for (i = 1; i <= n; i *= 2)
		m = 2*i - 1;
	for (m /= 2; m != 0; m /= 2) {
		k = a+(n-m);
		for (j = a+1; j <= k; j++) {
			ai = j;
			aim = ai+m;
			do {
				if (aim->value > ai->value ||
				   aim->value == ai->value &&
				   aim->serial > ai->serial)
					break;
				w = *ai;
				*ai = *aim;
				*aim = w;

				aim = ai;
				ai -= m;
			} while (ai > a && aim >= ai);
		}
	}
}

static void
unsort(struct line *f, int l, int *b)
{
	int *a;
	int i;

	a = MALLOC(int, (l+1));
	for(i=1;i<=l;i++)
		a[f[i].serial] = f[i].value;
	for(i=1;i<=l;i++)
		b[i] = a[i];
	FREE(a);
}

static void
prune(void)
{
	int i,j;

	for(pref=0;pref<len[0]&&pref<len[1]&&
		file[0][pref+1].value==file[1][pref+1].value;
		pref++ ) ;
	for(suff=0;suff<len[0]-pref&&suff<len[1]-pref&&
		file[0][len[0]-suff].value==file[1][len[1]-suff].value;
		suff++) ;
	for(j=0;j<2;j++) {
		sfile[j] = file[j]+pref;
		slen[j] = len[j]-pref-suff;
		for(i=0;i<=slen[j];i++)
			sfile[j][i].serial = i;
	}
}

static void
equiv(struct line *a, int n, struct line *b, int m, int *c)
{
	int i, j;

	i = j = 1;
	while(i<=n && j<=m) {
		if(a[i].value < b[j].value)
			a[i++].value = 0;
		else if(a[i].value == b[j].value)
			a[i++].value = j;
		else
			j++;
	}
	while(i <= n)
		a[i++].value = 0;
	b[m+1].value = 0;
	j = 0;
	while(++j <= m) {
		c[j] = -b[j].serial;
		while(b[j+1].value == b[j].value) {
			j++;
			c[j] = b[j].serial;
		}
	}
	c[j] = -1;
}

static int
newcand(int x, int  y, int pred)
{
	struct cand *q;

	clist = REALLOC(clist, struct cand, (clen+1));
	q = clist + clen;
	q->x = x;
	q->y = y;
	q->pred = pred;
	return clen++;
}

static int
search(int *c, int k, int y)
{
	int i, j, l;
	int t;

	if(clist[c[k]].y < y)	/*quick look for typical case*/
		return k+1;
	i = 0;
	j = k+1;
	while((l=(i+j)/2) > i) {
		t = clist[c[l]].y;
		if(t > y)
			j = l;
		else if(t < y)
			i = l;
		else
			return l;
	}
	return l+1;
}

static int
stone(int *a, int n, int *b, int *c)
{
	int i, k,y;
	int j, l;
	int oldc, tc;
	int oldl;

	k = 0;
	c[0] = newcand(0,0,0);
	for(i=1; i<=n; i++) {
		j = a[i];
		if(j==0)
			continue;
		y = -b[j];
		oldl = 0;
		oldc = c[0];
		do {
			if(y <= clist[oldc].y)
				continue;
			l = search(c, k, y);
			if(l!=oldl+1)
				oldc = c[l-1];
			if(l<=k) {
				if(clist[c[l]].y <= y)
					continue;
				tc = c[l];
				c[l] = newcand(i,y,oldc);
				oldc = tc;
				oldl = l;
			} else {
				c[l] = newcand(i,y,oldc);
				k++;
				break;
			}
		} while((y=b[++j]) > 0);
	}
	return k;
}

static void
unravel(int p)
{
	int i;
	struct cand *q;

	for(i=0; i<=len[0]; i++) {
		if (i <= pref)
			J[i] = i;
		else if (i > len[0]-suff)
			J[i] = i+len[1]-len[0];
		else
			J[i] = 0;
	}
	for(q=clist+p;q->y!=0;q=clist+q->pred)
		J[q->x+pref] = q->y+pref;
}

static void
output(void)
{
	int m, i0, i1, j0, j1;

	m = len[0];
	J[0] = 0;
	J[m+1] = len[1]+1;
	if (mode != 'e') {
		for (i0 = 1; i0 <= m; i0 = i1+1) {
			while (i0 <= m && J[i0] == J[i0-1]+1)
				i0++;
			j0 = J[i0-1]+1;
			i1 = i0-1;
			while (i1 < m && J[i1+1] == 0)
				i1++;
			j1 = J[i1+1]-1;
			J[i1] = j1;
			change(i0, i1, j0, j1);
		}
	}
	else {
		for (i0 = m; i0 >= 1; i0 = i1-1) {
			while (i0 >= 1 && J[i0] == J[i0+1]-1 && J[i0])
				i0--;
			j0 = J[i0+1]-1;
			i1 = i0+1;
			while (i1 > 1 && J[i1-1] == 0)
				i1--;
			j1 = J[i1-1]+1;
			J[i1] = j1;
			change(i1 , i0, j1, j0);
		}
	}
	if (m == 0)
		change(1, 0, 1, len[1]);
	flushchanges();
}

#define BUF 4096
static int
cmp(Biobuf* b1, Biobuf* b2)
{
	int n;
	uchar buf1[BUF], buf2[BUF];
	int f1, f2;
	vlong nc = 1;
	uchar *b1s, *b1e, *b2s, *b2e;

	f1 = Bfildes(b1);
	f2 = Bfildes(b2);
	seek(f1, 0, 0);
	seek(f2, 0, 0);
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
			return 1;
		}
		nc += n;
		b1s += n;
		b2s += n;
	}
	if(b1e - b1s == b2e - b2s)
		return 0;
	return 1;
}

void
diffreg(char *f, char *t)
{
	Biobuf *b0, *b1;
	int k;

	binary = 0;
	b0 = prepare(0, f);
	if (!b0)
		return;
	b1 = prepare(1, t);
	if (!b1) {
		FREE(file[0]);
		Bterm(b0);
		return;
	}
	if (binary){
		/* could use b0 and b1 but this is simpler. */
		if (cmp(b0, b1))
			print("binary files %s %s differ\n", f, t);
		Bterm(b0);
		Bterm(b1);
		return;
	}
	clen = 0;
	prune();
	sort(sfile[0], slen[0]);
	sort(sfile[1], slen[1]);

	member = (int *)file[1];
	equiv(sfile[0], slen[0], sfile[1], slen[1], member);
	member = REALLOC(member, int, slen[1]+2);

	class = (int *)file[0];
	unsort(sfile[0], slen[0], class);
	class = REALLOC(class, int, slen[0]+2);

	klist = MALLOC(int, slen[0]+2);
	clist = MALLOC(struct cand, 1);
	k = stone(class, slen[0], member, klist);
	FREE(member);
	FREE(class);

	J = MALLOC(int, len[0]+2);
	unravel(klist[k]);
	FREE(clist);
	FREE(klist);

	ixold = MALLOC(long, len[0]+2);
	ixnew = MALLOC(long, len[1]+2);
	Bseek(b0, 0, 0); Bseek(b1, 0, 0);
	check(b0, b1);
	output();
	FREE(J); FREE(ixold); FREE(ixnew);
	Bterm(b0); Bterm(b1);			/* ++++ */
}
