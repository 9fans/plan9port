#include <u.h>
#include <libc.h>
#include <draw.h>

float c1 = 1.402;
float c2 = 0.34414;
float c3 = 0.71414;
float c4 = 1.772;


int
closest(int Y, int Cb, int Cr)
{
	double r, g, b;
	double diff, min;
	int rgb, R, G, B, v, i;
	int y1, cb1, cr1;

	Cb -= 128;
	Cr -= 128;
	r = Y+c1*Cr;
	g = Y-c2*Cb-c3*Cr;
	b = Y+c4*Cb;

//print("YCbCr: %d %d %d, RGB: %g %g %g\n", Y, Cb, Cr, r, g, b);

	min = 1000000.;
	v = 1000;
	for(i=0; i<256; i++){
		rgb =  cmap2rgb(i);
		R = (rgb >> 16) & 0xFF;
		G = (rgb >> 8) & 0xFF;
		B = (rgb >> 0) & 0xFF;
		diff = (R-r)*(R-r) + (G-g)*(G-g) + (B-b)*(B-b);
//		y1 = 0.5870*G + 0.114*B + 0.299*R;
//		cb1 = (B-y1)/1.772;
//		cr1 = (R-y1)/1.402;
		if(diff < min){
//			if(Y==0 && y1!=0)
//				continue;
//			if(Y==256-16 && y1<256-16)
//				continue;
//			if(Cb==0 && cb1!=0)
//				continue;
//			if(Cb==256-16 && cb1<256-16)
//				continue;
//			if(Cr==0 && cr1!=0)
//				continue;
//			if(Cr==256-16 && cr1<256-16)
//				continue;
//print("%d %d %d\n", R, G, B);
			min = diff;
			v = i;
		}
	}
	if(v > 255)
		abort();
	return v;
}

#define 	SHIFT	5
#define	INC		(1<<SHIFT)

typedef struct Color Color;

struct Color
{
	int		col;
	Color	*next;
};

Color	*col[INC*INC*INC];

void
add(int c, int y, int cb, int cr)
{
	Color *cp;

	y >>= 8-SHIFT;
	cb >>= 8-SHIFT;
	cr >>= 8-SHIFT;
	cp = col[cr+INC*(cb+INC*y)];
	while(cp != nil){
		if(cp->col == c)
			return;
		cp = cp->next;
	}
	cp = malloc(sizeof(Color));
	cp->col = c;
	cp->next = col[cr+INC*(cb+INC*y)];
	col[cr+INC*(cb+INC*y)] = cp;
}

void
main(void)
{
	int y, cb, cr, n;
	Color *cp;

	for(y=0; y<256; y++){
		for(cb=0; cb<256; cb++)
			for(cr=0;cr<256;cr++)
				add(closest(y, cb, cr), y, cb, cr);
		fprint(2, "%d done\n", y);
	}
	for(y=0; y<INC*INC*INC; y++){
		n = 0;
		cp = col[y];
		while(cp != nil){
			n++;
			cp = cp->next;
		}
		cp = col[y];
		while(cp != nil){
			n++;
			print("%d ", cp->col);
			cp = cp->next;
		}
		print("\n");
	}
}
