
#define BMP_RGB      	0
#define BMP_RLE8     	1
#define BMP_RLE4     	2
#define BMP_BITFIELDS	3

typedef struct {
	uchar red;
	uchar green;
	uchar blue;
	uchar alpha;
} Rgb;

typedef struct {
        short	type;
        long	size;
        short	reserved1;
        short	reserved2;
        long	offbits;
} Filehdr;

typedef struct {
	long	size;		/* Size of the Bitmap-file */
	long	lReserved;	/* Reserved */
	long	dataoff;	/* Picture data location */
	long	hsize;		/* Header-Size */
	long	width;		/* Picture width (pixels) */
	long	height;		/* Picture height (pixels) */
	short	planes;		/* Planes (must be 1) */
	short	bpp;		/* Bits per pixel (1, 4, 8 or 24) */
	long	compression;	/* Compression mode */
	long	imagesize;	/* Image size (bytes) */
	long	hres;		/* Horizontal Resolution (pels/meter) */
	long	vres;		/* Vertical Resolution (pels/meter) */
	long	colours;	/* Used Colours (Col-Table index) */
	long	impcolours;	/* Important colours (Col-Table index) */
} Infohdr;
