# Usage: cd $PLAN9; awk -f dist/checkman.awk man?/*.?
#
# Checks:
#	- .TH is first line, and has proper name section number
#	- sections are in order NAME, SYNOPSIS, DESCRIPTION, EXAMPLES,
#		FILES, SOURCE, SEE ALSO, DIAGNOSTICS, BUGS
#	- there's a manual page for each cross-referenced page

BEGIN {

#	.SH sections should come in the following order

	Weight["NAME"] = 1
	Weight["SYNOPSIS"] = 2
	Weight["DESCRIPTION"] = 4
	Weight["EXAMPLE"] = 8
	Weight["EXAMPLES"] = 16
	Weight["FILES"] = 32
	Weight["SOURCE"] = 64
	Weight["SEE ALSO"] = 128
	Weight["DIAGNOSTICS"] = 256
	Weight["SYSTEM CALLS"] = 512
	Weight["BUGS"] = 1024

	Skipdirs["CVS"] = 1

	# allow references to pages provded
	# by the underlying Unix system
	Omitman["awk(1)"] = 1
	Omitman["bash(1)"] = 1
	Omitman["chmod(1)"] = 1
	Omitman["compress(1)"] = 1
	Omitman["cp(1)"] = 1
	Omitman["egrep(1)"] = 1
	Omitman["gs(1)"] = 1
	Omitman["gv(1)"] = 1
	Omitman["lex(1)"] = 1
	Omitman["lp(1)"] = 1
	Omitman["lpr(1)"] = 1
	Omitman["mail(1)"] = 1
	Omitman["make(1)"] = 1
	Omitman["nm(1)"] = 1
	Omitman["prof(1)"] = 1
	Omitman["pwd(1)"] = 1
	Omitman["qiv(1)"] = 1
	Omitman["sftp(1)"] = 1
	Omitman["sh(1)"] = 1
	Omitman["ssh(1)"] = 1
	Omitman["stty(1)"] = 1
	Omitman["tex(1)"] = 1
	Omitman["unutf(1)"] = 1
	Omitman["vnc(1)"] = 1
	Omitman["xterm(1)"] = 1

	Omitman["access(2)"] = 1
	Omitman["brk(2)"] = 1
	Omitman["chdir(2)"] = 1
	Omitman["close(2)"] = 1
	Omitman["connect(2)"] = 1
	Omitman["fork(2)"] = 1
	Omitman["gethostname(2)"] = 1
	Omitman["getpid(2)"] = 1
	Omitman["getuid(2)"] = 1
	Omitman["open(2)"] = 1
	Omitman["pipe(2)"] = 1
	Omitman["ptrace(2)"] = 1
	Omitman["rmdir(2)"] = 1
	Omitman["send(2)"] = 1
	Omitman["signal(2)"] = 1
	Omitman["sigprocmask(2)"] = 1
	Omitman["socketpair(2)"] = 1
	Omitman["unlink(2)"] = 1

	Omitman["abort(3)"] = 1
	Omitman["assert(3)"] = 1
	Omitman["fprintf(3)"] = 1
	Omitman["fscanf(3)"] = 1
	Omitman["fopen(3)"] = 1
	Omitman["isalpha(3)"] = 1
	Omitman["malloc(3)"] = 1
	Omitman["perror(3)"] = 1
	Omitman["remove(3)"] = 1
	Omitman["sin(3)"] = 1
	Omitman["strerror(3)"] = 1

	Omitman["core(5)"] = 1
	Omitman["passwd(5)"] = 1

	Omitman["signal(7)"] = 1

	Omitman["cron(8)"] = 1
	Omitman["mount(8)"] = 1

	# don't need documentation for these in bin
	Omitted[".cvsignore"] = 1
	Omitted["Getdir"] = 1
	Omitted["Irc"] = 1
	Omitted["Juke"] = 1
	Omitted["ajuke"] = 1
	Omitted["goodmk"] = 1
	Omitted["jukefmt"] = 1
	Omitted["jukeget"] = 1
	Omitted["jukeindex"] = 1
	Omitted["jukeinfo"] = 1
	Omitted["jukeplay"] = 1
	Omitted["jukeput"] = 1
	Omitted["jukesearch"] = 1
	Omitted["jukesongfile"] = 1
	Omitted["m4ainfo"] = 1
	Omitted["mp3info"] = 1
	Omitted["notes"] = 1
	Omitted["tcolors"] = 1
	Omitted["tref"] = 1
	Omitted["unutf"] = 1
	Omitted["volume"] = 1
	Omitted["vtdump"] = 1
	Omitted["netfilelib.rc"] = 1

	# not for users
	Omittedlib["creadimage"] = 1
	Omittedlib["pixelbits"] = 1
	Omittedlib["bouncemouse"] = 1
	Omittedlib["main"] = 1	# in libthread
	
	Omittedlib["opasstokey"] = 1	# in libauthsrv

	# functions provided for -lthread_db
	Omittedlib["ps_get_thread_area"] = 1
	Omittedlib["ps_getpid"] = 1
	Omittedlib["ps_lcontinue"] = 1
	Omittedlib["ps_lgetfpregs"] = 1
	Omittedlib["ps_lgetregs"] = 1
	Omittedlib["ps_lsetfpregs"] = 1
	Omittedlib["ps_lsetregs"] = 1
	Omittedlib["ps_lstop"] = 1
	Omittedlib["ps_pcontinue"] = 1
	Omittedlib["ps_pdread"] = 1
	Omittedlib["ps_pdwrite"] = 1
	Omittedlib["ps_pglobal_lookup"] = 1
	Omittedlib["ps_pstop"] = 1
	Omittedlib["ps_ptread"] = 1
	Omittedlib["ps_ptwrite"] = 1

	# libmach includes a small dwarf and elf library
	Omittedlib["corecmdfreebsd386"] = 1
	Omittedlib["corecmdlinux386"] = 1
	Omittedlib["coreregsfreebsd386"] = 1
	Omittedlib["coreregslinux386"] = 1
	Omittedlib["coreregsmachopower"] = 1
	Omittedlib["crackelf"] = 1
	Omittedlib["crackmacho"] = 1
	Omittedlib["dwarfaddrtounit"] = 1
	Omittedlib["dwarfclose"] = 1
	Omittedlib["dwarfenum"] = 1
	Omittedlib["dwarfenumunit"] = 1
	Omittedlib["dwarfget1"] = 1
	Omittedlib["dwarfget128"] = 1
	Omittedlib["dwarfget128s"] = 1
	Omittedlib["dwarfget2"] = 1
	Omittedlib["dwarfget4"] = 1
	Omittedlib["dwarfget8"] = 1
	Omittedlib["dwarfgetabbrev"] = 1
	Omittedlib["dwarfgetaddr"] = 1
	Omittedlib["dwarfgetn"] = 1
	Omittedlib["dwarfgetnref"] = 1
	Omittedlib["dwarfgetstring"] = 1
	Omittedlib["dwarflookupfn"] = 1
	Omittedlib["dwarflookupname"] = 1
	Omittedlib["dwarflookupnameinunit"] = 1
	Omittedlib["dwarflookupsubname"] = 1
	Omittedlib["dwarflookuptag"] = 1
	Omittedlib["dwarfnextsym"] = 1
	Omittedlib["dwarfnextsymat"] = 1
	Omittedlib["dwarfopen"] = 1
	Omittedlib["dwarfpctoline"] = 1
	Omittedlib["dwarfseeksym"] = 1
	Omittedlib["dwarfskip"] = 1
	Omittedlib["dwarfunwind"] = 1
	Omittedlib["elfclose"] = 1
	Omittedlib["elfdl386mapdl"] = 1
	Omittedlib["elfinit"] = 1
	Omittedlib["elfmachine"] = 1
	Omittedlib["elfmap"] = 1
	Omittedlib["elfopen"] = 1
	Omittedlib["elfsection"] = 1
	Omittedlib["elfsym"] = 1
	Omittedlib["elfsymlookup"] = 1
	Omittedlib["elftype"] = 1
	Omittedlib["machoclose"] = 1
	Omittedlib["machoinit"] = 1
	Omittedlib["machoopen"] = 1
	Omittedlib["stabsym"] = 1
	Omittedlib["symdwarf"] = 1
	Omittedlib["symelf"] = 1
	Omittedlib["symmacho"] = 1
	Omittedlib["symstabs"] = 1
	Omittedlib["elfcorelinux386"] = 1
	Omittedlib["linux2ureg386"] = 1
	Omittedlib["ureg2linux386"] = 1
	Omittedlib["coreregs"] = 1	# haven't documented mach yet
	Omittedlib["regdesc"] = 1

	Omittedlib["auth_attr"] = 1	# not happy about this

	Omittedlib["ndbnew"] = 1		# private to library
	Omittedlib["ndbsetval"] = 1

	Renamelib["chanalt"] = "alt"
	Renamelib["channbrecv"] = "nbrecv"
	Renamelib["channbrecvp"] = "nbrecvp"
	Renamelib["channbrecvul"] = "nbrecvul"
	Renamelib["channbsend"] = "nbsend"
	Renamelib["channbsendp"] = "nbsendp"
	Renamelib["channbsendul"] = "nbsendul"
	Renamelib["chanrecv"] = "recv"
	Renamelib["chanrecvp"] = "recvp"
	Renamelib["chanrecvul"] = "recvul"
	Renamelib["chansend"] = "send"
	Renamelib["chansendp"] = "sendp"
	Renamelib["chansendul"] = "sendul"
	Renamelib["threadyield"] = "yield"

	Renamelib["fmtcharstod"] = "charstod"
	Renamelib["fmtstrtod"] = "strtod"

	Renamelib["regcomp9"] = "regcomp"
	Renamelib["regcomplit9"] = "regcomplit"
	Renamelib["regcompnl9"] = "regcompnl"
	Renamelib["regerror9"] = "regerror"
	Renamelib["regexec9"] = "regexec"
	Renamelib["regsub9"] = "regsub"
	Renamelib["rregexec9"] = "rregexec"
	Renamelib["rregsub9"] = "rregsub"

	lastline = "XXX";
	lastfile = FILENAME;
}

