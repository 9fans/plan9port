#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <String.h>
#include "message.h"

Message *root;

void
prindent(int i)
{
	for(; i > 0; i--)
		print(" ");
}

void
prstring(int indent, char *tag, String *s)
{
	if(s == nil)
		return;
	prindent(indent+1);
	print("%s %s\n", tag, s_to_c(s));
}

void
info(int indent, int mno, Message *m)
{
	int i;
	Message *nm;

	prindent(indent);
	print("%d%c %d ", mno, m->allocated?'*':' ', m->end - m->start);
	if(m->unixfrom != nil)
		print("uf %s ", s_to_c(m->unixfrom));
	if(m->unixdate != nil)
		print("ud %s ", s_to_c(m->unixdate));
	print("\n");
	prstring(indent, "from:", m->from822);
	prstring(indent, "sender:", m->sender822);
	prstring(indent, "to:", m->to822);
	prstring(indent, "cc:", m->cc822);
	prstring(indent, "reply-to:", m->replyto822);
	prstring(indent, "subject:", m->subject822);
	prstring(indent, "date:", m->date822);
	prstring(indent, "filename:", m->filename);
	prstring(indent, "type:", m->type);
	prstring(indent, "charset:", m->charset);

	i = 1;
	for(nm = m->part; nm != nil; nm = nm->next){
		info(indent+1, i++, nm);
	}
}


void
main(int argc, char **argv)
{
	char *err;
	char *mboxfile;

	ARGBEGIN{
	}ARGEND;

	if(argc > 0)
		mboxfile = argv[0];
	else
		mboxfile = "./mbox";

	root = newmessage(nil);

	err = readmbox(mboxfile, &root->part);
	if(err != nil){
		fprint(2, "boom: %s\n", err);
		exits(0);
	}

	info(0, 1, root);

	exits(0);
}
