#include <u.h>
#include <libc.h>
#include <draw.h>
#include <mouse.h>
#include <frame.h>

static int npass, nfail;

static void
check(char *name, int ok)
{
	if(ok){
		print("PASS: %s\n", name);
		npass++;
	}else{
		print("FAIL: %s\n", name);
		nfail++;
	}
}

static Frbox*
newbox(int bc, int nrune)
{
	Frbox *b;

	b = mallocz(sizeof(Frbox), 1);
	b->bc = bc;
	b->nrune = nrune;
	b->wid = (bc == '\t') ? 50 : 0;
	b->minwid = (bc == '\n') ? 0 : 10;
	return b;
}

static Frame*
makeframe(int nboxes)
{
	Frame *f;
	int i;

	f = mallocz(sizeof(Frame), 1);
	f->nalloc = nboxes + 4;
	f->box = mallocz(f->nalloc * sizeof(Frbox), 1);
	for(i = 0; i < nboxes; i++){
		f->box[i] = *newbox('\n', -1);
		f->box[i].font = nil;
	}
	f->nbox = nboxes;
	f->nchars = nboxes;
	return f;
}

static void
test_setboxfont_basic(void)
{
	Frame *f;
	Font *fakefont = (Font*)0xdeadbeef;

	f = makeframe(5);

	frsetboxfont(f, 1, 4, fakefont);

	check("box0.font nil before range", f->box[0].font == nil);
	check("box1.font set", f->box[1].font == fakefont);
	check("box2.font set", f->box[2].font == fakefont);
	check("box3.font set", f->box[3].font == fakefont);
	check("box4.font nil after range", f->box[4].font == nil);
	check("frame.modified set", f->modified == 1);

	free(f->box);
	free(f);
}

static void
test_setboxfont_clear(void)
{
	Frame *f;
	Font *fakefont = (Font*)0xdeadbeef;
	int i;

	f = makeframe(3);
	for(i = 0; i < 3; i++)
		f->box[i].font = fakefont;

	frsetboxfont(f, 0, 3, nil);

	check("clear box0.font nil", f->box[0].font == nil);
	check("clear box1.font nil", f->box[1].font == nil);
	check("clear box2.font nil", f->box[2].font == nil);

	free(f->box);
	free(f);
}

static void
test_setboxfont_noop_same_font(void)
{
	Frame *f;
	Font *fakefont = (Font*)0xdeadbeef;

	f = makeframe(2);
	f->box[0].font = fakefont;
	f->box[1].font = fakefont;
	f->modified = 0;

	frsetboxfont(f, 0, 2, fakefont);

	check("noop.modified set", f->modified == 1);
	check("noop box0 unchanged", f->box[0].font == fakefont);
	check("noop box1 unchanged", f->box[1].font == fakefont);

	free(f->box);
	free(f);
}

static void
test_setboxfont_empty_range(void)
{
	Frame *f;
	Font *fakefont = (Font*)0xdeadbeef;

	f = makeframe(3);
	f->modified = 0;

	frsetboxfont(f, 2, 2, fakefont);

	check("empty range: no modification", f->modified == 0);
	check("empty range: box untouched", f->box[2].font == nil);

	free(f->box);
	free(f);
}

void
main(void)
{
	print("=== libframe frsetboxfont tests ===\n\n");

	test_setboxfont_basic();
	test_setboxfont_clear();
	test_setboxfont_noop_same_font();
	test_setboxfont_empty_range();

	print("\n=== Summary ===\n");
	print("%d PASS / %d FAIL\n", npass, nfail);
	exits(nfail ? "FAIL" : nil);
}
