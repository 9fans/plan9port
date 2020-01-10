#include <u.h>
#include <libc.h>
#include <bio.h>
#include <libsec.h>

#include "iso9660.h"

/* FreeBSD 4.5 installation CD for reference
g% cdsector 17 | xd -b
1+0 records in
1+0 records out
0000000  00 	- volume descriptor type (0)
         43 44 30 30 31     - "CD001"
         01     - volume descriptor version (1)
         45 4c 20 54 4f 52 49 54 4f
0000010  20 53 50 45 43 49 46 49 43 41 54 49 4f 4e 00 00
0000020  00 00 00 00 00 00 00 - 7-39 boot system identifier
"EL TORITO SPECIFICATION"

                              00 00 00 00 00 00 00 00 00
0000030  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0000040  00 00 00 00 00 00 00 - 39-71 boot identifier

boot system use:

absolute pointer to the boot catalog??

                              4d 0c 00 00 00 00 00 00 00
0000050  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0000580  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0000590  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00005a0  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
g% cdsector 3149 | xd -b	# 0x0c4d
1+0 records in
1+0 records out
0000000  01 	- header (1)
            00  - platform id (0 = 0x86)
               00 00 - reserved 0
                     00 00 00 00 00 00 00 00 00 00 00 00
0000010  00 00 00 00 00 00 00 00 00 00 00 00 - id string
                                             aa 55 - checksum
                                                    55 aa - magic

0000020  88    - 88 = bootable
            03   - 3 = 2.88MB diskette
               00 00 - load segment 0 means default 0x7C0
                     00  - system type (byte 5 of boot image)
                        00 - unused (0)
                           01 00 - 512-byte sector count for initial load
                                 4e 0c 00 00 - ptr to virtual disk
                                             00 00 00 00
0000030  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0000040  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0000050  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0000060  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
g% cdsector `{h2d 0c4e} | xd -b
1+0 records in
1+0 records out
0000000  eb 3c 00 00 00 00 00 00 00 00 00 00 02 00 00 00
0000010  00 00 00 00 00 00 00 00 12 00 02 00 00 00 00 00
0000020  00 00 00 00 00 16 1f 66 6a 00 51 50 06 53
                                          31 c0

FREEBSD
0000000  eb 3c 00 00 00 00 00 00 00 00 00 00 02 00 00 00
0000010  00 00 00 00 00 00 00 00 12 00 02 00 00 00 00 00
0000020  00 00 00 00 00 16 1f 66 6a 00 51 50 06 53
                                          31 c0

DOS 5
0000000  eb 3c 90 4d 53 44 4f 53 35 2e 30 00 02 01 01 00
0000010  02 e0 00 40 0b f0 09 00 12 00 02 00 00 00 00 00
0000020  00 00 00 00 00 00 29 6a 2c e0 16 4e 4f 20 4e 41
0000030  4d 45 20 20 20 20 46 41 54 31 32 20 20 20 fa 33
0000040  c0 8e d0 bc 00 7c 16 07 bb 78 00 36 c5 37 1e 56
0000050  16 53 bf 3e 7c b9 0b 00 fc f3 a4 06 1f c6 45 fe
0000060  0f 8b 0e 18 7c 88 4d f9 89 47 02 c7 07 3e 7c fb
0000070  cd 13 72 79 33 c0 39 06 13 7c 74 08 8b 0e 13 7c
0000080  89 0e 20 7c a0 10 7c f7 26 16 7c 03 06 1c 7c 13
0000090  16 1e 7c 03 06 0e 7c 83 d2 00 a3 50 7c 89 16 52
00000a0  7c a3 49 7c 89 16 4b 7c b8 20 00 f7 26 11 7c 8b

NDISK
0000000  eb 3c 90 50 6c 61 6e 39 2e 30 30 00 02 01 01 00
0000010  02 e0 00 40 0b f0 09 00 12 00 02 00 00 00 00 00
0000020  40 0b 00 00 00 00 29 13 00 00 00 43 59 4c 49 4e
0000030  44 52 49 43 41 4c 46 41 54 31 32 20 20 20 fa 31
0000040  c0 8e d0 8e d8 8e c0 bc ec 7b 89 e5 88 56 12 fb
0000050  be ea 7d bf 90 7d ff d7 bf 82 7d ff d7 8b 06 27
0000060  7c 8b 16 29 7c bb 00 7e bf 2c 7d ff d7 bb 10 00

PBSDISK
0000000  eb 3c 90 50 6c 61 6e 39 2e 30 30 00 02 01 01 00
0000010  02 e0 00 40 0b f0 09 00 12 00 02 00 00 00 00 00
0000020  40 0b 00 00 00 00 29 13 00 00 00 43 59 4c 49 4e
0000030  44 52 49 43 41 4c 46 41 54 31 32 20 20 20 fa 31
0000040  c0 8e d0 8e d8 8e c0 bc f8 7b 89 e5 88 56 00 fb
0000050  be f8 7d bf 00 7d ff d7 bf df 7c ff d7 8b 06 27
0000060  7c 8b 16 29 7c bb 00 7e bf 89 7c ff d7 bf fb 7c
*/

