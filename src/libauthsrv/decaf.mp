# negate r when n > (p-1)/2
decaf_neg(p, n, r) {
	mod(p) m = -r;
	r = n > (p-1)>>1 ? m : r;
}

#	field F_p
#	curve a*x**2+y**2==1+d*x**2*y**2
#	input X,Y,Z,T (extended coordinates)
decaf_encode(p,a,d, X,Y,Z,T, s) mod(p) {
	r = misqrt((a-d)*(Z+Y)*(Z-Y), p);
	u = (a-d)*r;
	decaf_neg(p, -2*u*Z, r);
	s = u*(r*(a*Z*X-d*Y*T)+Y)/a;
	decaf_neg(p, s, s);
}

#	field F_p
#	curve a*x**2+y**2==1+d*x**2*y**2
#	input s
#	output in extended coordinates
decaf_decode(p,a,d, s, ok,X,Y,Z,T) {
	if(s > (p-1)>>1){
		ok = 0;
	} else mod(p) {
		ss = s^2;
		Z = 1+a*ss;
		u = Z^2 - 4*d*ss;
		v = u*ss;
		if(v == 0)
			ok = 1;
		else {
			ok = msqrt(v, p);
			if(ok != 0){
				v = 1/ok;
				ok = 1;
			}
		}
		if(ok != 0) {
			decaf_neg(p, u*v, v);
			w = v * s * (2-Z);
			if(s == 0)
				w = w + 1;
			X = 2*s;
			Y = w * Z;
			T = w * X;
		}
	}
}
