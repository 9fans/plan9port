/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <thread.h>

typedef struct Queue Queue;
Queue *_fsqalloc(void);
int _fsqsend(Queue*, void*);
void *_fsqrecv(Queue*);
void _fsqhangup(Queue*);
void *_fsnbqrecv(Queue*);

#include <mux.h>
struct CFsys
{
	char version[20];
	int msize;
	QLock lk;
	int fd;
	int ref;
	Mux mux;
	CFid *root;
	Queue *txq;
	Queue *rxq;
	CFid *freefid;
	int nextfid;
	Ioproc *iorecv;
	Ioproc *iosend;
};

struct CFid
{
	int fid;
	int mode;
	CFid *next;
	QLock lk;
	CFsys *fs;
	Qid qid;
	vlong offset;
};

void _fsdecref(CFsys*);
void _fsputfid(CFid*);
CFid *_fsgetfid(CFsys*);

int	_fsrpc(CFsys*, Fcall*, Fcall*, void**);
CFid *_fswalk(CFid*, char*);
