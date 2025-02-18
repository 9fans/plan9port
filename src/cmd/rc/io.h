#define	EOF	(-1)

struct io{
	int	fd;
	unsigned char *buf, *bufp, *ebuf;
	io	*next;
};

io *openiofd(int), *openiostr(void), *openiocore(void*, int);
void pchr(io*, int);
int rchr(io*);
char *rstr(io*, char*);
char *closeiostr(io*);
void closeio(io*);
int emptyiobuf(io*);
void flushio(io*);
void pdec(io*, int);
void poct(io*, unsigned);
void pptr(io*, void*);
void pquo(io*, char*);
void pwrd(io*, char*);
void pstr(io*, char*);
void pcmd(io*, tree*);
void pval(io*, word*);
void pfun(io*, void(*)(void));
void pfnc(io*, thread*);
void pfmt(io*, char*, ...);
void vpfmt(io*, char*, va_list);
