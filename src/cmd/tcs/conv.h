void jis_in(int fd, long *notused, struct convert *out);
void jisjis_in(int fd, long *notused, struct convert *out);
void msjis_in(int fd, long *notused, struct convert *out);
void ujis_in(int fd, long *notused, struct convert *out);
void jisjis_out(Rune *base, int n, long *notused);
void ujis_out(Rune *base, int n, long *notused);
void msjis_out(Rune *base, int n, long *notused);
void big5_in(int fd, long *notused, struct convert *out);
void big5_out(Rune *base, int n, long *notused);
void gb_in(int fd, long *notused, struct convert *out);
void gb_out(Rune *base, int n, long *notused);
void uksc_in(int fd, long *notused, struct convert *out);
void uksc_out(Rune *base, int n, long *notused);
void html_in(int fd, long *notused, struct convert *out);
void html_out(Rune *base, int n, long *notused);
void tune_in(int fd, long *notused, struct convert *out);
void tune_out(Rune *base, int n, long *notused);

#define		emit(x)		*(*r)++ = (x)
#define		NRUNE		65536

extern long tab[];		/* common table indexed by Runes for reverse mappings */
