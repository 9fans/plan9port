# turn output of mkindex into form needed by dict
BEGIN {
	if(ARGC != 2) {
		print "Usage: awk -F'	' -f canonind.awk rawindex > index"
		exit 1
	}
	file = ARGV[1]
	ARGV[1] = ""
	while ((getline < file) > 0) {
		for(i = 2; i <= NF; i++) {
			w = $i
			if(length(w) == 0)
				continue
			b = index(w, "(")
			e = index(w, ")")
			if(b && e && b < e) {
				w1 = substr(w, 1, b-1)
				w2 = substr(w, b+1, e-b-1)
				w3 =  substr(w, e+1)
				printf "%s%s\t%d\n", w1, w3, $1 > "junk"
				printf "%s%s%s\t%d\n", w1, w2, w3, $1 > "junk"
			} else
				printf "%s\t%d\n", w, $1 > "junk"
		}
	}
	system("sort -u -t'	' +0f -1 +0 -1 +1n -2 < junk")
	system("rm junk")
	exit 0
}
