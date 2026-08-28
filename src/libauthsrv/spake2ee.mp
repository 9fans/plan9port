#
# this implements a variant of SPAKE2 Elligator edition described in:
# https://www.mail-archive.com/curves@moderncrypto.org/msg00412.html
#

# derive points PM or PN from a (password) hash
spake2ee_h2P(p,a,d, h, PX,PY,PZ,PT){
	# find a small non-square for elligator
	n = 2;
	while(legendresymbol(n, p) != -1)
		n = n + 1;
	PX,PY,PZ,PT = elligator2(p,a,d, n, h%p);
}

# Ya = xa*G+PM, Yb = xb*G+PN
spake2ee_1(p,a,d, x, GX,GY, PX,PY,PZ,PT, y){
	mod(p) X,Y,Z,T = edwards_scale(p,a,d, x, GX,GY,1,GX*GY);
	X,Y,Z,T = edwards_add(p,a,d, X,Y,Z,T, PX,PY,PZ,PT);
	y = decaf_encode(p,a,d, X,Y,Z,T);
}

# Z = xa*(Yb-PN)
#   = xa*(xb*G+PN-PN)
#   = xa*xb*G
#   = xb*xa*G
#   = xb*(xa*G+PM-PM)
#   = xb*(Ya-PM)
spake2ee_2(p,a,d, PX,PY,PZ,PT, x, y, ok, z){
	ok, X,Y,Z,T = decaf_decode(p,a,d, y);
	if(ok != 0){
		mod(p) X,Y,Z,T = edwards_add(p,a,d, X,Y,Z,T, -PX,PY,PZ,-PT);
		X,Y,Z,T = edwards_scale(p,a,d, x, X,Y,Z,T);
		z = decaf_encode(p,a,d, X,Y,Z,T);
	}
}
