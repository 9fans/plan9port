typedef struct Queue Queue;
Queue *_vtqalloc(void);
int _vtqsend(Queue*, void*);
void *_vtqrecv(Queue*);
void _vtqhangup(Queue*);
void *_vtnbqrecv(Queue*);
void _vtqdecref(Queue*);
Queue *_vtqincref(Queue*);
