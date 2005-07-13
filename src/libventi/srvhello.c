#include <u.h>
#include <libc.h>
#include <venti.h>

int
vtsrvhello(VtConn *z)
{
	VtFcall tx, rx;
	Packet *p;

	if((p = vtrecv(z)) == nil)
		return -1;

	if(vtfcallunpack(&tx, p) < 0){
		packetfree(p);
		return -1;
	}
	packetfree(p);

	if(tx.msgtype != VtThello){
		vtfcallclear(&tx);
		werrstr("bad packet type %d; want Thello %d", tx.msgtype, VtThello);
		return -1;
	}
	if(tx.tag != 0){
		vtfcallclear(&tx);
		werrstr("bad tag in hello");
		return -1;
	}
	if(strcmp(tx.version, z->version) != 0){
		vtfcallclear(&tx);
		werrstr("bad version in hello");
		return -1;
	}
	vtfree(z->uid);
	z->uid = tx.uid;
	tx.uid = nil;
	vtfcallclear(&tx);

	memset(&rx, 0, sizeof rx);
	rx.msgtype = VtRhello;
	rx.tag = tx.tag;
	rx.sid = "anonymous";
	if((p = vtfcallpack(&rx)) == nil)
		return -1;
	if(vtsend(z, p) < 0)
		return -1;

	return 0;
}
