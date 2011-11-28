typedef struct Imap Imap;
#pragma incomplete Imap

void		imapcheckbox(Imap *z, Box *b);
Imap*		imapconnect(char *server, int mode, char *root, char *user);
int		imapcopylist(Imap *z, char *nbox, Msg **m, uint nm);
void		imapfetchraw(Imap *z, Part *p);
void		imapfetchrawbody(Imap *z, Part *p);
void		imapfetchrawheader(Imap *z, Part *p);
void		imapfetchrawmime(Imap *z, Part *p);
int		imapflaglist(Imap *z, int op, int flag, Msg **m, uint nm);
void		imaphangup(Imap *z, int ticks);
int		imapremovelist(Imap *z, Msg **m, uint nm);
int		imapsearchbox(Imap *z, Box *b, char *search, Msg ***mm);

extern	int	chattyimap;

enum
{
	Unencrypted,
	Starttls,
	Tls,
	Cmd
};
