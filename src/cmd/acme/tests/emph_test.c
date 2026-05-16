#include <u.h>
#include <libc.h>

typedef struct Range Range;
struct Range {
	uint	q0;
	uint	q1;
};

void*
erealloc(void *p, uint n)
{
	p = realloc(p, n);
	if(p == nil) {
		fprint(2, "erealloc: out of memory\n");
		exits("erealloc");
	}
	return p;
}

void*
emalloc(uint n)
{
	void *p = malloc(n);
	if(p == nil) {
		fprint(2, "emalloc: out of memory\n");
		exits("emalloc");
	}
	memset(p, 0, n);
	return p;
}

/* Test infrastructure */
static int npass = 0;
static int nfail = 0;

static void
assert_eq(char *name, int got, int expected)
{
	if(got == expected) {
		print("PASS: %s\n", name);
		npass++;
	} else {
		print("FAIL: %s (got %d, expected %d)\n", name, got, expected);
		nfail++;
	}
}

static void
assert_range_eq(char *name, Range *m, int idx, uint q0, uint q1)
{
	if(idx < 0 || m == nil || m[idx].q0 != q0 || m[idx].q1 != q1) {
		print("FAIL: %s[%d] (got [%ud,%ud], expected [%ud,%ud])\n",
			name, idx,
			idx < 0 || m == nil ? 0 : m[idx].q0,
			idx < 0 || m == nil ? 0 : m[idx].q1,
			q0, q1);
		nfail++;
	} else {
		print("PASS: %s[%d]\n", name, idx);
		npass++;
	}
}

/* Stubs for the functions to be implemented */
extern void emphpush(Range **m, int *n, int *a, uint q0, uint q1);
extern void rangeshift(Range *m, int *n, uint q, int delta);
extern void rangemerge(Range **dst, int *ndst, int *adst, Range *src, int nsrc);
extern void emphfreearr(Range **m, int *n, int *a);

/* Group A - emphpush tests */
static void
test_push_empty(void)
{
	Range *m = nil;
	int n = 0, a = 0;

	emphpush(&m, &n, &a, 10, 15);
	assert_eq("A1: push_empty.n", n, 1);
	assert_eq("A1: push_empty.a_allocated", a > 0 ? 1 : 0, 1);
	if(n > 0)
		assert_range_eq("A1: push_empty.range", m, 0, 10, 15);
	free(m);
}

static void
test_push_growth(void)
{
	Range *m = nil;
	int n = 0, a = 0;
	int i;

	for(i = 0; i < 100; i++) {
		emphpush(&m, &n, &a, i*10, i*10+5);
	}
	assert_eq("A2: push_growth.n", n, 100);
	assert_eq("A2: push_growth.a_sufficient", a >= n ? 1 : 0, 1);
	if(n > 0) {
		for(i = 0; i < 100; i++) {
			if(m[i].q0 != i*10 || m[i].q1 != i*10+5) {
				print("FAIL: A2: push_growth order at %d\n", i);
				nfail++;
				free(m);
				return;
			}
		}
	}
	print("PASS: A2: push_growth all values\n");
	npass++;
	free(m);
}

static void
test_push_preserves_order(void)
{
	Range *m = nil;
	int n = 0, a = 0;
	int vals[] = {5, 10, 20, 30, 50};
	int i;

	for(i = 0; i < 5; i++) {
		emphpush(&m, &n, &a, vals[i], vals[i]+1);
	}
	assert_eq("A3: push_preserves_order.n", n, 5);
	if(n > 0) {
		for(i = 0; i < 5; i++) {
			if(m[i].q0 != vals[i] || m[i].q1 != vals[i]+1) {
				print("FAIL: A3: push_preserves_order[%d]\n", i);
				nfail++;
				free(m);
				return;
			}
		}
	}
	print("PASS: A3: push_preserves_order\n");
	npass++;
	free(m);
}

