#include <u.h>
#include <libc.h>
#include <ctype.h>

/*
 *  make an address, add the defaults
 */
char *
netmkaddr(char *linear, char *defnet, char *defsrv)
{
	static char addr[256];
	char *cp;

	/*
	 *  dump network name
	 */
	cp = strchr(linear, '!');
	if(cp == 0){
		if(defnet == 0)
			defnet = "net";
		/* allow unix sockets to omit unix! prefix */
		if(access(linear, 0) >= 0){
			snprint(addr, sizeof(addr), "unix!%s", linear);
			return addr;
		}
		/* allow host:service in deference to Unix convention */
		if((cp = strchr(linear, ':')) != nil){
			snprint(addr, sizeof(addr), "%s!%.*s!%s",
				defnet, utfnlen(linear, cp-linear),
				linear, cp+1);
			return addr;
		}
		if(defsrv)
			snprint(addr, sizeof(addr), "%s!%s!%s",
				defnet, linear, defsrv);
		else
			snprint(addr, sizeof(addr), "%s!%s", defnet, linear);
		return addr;
	}

	/*
	 *  if there is already a service, use it
	 */
	cp = strchr(cp+1, '!');
	if(cp)
		return linear;

	/*
	 * if the network is unix, no service
	 */
	if(strncmp(linear, "unix!", 5) == 0)
		return linear;

	/*
	 *  add default service
	 */
	if(defsrv == 0)
		return linear;

	snprint(addr, sizeof(addr), "%s!%s", linear, defsrv);
	return addr;
}
