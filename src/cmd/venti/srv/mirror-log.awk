# possible cron job:
# 
# cd /cfg/backup
# { for(i in 'E0 E1' 'E2 E3' 'E4 E5' 'E6 E7' 'F0 F1' 'F2 F3' 'F4 F5'){
# 	x=`{echo $i}
# 	venti/mirrorarenas -v /dev/sd$x(1)^/arenas /dev/sd$x(2)^/arenas
# } } >www/mirror1.txt >[2=1]
# mv www/mirror1.txt www/mirror.txt
# awk -f mirror-log.awk www/mirror.txt >www/mirror.html

BEGIN {
	print "<html><body><h1>mirror status</h1>"
	print "details in <a href=mirror.txt>mirror.txt</a><br><br>"
	print "<hr><table cellpadding=5 cellspacing=0 border=0>"
	laststatus = ""
	firstarena = ""
	lastarena = ""
	status = ""
	arena = ""
	
}

function fmt(  color) {
	nfmt++
	if(nfmt%2 == 0)
		color = "#cccccc"
	else
		color = "#ffffff"
	return "<tr bgcolor=" color "><td valign=top>%s</td><td valign=top>%s</td><td>%s</td><td>%s</td><td>"
}


function finish() {
	if(!arena && !status)
		return
	if(info == "" && laststatus == status){
		lastarena = arena
		return
	}
	if(firstarena != ""){
		if(firstarena == lastarena)
			printf(fmt(), time, firstarena, "", "");
		else
			printf(fmt(), time, firstarena, "-", lastarena);
		print laststatus "</td></tr>"
		firstarena = ""
		lastarena = ""
		laststatus = ""
	}
	if(info == ""){
		firstarena = arena
		laststatus = status
		lastarena = arena
		return
	}
	printf(fmt(), time, arena, "", "");
	print status
	if(info != ""){
		print "<pre>"
		printf("%s", info)
		print "</pre>"
	}
	print "</td>"
}

$3 !~ /:$/ && $4 ~ /^\(.*-.*\)$/ {
	finish();
	arena = $3
	range = $4
	status = ""
	info = ""
	size = 0
	time = $1 " " $2
	next
}

$3 ~ /:$/ && $0 ~ /^....\/.... ..:..:.. [^ ]/ {
	if($4 == "0" && $5 == "used" && $6 == "mirrored"){
		status = "empty"
		next
	}
	if($4 ~ /^[0-9,]+$/ && $5 == "used" && $6 == "mirrored"){
		size = $4
		status = "partial " size ", mirrored"
		next
	}
	if($4 ~ /^[0-9a-f]+$/ && length($4) == 40 && $5 == "sealed" && $6 == "mirrored"){
		status = "sealed, mirrored";
		next
	}
}

{
	info = info $0 "\n"
}

END{
	finish();
	status = "done"
	arena = ""
	info = ""
	finish();
	print "</table><hr>"
	print "</body></html>"
}
