#include <u.h>
#include <libc.h>
#include <venti.h>

int
vtsrvhello(VtConn *z)
{
	VtFcall tx, rx;
	Packet *p;

	if((p = vtrecv(z)) == nil)
		return 0;

	if(vtfcallunpack(&tx, p) < 0){
		packetfree(p);
		return 0;
	}
	packetfree(p);

	if(tx.type != VtThello){
		vtfcallclear(&tx);
		werrstr("bad packet type %d; want Thello %d", tx.type, VtThello);
		return 0;
	}
	if(tx.tag != 0){
		vtfcallclear(&tx);
		werrstr("bad tag in hello");
		return 0;
	}
	if(strcmp(tx.version, z->version) != 0){
		vtfcallclear(&tx);
		werrstr("bad version in hello");
		return 0;
	}
	vtfree(z->uid);
	z->uid = tx.uid;
	tx.uid = nil;
	vtfcallclear(&tx);

	memset(&rx, 0, sizeof rx);
	rx.type = VtRhello;
	rx.tag = tx.tag;
	rx.sid = "anonymous";
	if((p = vtfcallpack(&rx)) == nil)
		return 0;
	if(vtsend(z, p) < 0)
		return 0;

	return 1;
}