func getnmlist(lib,    cmd)
{
	cmd = "nm -g " lib
	while (cmd | getline) {
		if (($2 == "T" || $2 == "L") && $3 !~ "^_"){
			sym = $3
			sub("^p9", "", sym)
			if(sym in Renamelib)
				List[Renamelib[sym]] = lib " as " sym
			else
				List[sym] = lib
		}
	}
	close(cmd)
}


func getindex(dir,    fname)
{
	fname = dir "/INDEX"
	while ((getline < fname) > 0)
		Index[$1] = dir
	close(fname)
}

func getbinlist(dir,    cmd, subdirs, nsd)
{
	cmd = "ls -p -l " dir
	nsd = 0
	while (cmd | getline) {
		if ($1 ~ /^d/) {
			if (!($10 in Skipdirs))
				subdirs[++nsd] = $10
		} else if ($10 !~ "^_")
			List[$10] = dir
	}
	for ( ; nsd > 0 ; nsd--)
		getbinlist(dir "/" subdirs[nsd])
	close(cmd)
}

func clearindex(    i)
{
	for (i in Index)
		delete Index[i]
}

func clearlist(    i)
{
	for (i in List)
		delete List[i]
}


FNR==1	{
	if(lastline == ""){
		# screws up troff headers
		print lastfile ":$ is a blank line"
	}

	n = length(FILENAME)
	nam = FILENAME
	if(nam ~ /\.html$/)
		next
	if(nam !~ /^man\/man(.*)\/(.*)\.(.*)$/){
		print "nam", nam, "not of form [0-9][0-9]?/*"
		next
	}
	nam = substr(nam, 8)
	gsub("[/.]", " ", nam);
	n = split(nam, a)
	sec = a[1]
	name = a[2]
	section = a[3]
	if($1 != ".TH" || NF != 3)
		print "First line of", FILENAME, "not a proper .TH"
	else if(($2 != toupper(name) || substr($3, 1, length(sec)) != sec || $3 != toupper(section)) \
			&& ($2!="INTRO" || name!="0intro") \
			&& (name !~ /^9/ || $2!=toupper(substr(name, 2)))){
		print ".TH of", FILENAME, "doesn't match filename"
	}else
		Pages[tolower($2) "(" tolower($3) ")"] = 1
	Sh = 0
}

