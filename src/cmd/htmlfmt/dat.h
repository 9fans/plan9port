typedef struct Bytes Bytes;
typedef struct URLwin URLwin;

enum
{
	STACK		= 8192,
	EVENTSIZE	= 256,
};

struct Bytes
{
	uchar	*b;
	long		n;
	long		nalloc;
};

struct URLwin
{
	int		infd;
	int		outfd;
	int		type;

	char		*url;
	Item		*items;
	Docinfo	*docinfo;
};

extern	char*	url;
extern	int		aflag;
extern	int		width;
extern	int		defcharset;

extern	char*	loadhtml(int);

extern	char*	readfile(char*, char*, int*);
extern	int	charset(char*);
extern	void*	emalloc(ulong);
extern	char*	estrdup(char*);
extern	char*	estrstrdup(char*, char*);
extern	char*	egrow(char*, char*, char*);
extern	char*	eappend(char*, char*, char*);
extern	void		error(char*, ...);

extern	void		growbytes(Bytes*, char*, long);

extern	void		rendertext(URLwin*, Bytes*);
extern	void		rerender(URLwin*);
extern	void		freeurlwin(URLwin*);

#pragma	varargck	argpos	error	1
