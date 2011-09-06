#define QLock DQLock
#define qlock dqlock
#define qunlock dqunlock
#define Rendez DRendez
#define rsleep drsleep
#define rwakeup drwakeup

typedef struct QLock QLock;
typedef struct Rendez Rendez;

struct QLock
{
	pthread_mutex_t m;
	int init;
};

struct Rendez
{
	QLock *l;
	pthread_cond_t c;
	int init;
};

void	qlock(QLock*);
void	qunlock(QLock*);
void rsleep(Rendez*);
int rwakeup(Rendez*);	/* BUG: always returns 0 */
