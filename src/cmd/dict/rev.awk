NF == 2	{
		printf "%s\t%s\n", $2, $1
	}
NF != 2 {
		print "ERROR " $0
	}
