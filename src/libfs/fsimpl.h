/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

typedef struct Queue Queue;
Queue *_fsqalloc(void);
int _fsqsend(Queue*, void*);
void *_fsqrecv(Queue*);
void _fsqhangup(Queue*);
void *_fsnbqrecv(Queue*);

#include <mux.h>
struct Fsys
{
	char version[20];
	int msize;
	QLock lk;
	int fd;
	int ref;
	Mux mux;
	Fid *root;
	Queue *txq;
	Queue *rxq;
	Fid *freefid;
	int nextfid;
};

struct Fid
{
	int fid;
	int mode;
	Fid *next;
	QLock lk;
	Fsys *fs;
	Qid qid;
	vlong offset;
};

void _fsdecref(Fsys*);
void _fsputfid(Fid*);
Fid *_fsgetfid(Fsys*);

