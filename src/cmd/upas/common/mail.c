#include "common.h"

/* format of REMOTE FROM lines */
char *REMFROMRE =
	"^>?From[ \t]+((\".*\")?[^\" \t]+?(\".*\")?[^\" \t]+?)[ \t]+(.+)[ \t]+remote[ \t]+from[ \t]+(.*)\n$";
int REMSENDERMATCH = 1;
int REMDATEMATCH = 4;
int REMSYSMATCH = 5;

/* format of LOCAL FROM lines */
char *FROMRE =
	"^>?From[ \t]+((\".*\")?[^\" \t]+?(\".*\")?[^\" \t]+?)[ \t]+(.+)\n$";
int SENDERMATCH = 1;
int DATEMATCH = 4;

/* output a unix style local header */
int
print_header(Biobuf *fp, char *sender, char *date)
{
	return Bprint(fp, "From %s %s\n", sender, date);
}

/* output a unix style remote header */
int
print_remote_header(Biobuf *fp, char *sender, char *date, char *system)
{
	return Bprint(fp, "From %s %s remote from %s\n", sender, date, system);
}

/* parse a mailbox style header */
int
parse_header(char *line, String *sender, String *date)
{
	if (!IS_HEADER(line))
		return -1;
	line += sizeof("From ") - 1;
	s_restart(sender);
	while(*line==' '||*line=='\t')
		line++;
	if(*line == '"'){
		s_putc(sender, *line++);
		while(*line && *line != '"')
			s_putc(sender, *line++);
		s_putc(sender, *line++);
	} else {
		while(*line && *line != ' ' && *line != '\t')
			s_putc(sender, *line++);
	}
	s_terminate(sender);
	s_restart(date);
	while(*line==' '||*line=='\t')
		line++;
	while(*line)
		s_putc(date, *line++);
	s_terminate(date);
	return 0;
}
