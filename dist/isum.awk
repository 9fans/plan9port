# Summarize the installation log, printing errors along with
# enough context to make sense of them.

BEGIN {
#	print verbose 
	cd = ""
	out = "/dev/stdout";
}

debug { print "# " $0 }

/^$/ { next }

/^echo cd / { next }
/^\+\+ pwd/ { next }

/^\* /{
	if(debug) print "% mark"
	print >out
	fflush(out)
	cmd = ""
	printtabs = 1	# print indented lines immediately following
	errors = 0
	next
}

/^	/ && printtabs {
	print >out
	fflush(out)
	next
}

{ printtabs = 0 }

/^(9a|9c|9l|9ar|9?install|cp|rm|mv|mk|9 yacc|9 lex|9 rc|do|for i|if|mk|gcc|cpp|cp|sh|cmp|rc|\.\/o)($|[^:])/ {
	if(debug) print "% start"
	errors = 0
	cmd = ""
	if(!verbose)
		cmd = cmd cd
	cmd = cmd $0 "\n"
	next
}

/^cd / {
	if(debug) print "% cd"
	errors = 0
	if(verbose){
		print >out
		fflush(out)
	}
	cd = $0 "\n"
	cmd = ""
	next
}

{
	cmd = cmd $0 "\n"
}

errors {
	if(debug) print "% errors"
	printf "%s", cmd >out
	fflush(out)
	cmd = ""
	next
}

/^(	|then|else|fi|done|[ar] - [^ ]*\.o$)/ {
	next
}

/^(up to date|nothing to see|assuming it will be|loop not entered|conflicts:)/ {
	if(debug) print "% skip1"
	next
}

/is up to date/ {
	next
}

/(nodes\(%e\)|packed transitions)/ {
	if(debug) print "% skip2"
	next
}

{ 
	# unexpected line 
	if(debug) print "% errors1"
	errors = 1
	printf ">>> %s", cmd >out
	fflush(out)
	cmd = ""
}

