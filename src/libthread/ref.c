#include "threadimpl.h"

void
incref(Ref *r)
{
	_xinc(&r->ref);
}

long
decref(Ref *r)
{
	return _xdec(&r->ref);
}
