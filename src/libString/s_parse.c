#include <u.h>
#include <libc.h>
#include "libString.h"

#undef isspace
#define isspace(c) ((c)==' ' || (c)=='\t' || (c)=='\n')

/* Get the next field from a String.  The field is delimited by white space,
 * single or double quotes.
 */
String *
s_parse(String *from, String *to)
{
	if (*from->ptr == '\0')
		return 0;
	if (to == 0)
		to = s_new();
	if (*from->ptr == '\'') {
		from->ptr++;
		for (;*from->ptr != '\'' && *from->ptr != '\0'; from->ptr++)
			s_putc(to, *from->ptr);
		if (*from->ptr == '\'')
			from->ptr++;
	} else if (*from->ptr == '"') {
		from->ptr++;
		for (;*from->ptr != '"' && *from->ptr != '\0'; from->ptr++)
			s_putc(to, *from->ptr);
		if (*from->ptr == '"')
			from->ptr++;
	} else {
		for (;!isspace(*from->ptr) && *from->ptr != '\0'; from->ptr++)
			s_putc(to, *from->ptr);
	}
	s_terminate(to);

	/* crunch trailing white */
	while(isspace(*from->ptr))
		from->ptr++;

	return to;
}
