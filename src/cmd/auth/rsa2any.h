
DSApriv*	getdsakey(int argc, char **argv, int needprivate, Attr **pa);
RSApriv*	getkey(int argc, char **argv, int needprivate, Attr **pa);
uchar*		put4(uchar *p, uint n);
uchar*		putmp2(uchar *p, mpint *b);
uchar*		putn(uchar *p, void *v, uint n);
uchar*		putstr(uchar *p, char *s);