/* Group B - rangeshift insertion tests */
static void
test_shift_ins_before(void)
{
	Range m[] = {{10, 15}};
	int n = 1;

	rangeshift(m, &n, 5, 3);
	assert_eq("B1: shift_ins_before.n", n, 1);
	assert_range_eq("B1: shift_ins_before.range", m, 0, 13, 18);
}

static void
test_shift_ins_after(void)
{
	Range m[] = {{10, 15}};
	int n = 1;

	rangeshift(m, &n, 20, 3);
	assert_eq("B2: shift_ins_after.n", n, 1);
	assert_range_eq("B2: shift_ins_after.range", m, 0, 10, 15);
}

static void
test_shift_ins_straddle(void)
{
	Range m[] = {{10, 15}};
	int n = 1;

	rangeshift(m, &n, 12, 3);
	assert_eq("B3: shift_ins_straddle.n (dropped)", n, 0);
}

static void
test_shift_ins_at_start(void)
{
	Range m[] = {{10, 15}};
	int n = 1;

	rangeshift(m, &n, 10, 3);
	assert_eq("B4: shift_ins_at_start.n", n, 1);
	assert_range_eq("B4: shift_ins_at_start.range", m, 0, 13, 18);
}

static void
test_shift_ins_at_end(void)
{
	Range m[] = {{10, 15}};
	int n = 1;

	rangeshift(m, &n, 15, 3);
	assert_eq("B5: shift_ins_at_end.n", n, 1);
	assert_range_eq("B5: shift_ins_at_end.range", m, 0, 10, 15);
}

/* Group C - rangeshift deletion tests */
static void
test_shift_del_before(void)
{
	Range m[] = {{10, 15}};
	int n = 1;

	rangeshift(m, &n, 3, -2);
	assert_eq("C1: shift_del_before.n", n, 1);
	assert_range_eq("C1: shift_del_before.range", m, 0, 8, 13);
}

static void
test_shift_del_after(void)
{
	Range m[] = {{10, 15}};
	int n = 1;

	rangeshift(m, &n, 20, -2);
	assert_eq("C2: shift_del_after.n", n, 1);
	assert_range_eq("C2: shift_del_after.range", m, 0, 10, 15);
}

static void
test_shift_del_overlap_start(void)
{
	Range m[] = {{10, 15}};
	int n = 1;

	rangeshift(m, &n, 8, -4);
	assert_eq("C3: shift_del_overlap_start.n (dropped)", n, 0);
}

static void
test_shift_del_overlap_end(void)
{
	Range m[] = {{10, 15}};
	int n = 1;

	rangeshift(m, &n, 12, -6);
	assert_eq("C4: shift_del_overlap_end.n (dropped)", n, 0);
}

static void
test_shift_del_inside(void)
{
	Range m[] = {{10, 15}};
	int n = 1;

	rangeshift(m, &n, 11, -2);
	assert_eq("C5: shift_del_inside.n (dropped)", n, 0);
}

static void
test_shift_del_contains(void)
{
	Range m[] = {{10, 15}};
	int n = 1;

	rangeshift(m, &n, 8, -12);
	assert_eq("C6: shift_del_contains.n (dropped)", n, 0);
}

/* Group D - compaction multi-ranges */
static void
test_shift_compact_many(void)
{
	Range m[] = {
		{10, 12},
		{20, 22},
		{30, 32},
		{40, 42},
		{50, 52}
	};
	int n = 5;

	rangeshift(m, &n, 31, 3);
	assert_eq("D1: shift_compact_many.n", n, 4);
	assert_range_eq("D1: shift_compact_many[0]", m, 0, 10, 12);
	assert_range_eq("D1: shift_compact_many[1]", m, 1, 20, 22);
	assert_range_eq("D1: shift_compact_many[2]", m, 2, 43, 45);
	assert_range_eq("D1: shift_compact_many[3]", m, 3, 53, 55);
}

