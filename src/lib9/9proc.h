enum
{
	NPRIV = 16,
	RENDHASH = 33,
};

typedef struct Uproc Uproc;
struct Uproc
{
	int pid;
	int state;
	void *priv[NPRIV];
	ulong rendval;
	ulong rendtag;
	Uproc *rendhash;
	p9jmp_buf notejb;
};

extern Uproc *_p9uproc(int);
extern void _p9uprocdie(void);
