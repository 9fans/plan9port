#include "os.h"
#include <libsec.h>

static void
ccpolyotk(Chachastate *cs, DigestState *ds)
{
	uchar otk[ChachaBsize];

	memset(ds, 0, sizeof(*ds));
	memset(otk, 0, 32);
	chacha_setblock(cs, 0);
	chacha_encrypt(otk, ChachaBsize, cs);
	poly1305(nil, 0, otk, 32, nil, ds);
}

static void
ccpolypad(uchar *buf, ulong nbuf, DigestState *ds)
{
	static uchar zeros[16];
	ulong npad;

	if(nbuf == 0)
		return;
	poly1305(buf, nbuf, nil, 0, nil, ds);
	npad = nbuf % 16;
	if(npad == 0)
		return;
	poly1305(zeros, 16 - npad, nil, 0, nil, ds);
}

static void
ccpolylen(ulong n, uchar tag[16], DigestState *ds)
{
	uchar info[8];

	info[0] = n;
	info[1] = n >> 8;
	info[2] = n >> 16;
	info[3] = n >> 24;
	info[4] = 0;
	info[5] = 0;
	info[6] = 0;
	info[7] = 0;
	poly1305(info, 8, nil, 0, tag, ds);
}

void
ccpoly_encrypt(uchar *dat, ulong ndat, uchar *aad, ulong naad, uchar tag[16], Chachastate *cs)
{
	DigestState ds;

	ccpolyotk(cs, &ds);
	if(cs->ivwords == 2){
		poly1305(aad, naad, nil, 0, nil, &ds);
		ccpolylen(naad, nil, &ds);
		chacha_encrypt(dat, ndat, cs);
		poly1305(dat, ndat, nil, 0, nil, &ds);
		ccpolylen(ndat, tag, &ds);
	}else{
		ccpolypad(aad, naad, &ds);
		chacha_encrypt(dat, ndat, cs);
		ccpolypad(dat, ndat, &ds);
		ccpolylen(naad, nil, &ds);
		ccpolylen(ndat, tag, &ds);
	}
}

int
ccpoly_decrypt(uchar *dat, ulong ndat, uchar *aad, ulong naad, uchar tag[16], Chachastate *cs)
{
	DigestState ds;
	uchar tmp[16];

	ccpolyotk(cs, &ds);
	if(cs->ivwords == 2){
		poly1305(aad, naad, nil, 0, nil, &ds);
		ccpolylen(naad, nil, &ds);
		poly1305(dat, ndat, nil, 0, nil, &ds);
		ccpolylen(ndat, tmp, &ds);
	}else{
		ccpolypad(aad, naad, &ds);
		ccpolypad(dat, ndat, &ds);
		ccpolylen(naad, nil, &ds);
		ccpolylen(ndat, tmp, &ds);
	}
	if(tsmemcmp(tag, tmp, 16) != 0)
		return -1;
	chacha_encrypt(dat, ndat, cs);
	return 0;
}
