#include <lib9.h>

int
dec32(uchar *dest, int ndest, char *src, int nsrc)
{
	char *s, *tab;
	uchar *start;
	int i, u[8];

	if(ndest+1 < (5*nsrc+7)/8)
		return -1;
	start = dest;
	tab = "23456789abcdefghijkmnpqrstuvwxyz";
	while(nsrc>=8){
		for(i=0; i<8; i++){
			s = strchr(tab,(int)src[i]);
			u[i] = s ? s-tab : 0;
		}
		*dest++ = (u[0]<<3) | (0x7 & (u[1]>>2));
		*dest++ = ((0x3 & u[1])<<6) | (u[2]<<1) | (0x1 & (u[3]>>4));
		*dest++ = ((0xf & u[3])<<4) | (0xf & (u[4]>>1));
		*dest++ = ((0x1 & u[4])<<7) | (u[5]<<2) | (0x3 & (u[6]>>3));
		*dest++ = ((0x7 & u[6])<<5) | u[7];
		src  += 8;
		nsrc -= 8;
	}
	if(nsrc > 0){
		if(nsrc == 1 || nsrc == 3 || nsrc == 6)
			return -1;
		for(i=0; i<nsrc; i++){
			s = strchr(tab,(int)src[i]);
			u[i] = s ? s-tab : 0;
		}
		*dest++ = (u[0]<<3) | (0x7 & (u[1]>>2));
		if(nsrc == 2)
			goto out;
		*dest++ = ((0x3 & u[1])<<6) | (u[2]<<1) | (0x1 & (u[3]>>4));
		if(nsrc == 4)
			goto out;
		*dest++ = ((0xf & u[3])<<4) | (0xf & (u[4]>>1));
		if(nsrc == 5)
			goto out;
		*dest++ = ((0x1 & u[4])<<7) | (u[5]<<2) | (0x3 & (u[6]>>3));
	}
out:
	return dest-start;
}

int
enc32(char *dest, int ndest, uchar *src, int nsrc)
{
	char *tab, *start;
	int j;

	if(ndest <= (8*nsrc+4)/5 )
		return -1;
	start = dest;
	tab = "23456789abcdefghijkmnpqrstuvwxyz";
	while(nsrc>=5){
		j = (0x1f & (src[0]>>3));
		*dest++ = tab[j];
		j = (0x1c & (src[0]<<2)) | (0x03 & (src[1]>>6));
		*dest++ = tab[j];
		j = (0x1f & (src[1]>>1));
		*dest++ = tab[j];
		j = (0x10 & (src[1]<<4)) | (0x0f & (src[2]>>4));
		*dest++ = tab[j];
		j = (0x1e & (src[2]<<1)) | (0x01 & (src[3]>>7));
		*dest++ = tab[j];
		j = (0x1f & (src[3]>>2));
		*dest++ = tab[j];
		j = (0x18 & (src[3]<<3)) | (0x07 & (src[4]>>5));
		*dest++ = tab[j];
		j = (0x1f & (src[4]));
		*dest++ = tab[j];
		src  += 5;
		nsrc -= 5;
	}
	if(nsrc){
		j = (0x1f & (src[0]>>3));
		*dest++ = tab[j];
		j = (0x1c & (src[0]<<2));
		if(nsrc == 1)
			goto out;
		j |= (0x03 & (src[1]>>6));
		*dest++ = tab[j];
		j = (0x1f & (src[1]>>1));
		if(nsrc == 2)
			goto out;
		*dest++ = tab[j];
		j = (0x10 & (src[1]<<4));
		if(nsrc == 3)
			goto out;
		j |= (0x0f & (src[2]>>4));
		*dest++ = tab[j];
		j = (0x1e & (src[2]<<1));
		if(nsrc == 4)
			goto out;
		j |= (0x01 & (src[3]>>7));
		*dest++ = tab[j];
		j = (0x1f & (src[3]>>2));
		*dest++ = tab[j];
		j = (0x18 & (src[3]<<3));
out:
		*dest++ = tab[j];
	}
	*dest = 0;
	return dest-start;
}