void
Cputbootvol(Cdimg *cd)
{
	Cputc(cd, 0x00);
	Cputs(cd, "CD001", 5);
	Cputc(cd, 0x01);
	Cputs(cd, "EL TORITO SPECIFICATION", 2+1+6+1+13);
	Crepeat(cd, 0, 2+16+16+7);
	cd->bootcatptr = Cwoffset(cd);
	Cpadblock(cd);
}

void
Cupdatebootvol(Cdimg *cd)
{
	ulong o;

	o = Cwoffset(cd);
	Cwseek(cd, cd->bootcatptr);
	Cputnl(cd, cd->bootcatblock, 4);
	Cwseek(cd, o);
}

void
Cputbootcat(Cdimg *cd)
{
	cd->bootcatblock = Cwoffset(cd) / Blocksize;
	Cputc(cd, 0x01);
	Cputc(cd, 0x00);
	Cputc(cd, 0x00);
	Cputc(cd, 0x00);
	Crepeat(cd, 0, 12+12);

	/*
	 * either the checksum doesn't include the header word
	 * or it just doesn't matter.
	 */
	Cputc(cd, 0xAA);
	Cputc(cd, 0x55);
	Cputc(cd, 0x55);
	Cputc(cd, 0xAA);

	cd->bootimageptr = Cwoffset(cd);
	Cpadblock(cd);
}

void
Cupdatebootcat(Cdimg *cd)
{
	ulong o;

	if(cd->bootdirec == nil)
		return;

	o = Cwoffset(cd);
	Cwseek(cd, cd->bootimageptr);
	Cputc(cd, 0x88);
	switch(cd->bootdirec->length){
	default:
		fprint(2, "warning: boot image is not 1.44MB or 2.88MB; pretending 1.44MB\n");
	case 1440*1024:
		Cputc(cd, 0x02);	/* 1.44MB disk */
		break;
	case 2880*1024:
		Cputc(cd, 0x03);	/* 2.88MB disk */
		break;
	}
	Cputnl(cd, 0, 2);	/* load segment */
	Cputc(cd, 0);	/* system type */
	Cputc(cd, 0);	/* unused */
	Cputnl(cd, 1, 2);	/* 512-byte sector count for load */
	Cputnl(cd, cd->bootdirec->block, 4);	/* ptr to disk image */
	Cwseek(cd, o);
}

void
findbootimage(Cdimg *cd, Direc *root)
{
	Direc *d;

	d = walkdirec(root, cd->bootimage);
	if(d == nil){
		fprint(2, "warning: did not encounter boot image\n");
		return;
	}

	cd->bootdirec = d;
}