/* Group E - rangemerge tests */
static void
test_merge_disjoint_after(void)
{
	Range *dst = nil;
	int ndst = 0, adst = 0;
	Range src[] = {{100, 102}};
	int nsrc = 1;

	emphpush(&dst, &ndst, &adst, 10, 12);
	emphpush(&dst, &ndst, &adst, 30, 32);

	rangemerge(&dst, &ndst, &adst, src, nsrc);
	assert_eq("E1: merge_disjoint_after.n", ndst, 3);
	assert_range_eq("E1: merge_disjoint_after[2]", dst, 2, 100, 102);
	free(dst);
}

static void
test_merge_interleaved(void)
{
	Range *dst = nil;
	int ndst = 0, adst = 0;
	Range src[] = {{5, 7}, {20, 22}};
	int nsrc = 2;

	emphpush(&dst, &ndst, &adst, 10, 12);
	emphpush(&dst, &ndst, &adst, 30, 32);

	rangemerge(&dst, &ndst, &adst, src, nsrc);
	assert_eq("E2: merge_interleaved.n", ndst, 4);
	assert_range_eq("E2: merge_interleaved[0]", dst, 0, 5, 7);
	assert_range_eq("E2: merge_interleaved[1]", dst, 1, 10, 12);
	assert_range_eq("E2: merge_interleaved[2]", dst, 2, 20, 22);
	assert_range_eq("E2: merge_interleaved[3]", dst, 3, 30, 32);
	free(dst);
}

static void
test_merge_empty_src(void)
{
	Range *dst = nil;
	int ndst = 0, adst = 0;
	int nsrc = 0;

	emphpush(&dst, &ndst, &adst, 10, 12);

	rangemerge(&dst, &ndst, &adst, nil, nsrc);
	assert_eq("E3: merge_empty_src.n", ndst, 1);
	free(dst);
}

static void
test_merge_into_empty(void)
{
	Range *dst = nil;
	int ndst = 0, adst = 0;
	Range src[] = {{10, 12}, {20, 22}};
	int nsrc = 2;

	rangemerge(&dst, &ndst, &adst, src, nsrc);
	assert_eq("E4: merge_into_empty.n", ndst, 2);
	assert_range_eq("E4: merge_into_empty[0]", dst, 0, 10, 12);
	assert_range_eq("E4: merge_into_empty[1]", dst, 1, 20, 22);
	free(dst);
}

/* Group F - emphfreearr tests */
static void
test_free_resets(void)
{
	Range *m = nil;
	int n = 0, a = 0;

	emphpush(&m, &n, &a, 10, 15);
	emphpush(&m, &n, &a, 20, 25);

	emphfreearr(&m, &n, &a);
	assert_eq("F1: free_resets.n", n, 0);
	assert_eq("F1: free_resets.a", a, 0);
	assert_eq("F1: free_resets.m_nil", m == nil ? 1 : 0, 1);
}

void
main(void)
{
	print("=== Emphasis Range Test Suite (A-F) ===\n");

	print("\nGroup A - emphpush:\n");
	test_push_empty();
	test_push_growth();
	test_push_preserves_order();

	print("\nGroup B - rangeshift insertion:\n");
	test_shift_ins_before();
	test_shift_ins_after();
	test_shift_ins_straddle();
	test_shift_ins_at_start();
	test_shift_ins_at_end();

	print("\nGroup C - rangeshift deletion:\n");
	test_shift_del_before();
	test_shift_del_after();
	test_shift_del_overlap_start();
	test_shift_del_overlap_end();
	test_shift_del_inside();
	test_shift_del_contains();

	print("\nGroup D - multi-range compaction:\n");
	test_shift_compact_many();

	print("\nGroup E - rangemerge:\n");
	test_merge_disjoint_after();
	test_merge_interleaved();
	test_merge_empty_src();
	test_merge_into_empty();

	print("\nGroup F - emphfreearr:\n");
	test_free_resets();

	print("\n=== Summary ===\n");
	print("%d PASS / %d FAIL\n", npass, nfail);

	exits(nfail > 0 ? "FAIL" : nil);
}
