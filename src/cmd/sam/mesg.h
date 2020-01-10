/* VERSION 1 introduces plumbing
	2 increases SNARFSIZE from 4096 to 32000
 */
#define	VERSION	2

#define	TBLOCKSIZE 512		  /* largest piece of text sent to terminal */
#define	DATASIZE  (UTFmax*TBLOCKSIZE+30) /* ... including protocol header stuff */
#define	SNARFSIZE 32000		/* maximum length of exchanged snarf buffer, must fit in 15 bits */
/*
 * Messages originating at the terminal
 */
typedef enum Tmesg
{
	Tversion,	/* version */
	Tstartcmdfile,	/* terminal just opened command frame */
	Tcheck,		/* ask host to poke with Hcheck */
	Trequest,	/* request data to fill a hole */
	Torigin,	/* gimme an Horigin near here */
	Tstartfile,	/* terminal just opened a file's frame */
	Tworkfile,	/* set file to which commands apply */
	Ttype,		/* add some characters, but terminal already knows */
	Tcut,
	Tpaste,
	Tsnarf,
	Tstartnewfile,	/* terminal just opened a new frame */
	Twrite,		/* write file */
	Tclose,		/* terminal requests file close; check mod. status */
	Tlook,		/* search for literal current text */
	Tsearch,	/* search for last regular expression */
	Tsend,		/* pretend he typed stuff */
	Tdclick,	/* double click */
	Tstartsnarf,	/* initiate snarf buffer exchange */
	Tsetsnarf,	/* remember string in snarf buffer */
	Tack,		/* acknowledge Hack */
	Texit,		/* exit */
	Tplumb,		/* send plumb message */
	TMAX
}Tmesg;
/*
 * Messages originating at the host
 */
typedef enum Hmesg
{
	Hversion,	/* version */
	Hbindname,	/* attach name[0] to text in terminal */
	Hcurrent,	/* make named file the typing file */
	Hnewname,	/* create "" name in menu */
	Hmovname,	/* move file name in menu */
	Hgrow,		/* insert space in rasp */
	Hcheck0,	/* see below */
	Hcheck,		/* ask terminal to check whether it needs more data */
	Hunlock,	/* command is finished; user can do things */
	Hdata,		/* store this data in previously allocated space */
	Horigin,	/* set origin of file/frame in terminal */
	Hunlockfile,	/* unlock file in terminal */
	Hsetdot,	/* set dot in terminal */
	Hgrowdata,	/* Hgrow + Hdata folded together */
	Hmoveto,	/* scrolling, context search, etc. */
	Hclean,		/* named file is now 'clean' */
	Hdirty,		/* named file is now 'dirty' */
	Hcut,		/* remove space from rasp */
	Hsetpat,	/* set remembered regular expression */
	Hdelname,	/* delete file name from menu */
	Hclose,		/* close file and remove from menu */
	Hsetsnarf,	/* remember string in snarf buffer */
	Hsnarflen,	/* report length of implicit snarf */
	Hack,		/* request acknowledgement */
	Hexit,
	Hplumb,		/* return plumb message to terminal - version 1 */
	HMAX
}Hmesg;
typedef struct Header{
	uchar	type;		/* one of the above */
	uchar	count0;		/* low bits of data size */
	uchar	count1;		/* high bits of data size */
	uchar	data[1];	/* variable size */
}Header;

/*
 * File transfer protocol schematic, a la Holzmann
 * #define N	6
 *
 * chan h = [4] of { mtype };
 * chan t = [4] of { mtype };
 *
 * mtype = {	Hgrow, Hdata,
 * 		Hcheck, Hcheck0,
 * 		Trequest, Tcheck,
 * 	};
 *
 * active proctype host()
 * {	byte n;
 *
 * 	do
 * 	:: n <  N -> n++; t!Hgrow
 * 	:: n == N -> n++; t!Hcheck0
 *
 * 	:: h?Trequest -> t!Hdata
 * 	:: h?Tcheck   -> t!Hcheck
 * 	od
 * }
 *
 * active proctype term()
 * {
 * 	do
 * 	:: t?Hgrow   -> h!Trequest
 * 	:: t?Hdata   -> skip
 * 	:: t?Hcheck0 -> h!Tcheck
 * 	:: t?Hcheck  ->
 * 		if
 * 		:: h!Trequest -> progress: h!Tcheck
 * 		:: break
 * 		fi
 * 	od;
 * 	printf("term exits\n")
 * }
 *
 * From: gerard@research.bell-labs.com
 * Date: Tue Jul 17 13:47:23 EDT 2001
 * To: rob@research.bell-labs.com
 *
 * spin -c 	(or -a) spec
 * pcc -DNP -o pan pan.c
 * pan -l
 *
 * proves that there are no non-progress cycles
 * (infinite executions *not* passing through
 * the statement marked with a label starting
 * with the prefix "progress")
 *
 */
