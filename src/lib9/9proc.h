enum
{
	NPRIV = 16,
	RENDHASH = 33,
	PIDHASH = 33,
};

typedef struct Uproc Uproc;
struct Uproc
{
	Uproc *next;
	int pid;
	int pipe[2];
	int state;
	void *priv[NPRIV];
	ulong rendval;
	ulong rendtag;
	Uproc *rendhash;
	p9jmp_buf notejb;
};

extern Uproc *_p9uproc(void);
extern void _p9uprocdie(void);
