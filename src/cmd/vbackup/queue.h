enum
{
	MAXQ = 256
};

typedef struct Queue Queue;
struct Queue
{
	struct {
		Block *db;
		u32int bno;
	} el[MAXQ];
	int ri, wi, nel, closed;

	QLock lk;
	Rendez r;
};

Queue	*qalloc(void);
void	qclose(Queue*);
Block	*qread(Queue*, u32int*);
void	qwrite(Queue*, Block*, u32int);
void	qfree(Queue*);
