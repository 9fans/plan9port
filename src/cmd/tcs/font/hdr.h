typedef Bitmap *readbitsfn(char *, int, long *, int, uchar *, int **);
typedef void mapfn(int, int, long *);

extern Bitmap *kreadbits(char *file, int nc, long *chars, int size, uchar *bits, int **);
extern void kmap(int from, int to, long *chars);
extern Bitmap *breadbits(char *file, int nc, long *chars, int size, uchar *bits, int **);
extern void bmap(int from, int to, long *chars);
extern Bitmap *greadbits(char *file, int nc, long *chars, int size, uchar *bits, int **);
extern void gmap(int from, int to, long *chars);
extern Bitmap *qreadbits(char *file, int nc, long *chars, int size, uchar *bits, int **);
extern Subfont *bf(int n, int size, Bitmap *b, int *);
