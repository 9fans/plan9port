#include <u.h>
#include <ctype.h>

/*
 *  return ndb attribute type of an ip name
 */
char*
ipattr(char *name)
{
	char *p, c;
	int dot = 0;
	int alpha = 0;
	int colon = 0;
	int hex = 0;

	for(p = name; *p; p++){
		c = *p;
		if(isdigit(c))
			continue;
		if(isxdigit(c))
			hex = 1;
		else if(isalpha(c) || c == '-')
			alpha = 1;
		else if(c == '.')
			dot = 1;
		else if(c == ':')
			colon = 1;
		else
			return "sys";
	}

	if(alpha){
		if(dot)
			return "dom";
		else
			return "sys";
	}

	if(colon)
		return "ip";	/* ip v6 */

	if(dot && !hex)
		return "ip";
	else
		return "sys";
}
