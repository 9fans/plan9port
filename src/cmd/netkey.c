#include <u.h>
#include <libc.h>
#include <libsec.h>
#include <authsrv.h>

void
usage(void)
{
	fprint(2, "usage: netkey\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	char *chal, *pass, buf[32], key[DESKEYLEN];
	char *s;
	int n;

	ARGBEGIN{
	default:
		usage();
	}ARGEND
	if(argc)
		usage();

	s = getenv("service");
	if(s && strcmp(s, "cpu") == 0){
		fprint(2, "netkey must not be run on the cpu server\n");
		exits("boofhead");
	}

	pass = readcons("password", nil, 1);
	if(pass == nil)
		sysfatal("reading password: %r");
	passtokey(key, pass);

	for(;;){
		chal = readcons("challenge", nil, 0);
		if(chal == nil || *chal == 0)
			exits(nil);
		n = strtol(chal, 0, 10);
		sprint(buf, "%d", n);
		netcrypt(key, buf);
		print("response: %s\n", buf);
	}
}
