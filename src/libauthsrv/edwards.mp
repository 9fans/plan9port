# Edwards curve arithmetic
edwards_add(p,a,d, X1,Y1,Z1,T1, X2,Y2,Z2,T2, X3,Y3,Z3,T3) mod(p) {
	A = X1*X2;
	B = Y1*Y2;
	C = d*T1*T2;
	D = Z1*Z2;
	E = (X1+Y1)*(X2+Y2);
	E = E - A - B;
	F = D - C;
	G = D + C;
	H = B - a*A;
	X3 = E*F;
	Y3 = G*H;
	Z3 = F*G;
	T3 = E*H;
}
edwards_sel(s, X1,Y1,Z1,T1, X2,Y2,Z2,T2, X3,Y3,Z3,T3){
	X3 = s != 0 ? X1 : X2;
	Y3 = s != 0 ? Y1 : Y2;
	Z3 = s != 0 ? Z1 : Z2;
	T3 = s != 0 ? T1 : T2;
}
edwards_new(x,y,z,t, X,Y,Z,T) {
	X = x;
	Y = y;
	Z = z;
	T = t;
}
edwards_scale(p,a,d, s, X1,Y1,Z1,T1, X3,Y3,Z3,T3) {
	X2,Y2,Z2,T2 = edwards_new(X1,Y1,Z1,T1);
	X4,Y4,Z4,T4 = edwards_new( 0, 1, 1, 0);
	X3,Y3,Z3,T3 = edwards_sel(s % 2, X2,Y2,Z2,T2, X4,Y4,Z4,T4);
	k = s >> 1; j = p >> 1;
	while(j != 0){
		X2,Y2,Z2,T2 = edwards_add(p,a,d, X2,Y2,Z2,T2, X2,Y2,Z2,T2);
		X4,Y4,Z4,T4 = edwards_add(p,a,d, X2,Y2,Z2,T2, X3,Y3,Z3,T3);
		X3,Y3,Z3,T3 = edwards_sel(k % 2, X4,Y4,Z4,T4, X3,Y3,Z3,T3);
		k = k >> 1; j = j >> 1;
	}
}
