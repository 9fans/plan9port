#include <u.h>
#include <libc.h>
#include <venti.h>

Packet*
vtfcallpack(VtFcall *f)
{
	uchar buf[4];
	Packet *p;

	p = packetalloc();

	buf[0] = f->msgtype;
	buf[1] = f->tag;
	packetappend(p, buf, 2);

	switch(f->msgtype){
	default:
		werrstr("vtfcallpack: unknown packet type %d", f->msgtype);
		goto Err;

	case VtRerror:
		if(vtputstring(p, f->error) < 0)
			goto Err;
		break;

	case VtTping:
		break;

	case VtRping:
		break;

	case VtThello:
		if(vtputstring(p, f->version) < 0
		|| vtputstring(p, f->uid) < 0)
			goto Err;
		buf[0] = f->strength;
		buf[1] = f->ncrypto;
		packetappend(p, buf, 2);
		packetappend(p, f->crypto, f->ncrypto);
		buf[0] = f->ncodec;
		packetappend(p, buf, 1);
		packetappend(p, f->codec, f->ncodec);
		break;

	case VtRhello:
		if(vtputstring(p, f->sid) < 0)
			goto Err;
		buf[0] = f->rcrypto;
		buf[1] = f->rcodec;
		packetappend(p, buf, 2);
		break;

	case VtTgoodbye:
		break;

	case VtTread:
		packetappend(p, f->score, VtScoreSize);
		buf[0] = vttodisktype(f->blocktype);
		if(~buf[0] == 0)
			goto Err;
		buf[1] = 0;
		buf[2] = f->count >> 8;
		buf[3] = f->count;
		packetappend(p, buf, 4);
		break;

	case VtRread:
		packetconcat(p, f->data);
		break;

	case VtTwrite:
		buf[0] = vttodisktype(f->blocktype);
		if(~buf[0] == 0)
			goto Err;
		buf[1] = 0;
		buf[2] = 0;
		buf[3] = 0;
		packetappend(p, buf, 4);
		packetconcat(p, f->data);
		break;

	case VtRwrite:
		packetappend(p, f->score, VtScoreSize);
		break;

	case VtTsync:
		break;

	case VtRsync:
		break;
	}

	return p;

Err:
	packetfree(p);
	return nil;
}

int
vtfcallunpack(VtFcall *f, Packet *p)
{
	uchar buf[4];

	memset(f, 0, sizeof *f);

	if(packetconsume(p, buf, 2) < 0)
		return -1;

	f->msgtype = buf[0];
	f->tag = buf[1];

	switch(f->msgtype){
	default:
		werrstr("vtfcallunpack: unknown bad packet type %d", f->msgtype);
		vtfcallclear(f);
		return -1;

	case VtRerror:
		if(vtgetstring(p, &f->error) < 0)
			goto Err;
		break;

	case VtTping:
		break;

	case VtRping:
		break;

	case VtThello:
		if(vtgetstring(p, &f->version) < 0
		|| vtgetstring(p, &f->uid) < 0
		|| packetconsume(p, buf, 2) < 0)
			goto Err;
		f->strength = buf[0];
		f->ncrypto = buf[1];
		if(f->ncrypto){
			f->crypto = vtmalloc(f->ncrypto);
			if(packetconsume(p, buf, f->ncrypto) < 0)
				goto Err;
		}
		if(packetconsume(p, buf, 1) < 0)
			goto Err;
		f->ncodec = buf[0];
		if(f->ncodec){
			f->codec = vtmalloc(f->ncodec);
			if(packetconsume(p, buf, f->ncodec) < 0)
				goto Err;
		}
		break;

	case VtRhello:
		if(vtgetstring(p, &f->sid) < 0
		|| packetconsume(p, buf, 2) < 0)
			goto Err;
		f->rcrypto = buf[0];
		f->rcodec = buf[1];
		break;

	case VtTgoodbye:
		break;

	case VtTread:
		if(packetconsume(p, f->score, VtScoreSize) < 0
		|| packetconsume(p, buf, 4) < 0)
			goto Err;
		f->blocktype = vtfromdisktype(buf[0]);
		if(~f->blocktype == 0)
			goto Err;
		f->count = (buf[2] << 8) | buf[3];
		break;

	case VtRread:
		f->data = packetalloc();
		packetconcat(f->data, p);
		break;

	case VtTwrite:
		if(packetconsume(p, buf, 4) < 0)
			goto Err;
		f->blocktype = vtfromdisktype(buf[0]);
		if(~f->blocktype == 0)
			goto Err;
		f->data = packetalloc();
		packetconcat(f->data, p);
		break;

	case VtRwrite:
		if(packetconsume(p, f->score, VtScoreSize) < 0)
			goto Err;
		break;

	case VtTsync:
		break;

	case VtRsync:
		break;
	}

	if(packetsize(p) != 0)
		goto Err;

	return 0;

Err:
	werrstr("bad packet");
	vtfcallclear(f);
	return -1;
}

void
vtfcallclear(VtFcall *f)
{
	vtfree(f->error);
	f->error = nil;
	vtfree(f->uid);
	f->uid = nil;
	vtfree(f->sid);
	f->sid = nil;
	vtfree(f->version);
	f->version = nil;
	vtfree(f->crypto);
	f->crypto = nil;
	vtfree(f->codec);
	f->codec = nil;
	vtfree(f->auth);
	f->auth = nil;
	packetfree(f->data);
	f->data = nil;
}
