int
_schedfork(Proc *p)
{
	return ffork(RFMEM|RFNOWAIT, _schedinit, p);
}
