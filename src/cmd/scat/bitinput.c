#include	<u.h>
#include	<libc.h>
#include	<bio.h>
#include	"sky.h"

static int hufvals[] = {
	 1,  1,  1,  1,  1,  1,  1,  1,
	 2,  2,  2,  2,  2,  2,  2,  2,
	 4,  4,  4,  4,  4,  4,  4,  4,
	 8,  8,  8,  8,  8,  8,  8,  8,
	 3,  3,  3,  3,  5,  5,  5,  5,
	10, 10, 10, 10, 12, 12, 12, 12,
	15, 15, 15, 15,  6,  6,  7,  7,
	 9,  9, 11, 11, 13, 13,  0, 14,
};

static int huflens[] = {
	3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 6, 6,
};

static	int	buffer;
static	int	bits_to_go;		/* Number of bits still in buffer */

void
start_inputing_bits(void)
{
	bits_to_go = 0;
}

int
input_huffman(Biobuf *infile)
{
	int c;

	if(bits_to_go < 6) {
		c = Bgetc(infile);
		if(c < 0) {
			fprint(2, "input_huffman: unexpected EOF\n");
			exits("format");
		}
		buffer = (buffer<<8) | c;
		bits_to_go += 8;
	}
	c = (buffer >> (bits_to_go-6)) & 0x3f;
	bits_to_go -= huflens[c];
	return hufvals[c];
}

int
input_nybble(Biobuf *infile)
{
	int c;

	if(bits_to_go < 4) {
		c = Bgetc(infile);
		if(c < 0){
			fprint(2, "input_nybble: unexpected EOF\n");
			exits("format");
		}
		buffer = (buffer<<8) | c;
		bits_to_go += 8;
	}

	/*
	 * pick off the first 4 bits
	 */
	bits_to_go -= 4;
	return (buffer>>bits_to_go) & 0x0f;
}
