#include <u.h>
#include <libc.h>
#include <flate.h>

char *
flateerr(int err)
{
	switch(err){
	case FlateOk:
		return "no error";
	case FlateNoMem:
		return "out of memory";
	case FlateInputFail:
		return "input error";
	case FlateOutputFail:
		return "output error";
	case FlateCorrupted:
		return "corrupted data";
	case FlateInternal:
		return "internal error";
	}
	return "unknown error";
}
