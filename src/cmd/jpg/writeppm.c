#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <bio.h>
#include "imagefile.h"

#define	MAXLINE	70

/*
 * Write data
 */
static
char*
writedata(Biobuf *fd, Image *image, Memimage *memimage)
{
	char *err;
	uchar *data;
	int i, x, y, ndata, depth, col, pix, xmask, pmask;
	ulong chan;
	Rectangle r;

	if(memimage != nil){
		r = memimage->r;
		depth = memimage->depth;
		chan = memimage->chan;
	}else{
		r = image->r;
		depth = image->depth;
		chan = image->chan;
	}

	/*
	 * Read image data into memory
	 * potentially one extra byte on each end of each scan line
	 */
	ndata = Dy(r)*(2+Dx(r)*depth/8);
	data = malloc(ndata);
	if(data == nil)
		return "WritePPM: malloc failed";
	if(memimage != nil)
		ndata = unloadmemimage(memimage, r, data, ndata);
	else
		ndata = unloadimage(image, r, data, ndata);
	if(ndata < 0){
		err = malloc(ERRMAX);
		if(err == nil)
			return "WritePPM: malloc failed";
		snprint(err, ERRMAX, "WriteGIF: %r");
		free(data);
		return err;
	}

	/* Encode and emit the data */
	col = 0;
	switch(chan){
	case GREY1:
	case GREY2:
	case GREY4:
		pmask = (1<<depth)-1;
		xmask = 7>>drawlog2[depth];
		for(y=r.min.y; y<r.max.y; y++){
			i = (y-r.min.y)*bytesperline(r, depth);
			for(x=r.min.x; x<r.max.x; x++){
				pix = (data[i]>>depth*((xmask-x)&xmask))&pmask;
				if(((x+1)&xmask) == 0)
					i++;
				col += Bprint(fd, "%d ", pix);
				if(col >= MAXLINE-(2+1)){
					Bprint(fd, "\n");
					col = 0;
				}else
					col += Bprint(fd, " ");
			}
		}
		break;
	case	GREY8:
		for(i=0; i<ndata; i++){
			col += Bprint(fd, "%d ", data[i]);
			if(col >= MAXLINE-(4+1)){
				Bprint(fd, "\n");
				col = 0;
			}else
				col += Bprint(fd, " ");
		}
		break;
	case RGB24:
		for(i=0; i<ndata; i+=3){
			col += Bprint(fd, "%d %d %d", data[i+2], data[i+1], data[i]);
			if(col >= MAXLINE-(4+4+4+1)){
				Bprint(fd, "\n");
				col = 0;
			}else
				col += Bprint(fd, " ");
		}
		break;
	default:
		return "WritePPM: can't handle channel type";
	}

	return nil;
}

static
char*
writeppm0(Biobuf *fd, Image *image, Memimage *memimage, Rectangle r, int chan, char *comment)
{
	char *err;

	switch(chan){
	case GREY1:
		Bprint(fd, "P1\n");
		break;
	case GREY2:
	case GREY4:
	case	GREY8:
		Bprint(fd, "P2\n");
		break;
	case RGB24:
		Bprint(fd, "P3\n");
		break;
	default:
		return "WritePPM: can't handle channel type";
	}

	if(comment!=nil && comment[0]!='\0'){
		Bprint(fd, "# %s", comment);
		if(comment[strlen(comment)-1] != '\n')
			Bprint(fd, "\n");
	}
	Bprint(fd, "%d %d\n", Dx(r), Dy(r));

	/* maximum pixel value */
	switch(chan){
	case GREY2:
		Bprint(fd, "%d\n", 3);
		break;
	case GREY4:
		Bprint(fd, "%d\n", 15);
		break;
	case	GREY8:
	case RGB24:
		Bprint(fd, "%d\n", 255);
		break;
	}

	err = writedata(fd, image, memimage);

	Bprint(fd, "\n");
	Bflush(fd);
	return err;
}

char*
writeppm(Biobuf *fd, Image *image, char *comment)
{
	return writeppm0(fd, image, nil, image->r, image->chan, comment);
}

char*
memwriteppm(Biobuf *fd, Memimage *memimage, char *comment)
{
	return writeppm0(fd, nil, memimage, memimage->r, memimage->chan, comment);
}
