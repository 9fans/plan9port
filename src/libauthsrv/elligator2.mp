#elligator2:
#	curve a*x^2+y^2==1+d*x^2*y^2
#	input r0
#	n is any non-square
#
elligator2(p,a,d, n, r0, X,Y,Z,T) mod(p) {
	r = n*r0*r0;
	D = (d*r+a-d)*(d*r-a*r-d);
	N = (r+1)*(a-2*d);
	ND = N*D;
	if(ND == 0) {
		c = 1;
		e = 0;
	} else {
		e = msqrt(ND, p);
		if(e != 0) {
			c = 1;
			e = 1/e;
		} else {
			c = -1;
			e = n*r0*misqrt(n*ND, p);
		}
	}
	s = c*N*e;
	t = -c*N*(r-1)*((a-2*d)*e)^2-1;
	X = 2*s*t;
	Y = (1-a*s*s)*(1+a*s*s);
	Z = (1+a*s*s)*t;
	T = (2*s)*(1-a*s*s);
}
