#define QLock DQLock
#define qlock dqlock
#define qunlock dqunlock

typedef struct QLock QLock;

struct QLock
{
	pthread_mutex_t m;
	int init;
};

void	qlock(QLock*);
void	qunlock(QLock*);
