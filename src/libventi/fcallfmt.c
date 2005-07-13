#include <u.h>
#include <libc.h>
#include <venti.h>

int
vtfcallfmt(Fmt *f)
{
	VtFcall *t;

	t = va_arg(f->args, VtFcall*);
	if(t == nil){
		fmtprint(f, "<nil fcall>");
		return 0;
	}
	switch(t->msgtype){
	default:
		return fmtprint(f, "%c%d tag %ud", "TR"[t->msgtype&1], t->msgtype>>1, t->tag);
	case VtRerror:
		return fmtprint(f, "Rerror tag %ud error %s", t->tag, t->error);
	case VtTping:
		return fmtprint(f, "Tping tag %ud", t->tag);
	case VtRping:
		return fmtprint(f, "Rping tag %ud", t->tag);
	case VtThello:
		return fmtprint(f, "Thello tag %ud vers %s uid %s strength %d crypto %d:%.*H codec %d:%.*H", t->tag,
			t->version, t->uid, t->strength, t->ncrypto, t->ncrypto, t->crypto,
			t->ncodec, t->ncodec, t->codec);
	case VtRhello:
		return fmtprint(f, "Rhello tag %ud sid %s rcrypto %d rcodec %d", t->tag, t->sid, t->rcrypto, t->rcodec);
	case VtTgoodbye:
		return fmtprint(f, "Tgoodbye tag %ud", t->tag);
	case VtRgoodbye:
		return fmtprint(f, "Rgoodbye tag %ud", t->tag);
	case VtTauth0:
		return fmtprint(f, "Tauth0 tag %ud auth %.*H", t->tag, t->nauth, t->auth);
	case VtRauth0:
		return fmtprint(f, "Rauth0 tag %ud auth %.*H", t->tag, t->nauth, t->auth);
	case VtTauth1:
		return fmtprint(f, "Tauth1 tag %ud auth %.*H", t->tag, t->nauth, t->auth);
	case VtRauth1:
		return fmtprint(f, "Rauth1 tag %ud auth %.*H", t->tag, t->nauth, t->auth);
	case VtTread:
		return fmtprint(f, "Tread tag %ud score %V blocktype %d count %d", t->tag, t->score, t->blocktype, t->count);
	case VtRread:
		return fmtprint(f, "Rread tag %ud count %d", t->tag, packetsize(t->data));
	case VtTwrite:
		return fmtprint(f, "Twrite tag %ud blocktype %d count %d", t->tag, t->blocktype, packetsize(t->data));
	case VtRwrite:
		return fmtprint(f, "Rwrite tag %ud score %V", t->tag, t->score);
	case VtTsync:
		return fmtprint(f, "Tsync tag %ud", t->tag);
	case VtRsync:
		return fmtprint(f, "Rsync tag %ud", t->tag);
	}
}
