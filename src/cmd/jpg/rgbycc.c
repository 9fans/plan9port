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
		y1 = 0.5870*G + 0.114*B + 0.299*R;
		cb1 = (B-y1)/1.772;
		cr1 = (R-y1)/1.402;
		if(diff < min){
//			if(Y==0 && y1!=0)
//				continue;
			if(Y==256-16 && y1<256-16)
				continue;
//			if(Cb==0 && cb1!=0)
//				continue;
			if(Cb==256-16 && cb1<256-16)
				continue;
//			if(Cr==0 && cr1!=0)
//				continue;
			if(Cr==256-16 && cr1<256-16)
				continue;
//print("%d %d %d\n", R, G, B);
			min = diff;
			v = i;
		}
	}
	if(v > 255)
		abort();
	return v;
}

void
main(int argc, char *argv[])
{
	int i, rgb;
	int r, g, b;
	double Y, Cr, Cb;
	int y, cb, cr;
	uchar close[16*16*16];

//print("%d\n", closest(atoi(argv[1]), atoi(argv[2]), atoi(argv[3])));
//exits("X");

	/* ycbcrmap */
	print("uint ycbcrmap[256] = {\n");
	for(i=0; i<256; i++){
		if(i%8 == 0)
			print("\t");
		rgb = cmap2rgb(i);
		r = (rgb>>16) & 0xFF;
		g = (rgb>>8) & 0xFF;
		b = (rgb>>0) & 0xFF;
		Y = 0.5870*g + 0.114*b + 0.299*r;
		Cr = (r-Y)/1.402 + 128.;
		Cb = (b-Y)/1.772 + 128.;
		if(Y<0. || Y>=256. || Cr<0. || Cr>=256. || Cb<0. || Cb>=256.)
			print("bad at %d: %d %d %d; %g %g %g\n", i, r, g, b, Y, Cb, Cr);
		r = Y;
		g = Cb;
		b = Cr;
		print("0x%.6ulX, ", (r<<16) | (g<<8) | b);
		if(i%8 == 7)
			print("\n");
	}
	print("};\n\n");

	/* closestycbcr */
	print("uchar closestycbcr[16*16*16] = {\n");
	for(y=0; y<256; y+=16)
	for(cb=0; cb<256; cb+=16)
	for(cr=0; cr<256; cr+=16)
		close[(cr/16)+16*((cb/16)+16*(y/16))] = closest(y, cb, cr);
if(0){
	/*weird: set white for nearly white */
	for(cb=128-32; cb<=128+32; cb+=16)
	for(cr=128-32; cr<=128+32; cr+=16)
		close[(cr/16)+16*((cb/16)+16*(255/16))] = 0;
	/*weird: set black for nearly black */
	for(cb=128-32; cb<=128+32; cb+=16)
	for(cr=128-32; cr<=128+32; cr+=16)
		close[(cr/16)+16*((cb/16)+16*(0/16))] = 255;
}
	for(i=0; i<16*16*16; i++){
		if(i%16 == 0)
			print("\t");
		print("%d,", close[i]);
		if(i%16 == 15)
			print("\n");
	}
	print("};\n\n");
	exits(nil);
}
