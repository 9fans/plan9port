	mypid = getpid();

	/*
	 * signal others.
	 * copying all the pids first avoids other thread's
	 * teardown procedures getting in the way.
	 */
	lock(&_threadpq.lock);
	npid = 0;
	for(p=_threadpq.head; p; p=p->next)
		npid++;
	pid = _threadmalloc(npid*sizeof(pid[0]), 0);
	npid = 0;
	for(p = _threadpq.head; p; p=p->next)
		pid[npid++] = p->pid;
	unlock(&_threadpq.lock);
	for(i=0; i<npid; i++){
		_threaddebug(DBGSCHED, "threadexitsall kill %d", pid[i]);
		if(pid[i]==0 || pid[i]==-1)
			fprint(2, "bad pid in threadexitsall: %d\n", pid[i]);
		else if(pid[i] != mypid){
			kill(pid[i], SIGTERM);
		}
	}

