NF == 2	{
		if($2 !~ / or / || $2 ~ /\(or/)
			print $0
		else {
			n = split($2, a, / or /)
			for(i = 1; i <= n; i++) {
				printf "%s\t%s\n", $1, a[i]
			}
		}
	}
NF != 2 {
	print $0
	}
