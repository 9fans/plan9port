#include "threadimpl.h"

int	_threadnopasser;

#define	NFN		33
#define	ERRLEN	48
typedef struct Note Note;
struct Note
{
	Lock		inuse;
	Proc		*proc;		/* recipient */
	char		s[ERRMAX];	/* arg2 */
};

static Note	notes[128];
static Note	*enotes = notes+nelem(notes);
static int		(*onnote[NFN])(void*, char*);
static int		onnotepid[NFN];
static Lock	onnotelock;

int
threadnotify(int (*f)(void*, char*), int in)
{
	int i, topid;
	int (*from)(void*, char*), (*to)(void*, char*);

	if(in){
		from = 0;
		to = f;
		topid = _threadgetproc()->pid;
	}else{
		from = f;
		to = 0;
		topid = 0;
	}
	lock(&onnotelock);
	for(i=0; i<NFN; i++)
		if(onnote[i]==from){
			onnote[i] = to;
			onnotepid[i] = topid;
			break;
		}
	unlock(&onnotelock);
	return i<NFN;
}

static void
delayednotes(Proc *p, void *v)
{
	int i;
	Note *n;
	int (*fn)(void*, char*);

	if(!p->pending)
		return;

	p->pending = 0;
	for(n=notes; n<enotes; n++){
		if(n->proc == p){
			for(i=0; i<NFN; i++){
				if(onnotepid[i]!=p->pid || (fn = onnote[i])==0)
					continue;
				if((*fn)(v, n->s))
					break;
			}
			if(i==NFN){
				_threaddebug(DBGNOTE, "Unhandled note %s, proc %p\n", n->s, p);
				if(strcmp(n->s, "sys: child") == 0)
					noted(NCONT);
				fprint(2, "unhandled note %s, pid %d\n", n->s, p->pid);
				if(v != nil)
					noted(NDFLT);
				else if(strncmp(n->s, "sys:", 4)==0)
					abort();
				threadexitsall(n->s);
			}
			n->proc = nil;
			unlock(&n->inuse);
		}
	}
}

void
_threadnote(void *v, char *s)
{
	Proc *p;
	Note *n;

	_threaddebug(DBGNOTE, "Got note %s", s);
	if(strncmp(s, "sys:", 4) == 0)
		noted(NDFLT);

//	if(_threadexitsallstatus){
//		_threaddebug(DBGNOTE, "Threadexitsallstatus = '%s'\n", _threadexitsallstatus);
//		_exits(_threadexitsallstatus);
//	}

	if(strcmp(s, "threadint")==0 || strcmp(s, "interrupt")==0)
		noted(NCONT);

	p = _threadgetproc();
	if(p == nil)
		noted(NDFLT);

	for(n=notes; n<enotes; n++)
		if(canlock(&n->inuse))
			break;
	if(n==enotes)
		sysfatal("libthread: too many delayed notes");
	utfecpy(n->s, n->s+ERRMAX, s);
	n->proc = p;
	p->pending = 1;
	if(!p->splhi)
		delayednotes(p, v);
	noted(NCONT);
}

int
_procsplhi(void)
{
	int s;
	Proc *p;

	p = _threadgetproc();
	s = p->splhi;
	p->splhi = 1;
	return s;
}

void
_procsplx(int s)
{
	Proc *p;

	p = _threadgetproc();
	p->splhi = s;
	if(s)
		return;
/*
	if(p->pending)
		delayednotes(p, nil);
*/
}

