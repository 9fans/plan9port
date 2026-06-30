# derived from: http://eli.thegreenplace.net/2009/03/07/computing-square-roots-in-python

# Compute the Legendre symbol a|p using Euler's criterion.
# p is a prime, a is relatively prime to p (if p divides a,
# then a|p = 0)
legendresymbol(a, p, r) {
	pm1 = p-1;
	mod(p) r = a^(pm1>>1);
	if(r == pm1)
		r = -1;
}

# Find a quadratic residue (mod p) of 'a'. p must be an
# odd prime.
#
# Solve the congruence of the form:
#	x^2 = a (mod p)
# And returns x. Node that p - x is also a root.
#
# 0 is returned if no square root exists for these
# a and p.
#
# The Tonelli-Shanks algorithm is used (except
# for some simple cases in which the solution is known 
# from an identity).
msqrt(a, p, r) {
	if(legendresymbol(a, p) != 1)
		r = 0;
	else if(a == 0)
		r = 0;
	else if(p == 2)
		r = a;
	else if(p%4 == 3){
		e = p+1 >> 2;
		mod(p) r = a^e;
	} else {
		# Partition p-1 to s * 2^e for an odd s (i.e.
		# reduce all the powers of 2 from p-1)
		s = p-1;
		e = 0;
		while(s%2 == 0){
			s = s >> 1;
			e = e + 1;
		}

		# Find some 'n' with a legendre symbol n|p = -1.
		# Shouldn't take long.
		n = 2;
		while(legendresymbol(n, p) != -1)
			n = n + 1;

		# x is a guess of the square root that gets better
		# with each iteration.
		# b is the "fudge factor" - by now much we're off
		# with the guess. The invariant x^2 == a*b (mod p)
		# is maintained throughout the loop.
		# g is used for successive powers of n to update
		# both a and b
		# e is the exponent - decreases with each update
		mod(p){
			x = a^(s+1 >> 1);
			b = a^s;
			g = n^s;
		}
		while(1==1){
			t = b;
			m = 0;
			while(m < e){
				if(t == 1)
					break;
				t = t*t % p;
				m = m + 1;
			}
			if(m == 0){
				r = x;
				break;
			}
			t = 2^(e-m-1);
			mod(p){
				gs = g^t;
				g = gs*gs;
				x = x*gs;
				b = b*g;
			}
			e = m;
		}
	}
}

# modular inverse square-root
misqrt(a, p, r) {
	if((p % 4) == 3){
		e = ((p-3)>>2);
		mod(p) r = a^e;
	} else {
		r = msqrt(a, p);
		if(r != 0)
			mod(p) r = 1/r;
	}
}
