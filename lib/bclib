scale = 50
define e(x) {
	auto a, b, c, d, e, g, w, y, t, r

	r = ibase
	ibase = A

	t = scale
	scale = t + .434*x + 1

	w = 0
	if(x<0) {
		x = -x
		w = 1
	}
	y = 0
	while(x>2) {
		x /= 2
		y++
	}

	a = 1
	b = 1
	c = b
	d = 1
	e = 1
	for(a=1; 1; a++) {
		b *= x
		c = c*a+b
		d *= a
		g = c/d
		if(g == e) {
			g = g/1
			while(y--) {
				g *= g
			}
			scale = t
			if(w==1) {
				ibase = r
				return 1/g
			}
			ibase = r
			return g/1
		}
		e = g
	}
}

define l(x) {
	auto a, b, c, d, e, f, g, u, s, t, r, z

	r = ibase
	ibase = A
	if(x <= 0) {
		z = 1-10^scale
		ibase = r
		return z
	}
	t = scale

	f = 1
	scale += scale(x) - length(x) + 1
	s = scale
	while(x > 2) {
		s += (length(x)-scale(x))/2 + 1
		if(s>0) {
			scale = s
		}
		x = sqrt(x)
		f *= 2
	}
	while(x < .5) {
		s += (length(x)-scale(x))/2 + 1
		if(s>0) {
			scale = s
		}
		x = sqrt(x)
		f *= 2
	}

	scale = t + length(f) - scale(f) + 1
	u = (x-1)/(x+1)

	scale += 1.1*length(t) - 1.1*scale(t)
	s = u*u
	b = 2*f
	c = b
	d = 1
	e = 1
	for(a=3; 1; a=a+2){
		b *= s
		c = c*a + d*b
		d *= a
		g = c/d
		if(g==e) {
			scale = t
			ibase = r
			return u*c/d
		}
		e = g
	}
}

define s(x) {
	auto a, b, c, s, t, y, p, n, i, r

	r = ibase
	ibase = A
	t = scale
	y = x/.7853
	s = t + length(y) - scale(y)
	if(s<t) {
		s = t
	}
	scale = s
	p = a(1)

	scale = 0
	if(x>=0) {
		n = (x/(2*p)+1)/2
	}
	if(x<0) {
		n = (x/(2*p)-1)/2
	}
	x -= 4*n*p
	if(n%2 != 0) {
		x = -x
	}

	scale = t + length(1.2*t) - scale(1.2*t)
	y = -x*x
	a = x
	b = 1
	s = x
	for(i=3; 1; i+=2) {
		a *= y
		b *= i*(i-1)
		c = a/b
		if(c==0){
			scale = t
			ibase = r
			return s/1
		}
		s += c
	}
}

define c(x) {
	auto t, r

	r = ibase
	ibase = A
	t = scale
	scale = scale+1
	x = s(x + 2*a(1))
	scale = t
	ibase = r
	return x/1
}

define a(x) {
	auto a, b, c, d, e, f, g, s, t, r, z

	r = ibase
	ibase = A
	if(x==0) {
		return 0
	}
	if(x==1) {
		z = .7853981633974483096156608458198757210492923498437764/1
		ibase = r
		if(scale<52) {
			return z
		}
	}
	t = scale
	f = 1
	while(x > .5) {
		scale++
		x = -(1 - sqrt(1.+x*x))/x
		f *= 2
	}
	while(x < -.5) {
		scale++
		x = -(1 - sqrt(1.+x*x))/x
		f *= 2
	}
	s = -x*x
	b = f
	c = f
	d = 1
	e = 1
	for(a=3; 1; a+=2) {
		b *= s
		c = c*a + d*b
		d *= a
		g = c/d
		if(g==e) {
			scale = t
			ibase = r
			return x*c/d
		}
		e = g
	}
}

define j(n,x) {
	auto a,b,c,d,e,g,i,s,k,t,r

	r = ibase
	ibase = A

	t = scale
	k = 1.36*x + 1.16*t - n
	k = length(k) - scale(k)
	if(k>0) {
		scale += k
	}

	s = -x*x/4
	if(n<0) {
		n = -n
		x = -x
	}
	a = 1
	c = 1
	for(i=1; i<=n; i++) {
		a *= x
		c *= 2*i
	}
	b = a
	d = 1
	e = 1
	for(i=1; 1; i++) {
		a *= s
		b = b*i*(n+i) + a
		c *= i*(n+i)
		g = b/c
		if(g==e) {
			scale = t
			ibase = r
			return g/1
		}
		e = g
	}
}
