# Summarize the installation log, printing errors along with
# enough context to make sense of them.

BEGIN {
#	print verbose 
	cd = ""
	out = "/dev/stdout"
	statuslen = 0
	debug = 0
	updates = "/dev/stderr"
	if(ENVIRON["winid"] != "") # running in acme window
		updates = ""
}

function myflush(f)
{
	# fflush is not available on sun, but system("") appears to work everywhere
	system("")
}

function clearstatus(noflush, i)
{
	if(!updates)
		return
	for(i=0; i<statuslen; i++)
		printf("\b \b") >updates
	statuslen = 0
	if(!noflush)
		myflush(updates)
}

function status(s)
{
	if(!updates)
		return
	clearstatus(1)
	printf("    %s ", s) >updates
	statuslen = length(s)+5
	myflush(updates)
}

debug!=0 { print "# " $0 }

/^$/ { next }

/^echo cd / { next }
/^\+\+ pwd/ { next }

/^\* /{
	clearstatus()
	if(debug) print "% mark"
	print >out
	myflush(out)
	if(copy){
		print >copy
		myflush(copy)
	}
	cmd = ""
	printtabs = 1	# print indented lines immediately following
	errors = 0
	next
}

/^	/ && printtabs!=0 {
	clearstatus()
	print >out
	myflush(out)
	if(copy){
		print >copy
		myflush(copy)
	}
	next
}

{ printtabs = 0 }

/^(9a|9c|9l|9ar|9?install|cat pdf|cp|rm|mv|mk|9 yacc|9 lex|9 rc|do|for i|if|mk|gcc|cpp|cp|sh|cmp|rc|\.\/o)($|[^:])/ {
	if(debug) print "% start"
	errors = 0
	cmd = ""
	if(!verbose)
		cmd = cmd cd
	cmd = cmd $0 "\n"
	next
}

/^cd .+; mk .+/ && !verbose {
	dir = $2
	sub(/;$/, "", dir)
	status(dir " mk " $4)
}

/^cd / {
	if(debug) print "% cd"
	errors = 0
	if(verbose){
		print >out
		myflush(out)
		if(copy){
			print >copy
			myflush(copy)
		}
	}
	cd = $0 "\n"
	cmd = ""
	next
}

{
	cmd = cmd $0 "\n"
}

errors != 0 {
	clearstatus()
	if(debug) print "% errors"
	printf "%s", cmd >out
	myflush(out)
	if(copy){
		printf "%s", cmd >copy
		myflush(copy)
	}
	cmd = ""
	next
}

/^(	|then|else|fi|done|[ar] - [^ ]*\.o$)/ {
	next
}

/^(conflicts:)/ {
	if(debug) print "% skip1"
	next
}

/(up to date|nothing to see|assuming it will be|loop not entered|# WSYSTYPE)/ {
	next
}

/(nodes\(%e\)|packed transitions)/ {
	if(debug) print "% skip2"
	next
}

{ 
	# unexpected line 
	clearstatus()
	if(debug) print "% errors1"
	errors = 1
	printf ">>> %s", cmd >out
	myflush(out)
	if(copy){
		printf ">>> %s", cmd >copy
		myflush(copy)
	}
	cmd = ""
}

