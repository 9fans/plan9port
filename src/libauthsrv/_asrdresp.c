#include <u.h>
#include <libc.h>
#include <authsrv.h>

static char *pbmsg = "AS protocol botch";

int
_asrdresp(int fd, char *buf, int len)
{
	int n;
	char error[64];

	if(read(fd, buf, 1) != 1){
		werrstr(pbmsg);
		return -1;
	}

	n = len;
	switch(buf[0]){
	case AuthOK:
		if(readn(fd, buf, len) != len){
			werrstr(pbmsg);
			return -1;
		}
		break;
	case AuthErr:
		if(readn(fd, error, sizeof error) != sizeof error){
			werrstr(pbmsg);
			return -1;
		}
		error[sizeof error-1] = '\0';
		werrstr("remote: %s", error);
		return -1;
	case AuthOKvar:
		if(readn(fd, error, 5) != 5){
			werrstr(pbmsg);
			return -1;
		}
		error[5] = 0;
		n = atoi(error);
		if(n <= 0 || n > len){
			werrstr(pbmsg);
			return -1;
		}
		memset(buf, 0, len);
		if(readn(fd, buf, n) != n){
			werrstr(pbmsg);
			return -1;
		}
		break;
	default:
		werrstr(pbmsg);
		return -1;
	}
	return n;
}