{ lastline=$0; lastfile=FILENAME; }

$1 == ".SH" {
	if(inex)
		print "Unterminated .EX in", FILENAME, ":", $0
	inex = 0;
	if (substr($2, 1, 1) == "\"") {
		if (NF == 2) {
			print "Unneeded quote in", FILENAME, ":", $0
			$2 = substr($2, 2, length($2)-2)
		} else if (NF == 3) {
			$2 = substr($2, 2) substr($3, 1, length($3)-1)
			NF = 2
		}
	}
	if(Sh == 0 && $2 != "NAME")
		print FILENAME, "has no .SH NAME"
	w = Weight[$2]
	if (w) {
		if (w < Sh)
			print "Heading", $2, "out of order in", FILENAME
		Sh += w
	}
	sh = $2
}

$1 == ".EX" {
		if(inex)
			print "Nested .EX in", FILENAME ":" FNR, ":", $0
		inex = 1
}

$1 == ".EE" {
	if(!inex)
		print "Bad .EE in", FILENAME ":" FNR ":", $0
	inex = 0;
}

$1 == ".TF" {
	smallspace = 1
}

$1 == ".PD" || $1 == ".SH" || $1 == ".SS" || $1 == ".TH" {
	smallspace = 0
}

$1 == ".RE" {
	lastre = 1
}

$1 == ".PP" {
	if(smallspace && !lastre)
		print "Possible missing .PD at " FILENAME ":" FNR
	smallspace = 0
}

$1 != ".RE" {
	lastre = 0
}

