# when raw index has a lot of entries like
# 1578324	problematico, a, ci, che
# apply this algorithm:
#  treat things after comma as suffixes
#  for each suffix:
#      if single letter, replace last letter
#      else search backwards for beginning of suffix
#      and if it leads to an old suffix of approximately
#      the same length, put replace that suffix
# This will still leave some commas to fix by hand
# Usage: awk -F'	' -f comfix.awk rawindex > newrawindex

NF == 2	{
		i = index($2, ",")
		if(i == 0 || length($2) == 0)
			print $0
		else {
			n = split($2, a, /,[ ]*/)
			w = a[1]
			printf "%s\t%s\n", $1, w
			for(i = 2; i <= n; i++) {
				suf = a[i]
				m = matchsuflen(w, suf)
				if(m) {
					nw = substr(w, 1, length(w)-m) suf
					printf "%s\t%s\n", $1, nw
				} else
					printf "%s\t%s\n", $1, w ", " suf
			}
		}
	}
NF != 2 {
	print $0
	}

function matchsuflen(w, suf,		wlen,suflen,c,pat,k,d)
{
	wlen = length(w)
	suflen = length(suf)
	if(suflen == 1)
		return 1
	else {
		c = substr(suf, 1, 1)
		for (k = 1; k <= wlen ; k++)
			if(substr(w, wlen-k+1, 1) == c)
				break
		if(k > wlen)
			return 0
		d = k-suflen
		if(d < 0)
			d = -d
		if(d > 3)
			return 0
		return k
	}
}
