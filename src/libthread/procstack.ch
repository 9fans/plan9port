static int fforkstacksize = 16384;

typedef struct Stack Stack;
struct Stack
{
	Stack *next;
	Stack *fnext;
	int pid;
};

static Lock stacklock;
static Stack *freestacks;
static Stack *allstacks;
static int stackmallocs;
static void gc(void);

static void*
mallocstack(void)
{
	Stack *p;

	lock(&stacklock);
top:
	p = freestacks;
	if(p)
		freestacks = p->fnext;
	else{
		if(stackmallocs++%1 == 0)
			gc();
		if(freestacks)
			goto top;
		p = malloc(fforkstacksize);
		p->next = allstacks;
		allstacks = p;
	}
	if(p)
		p->pid = 1;
	unlock(&stacklock);
	return p;
}

static void
gc(void)
{
	Stack *p;

	for(p=allstacks; p; p=p->next){
		if(p->pid > 1 && procexited(p->pid)){
			if(0) fprint(2, "reclaim stack from %d\n", p->pid);
			p->pid = 0;
		}
		if(p->pid == 0){
			p->fnext = freestacks;
			freestacks = p;
		}
	}
}

static void
freestack(void *v)
{
	Stack *p;

	p = v;
	if(p == nil)
		return;
	lock(&stacklock);
	p->fnext = freestacks;
	p->pid = 0;
	freestacks = p;
	unlock(&stacklock);
	return;
}