sh == "BUGS" && $1 == ".br" {
	print FILENAME ":" FNR ": .br in BUGS"
}

sh == "SOURCE" && $1 ~ /^\\\*9\// {
	s = ENVIRON["PLAN9"] substr($1, 4)
	Sources[s] = 1
}

sh == "SOURCE" && $2 ~ /^\\\*9\// {
	s = ENVIRON["PLAN9"] substr($2, 4)
	Sources[s] = 1
}

sh == "SOURCE" && $1 ~ /^\// {
	Sources[$1] = 1
}

sh == "SOURCE" && $2 ~ /^\// {
	Sources[$2] = 1
}

$0 ~ /^\.[A-Z].*\([1-9]\)/ {
		if ($1 == ".IR" && $3 ~ /\([0-9]\)/) {
			name = $2
			section = $3
		}else if ($1 == ".RI" && $2 == "(" && $4 ~ /\([0-9]\)/) {
			name = $3
			section = $4
		}else if ($1 == ".IR" && $3 ~ /9.\([0-9]\)/) {
			name = $2
			section = "9"
		}else if ($1 == ".RI" && $2 == "(" && $4 ~ /9.\([0-9]\)/) {
			name = $3
			section = "9"
		} else {
			if ($1 == ".HR" && $3 == "\"Section")
				next;
			print "Possible bad cross-reference format in", FILENAME ":" FNR
			print $0
			next
		}
		gsub(/[^0-9]/, "", section)
		Refs[toupper(name) "(" section ")"]++
}

END {
	if(lastline == ""){
		print lastfile ":$ is a blank line"
	}

	print "Checking Source References"
	cmd = "xargs -n 100 ls -d 2>&1 >/dev/null | sed 's/^ls: /	/; s/: .*//'"
	for (i in Sources) {
		print i |cmd
	}
	close(cmd)
	print ""
	print "Checking Cross-Referenced Pages"
	for (i in Refs) {
		if (!(tolower(i) in Pages) && !(tolower(i) in Omitman)){
			b = tolower(i)
			gsub("\\(", " \\(", b)
			gsub("\\)", "\\)", b)
			split(tolower(i), a, "/")
			print "grep -in '^\\.IR.*" b "' $PLAN9/man/man*/* # Need " tolower(i) |"sort"
		}
	}
	close("sort")
	print ""
	print "Checking commands"
	getindex("man/man1")
	getindex("man/man4")
	getindex("man/man7")
	getindex("man/man8")
	getbinlist("bin")
	for (i in List) {
		if (!(i in Index) && !(i in Omitted))
			print "Need", i, "(in " List[i] ")" |"sort"
	}
	close("sort")
	print ""
	for (i in List) {
		if (!(i in Index) && (i in Omitted))
			print "Omit", i, "(in " List[i] ")" |"sort"
	}
	close("sort")
	clearindex()
	clearlist()
	print ""
	print "Checking libraries"
	getindex("man/man3")
	getnmlist("lib/lib9.a")
	getnmlist("lib/lib9p.a")
	getnmlist("lib/lib9pclient.a")
	getnmlist("lib/libString.a")
	getnmlist("lib/libauth.a")
	getnmlist("lib/libauthsrv.a")
	getnmlist("lib/libbin.a")
	getnmlist("lib/libbio.a")
	getnmlist("lib/libcomplete.a")
	# getnmlist("lib/libcontrol.a")
	getnmlist("lib/libdisk.a")
	getnmlist("lib/libdraw.a")
	getnmlist("lib/libflate.a")
	getnmlist("lib/libframe.a")
	getnmlist("lib/libgeometry.a")
	getnmlist("lib/libhtml.a")
	# getnmlist("lib/libhttpd.a")
	getnmlist("lib/libip.a")
	getnmlist("lib/libmach.a")
	# getnmlist("lib/libmemdraw.a")
	# getnmlist("lib/libmemlayer.a")
	getnmlist("lib/libmp.a")
	getnmlist("lib/libmux.a")
	getnmlist("lib/libndb.a")
	getnmlist("lib/libplumb.a")
	getnmlist("lib/libregexp9.a")
	getnmlist("lib/libsec.a")
	getnmlist("lib/libthread.a")
	# getnmlist("lib/libventi.a")
	for (i in List) {
		if (!(i in Index) && !(i in Omittedlib))
			print "Need", List[i], i |"sort"
			# print "Need", i, "(in " List[i] ")" |"sort"
	}
	close("sort")
	print ""
	for (i in List) {
		if (!(i in Index) && (i in Omittedlib))
			print "Omit", List[i], i |"sort"
			# print "Omit", i, "(in " List[i] ")" |"sort"
	}
	close("sort")
}

