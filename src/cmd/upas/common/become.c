#include "common.h"
#include <auth.h>
#include <ndb.h>

/*
 *  become powerless user
 */
int
become(char **cmd, char *who)
{
	int fd;

	USED(cmd);
	if(strcmp(who, "none") == 0) {
		fd = open("#c/user", OWRITE);
		if(fd < 0 || write(fd, "none", strlen("none")) < 0) {
			werrstr("can't become none");
			return -1;
		}
		close(fd);
		// jpc if(newns("none", 0)) {
		// jpc 	werrstr("can't set new namespace");
		// jpc	return -1;
		// jpc }
	}
	return 0;
}

