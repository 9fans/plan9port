
biggraph = "arg=rpctotal&graph=diff"

graphname = new Array(
	"arg=*&graph=diskbw",
		"<b>disk</b> bytes/second",
	"arg=*&graph=netbw",
		"<b>network</b> bytes/second",
	"arg=*&graph=iobw",
		"total: <b>disk+net</b> bytes/second",

	"arg=apartreadbyte&graph=diff",
		"arena read bytes/second",
	"arg=apartwritebyte&graph=diff",
		"arena write bytes/second",

	"arg=bloomfalsemiss&graph=pctdiff&arg2=bloomlookup&max=100",
		"bloom false hit %",
	"arg=bloomhit&graph=pctdiff&arg2=bloomlookup&max=100",
		"bloom miss %",
	"arg=bloomlookuptime&graph=divdiff&arg2=bloomlookup",
		"bloom lookup time",
	"arg=bloomones&graph=pct&arg2=bloombits&max=100",
		"bloom usage %",

	"arg=dcachedirty&graph=pct&arg2=dcachesize&max=100",
		"dcache dirty %",
	"arg=dcachehit&graph=pctdiff&arg2=dcachelookup&max=100",
		"dcache hit %",
	"arg=dcachelookuptime&graph=divdiff&arg2=dcachelookup",
		"dcache lookup time",
	"arg=dcachelookup&graph=diff",
		"dcache lookups/second",
	"arg=dcachewrite&graph=diff",
		"dcache writes/second",

	"arg=icachedirty&graph=pct&arg2=icachesize&max=100",
		"icache dirty %",
	"arg=icachehit&graph=pctdiff&arg2=icachelookup&max=100",
		"icache hit %",
	"arg=icachelookuptime&graph=divdiff&arg2=icachelookup",
		"icache lookup time",
	"arg=icacheprefetch&graph=diff",
		"icache prefetches/second",
	"arg=icachewrite&graph=diff",
		"icache writes/second",

	"arg=isectreadbyte&graph=diff",	
		"isect read bytes/second",
	"arg=isectwritebyte&graph=diff",
		"isect write bytes/second",

	"arg=lcachehit&graph=pctdiff&arg2=lcachelookup&max=100",
		"lump cache hit %",
	"arg=lcachelookuptime&graph=divdiff&arg2=lcachelookup",
		"lump cache lookup time",
	"arg=lcachewrite&graph=diff",
		"lcache writes/second",

	"arg=rpcreadbyte&graph=diff",
		"read RPC bytes/second",
	"arg=rpctotal&graph=diff",
		"RPCs/second",
	"arg=rpcwritebyte&graph=diff",
		"write RPC bytes/second",
	"arg=rpcreadtime&graph=divdiff&arg2=rpcread",
		"read RPC time",
	"arg=rpcwritetime&graph=divdiff&arg2=rpcwrite",
		"write RPC time",
	"arg=rpcreadcachedtime&graph=divdiff&arg2=rpcreadcached",
		"cached read RPC time",
	"arg=rpcreaduncachedtime&graph=divdiff&arg2=rpcreaduncached",
		"uncached read RPC time",
	"arg=rpcwritenewtime&graph=divdiff&arg2=rpcwritenew",
		"fresh write RPC time",
	"arg=rpcwriteoldtime&graph=divdiff&arg2=rpcwriteold",
		"dup write RPC time",

	"arg=sumreadbyte&graph=diff",
		"checksum bytes/second",

	"arg=dblockstall",
		"threads stalled: dblock",
	"arg=dcachestall",
		"threads stalled: dcache",
	"arg=icachestall",
		"threads stalled: icache",
	"arg=lumpstall",
		"threads stalled: lump",

	"arg=END"
)

column0 = new Array(
	"column0",
	"!bandwidth",
	"arg=*&graph=iobw",
	"arg=*&graph=netbw",
	"arg=rpcreadbyte&graph=diff",
	"arg=rpcwritebyte&graph=diff",
	"arg=*&graph=diskbw",
	"arg=isectreadbyte&graph=diff",
	"arg=isectwritebyte&graph=diff",
	"arg=apartreadbyte&graph=diff",
	"arg=apartwritebyte&graph=diff",
	"arg=sumreadbyte&graph=diff",
	
	"!bloom filter",
	"arg=bloomhit&graph=pctdiff&arg2=bloomlookup&max=100",
	"arg=bloomfalsemiss&graph=pctdiff&arg2=bloomlookup&max=100",
	"arg=bloomones&graph=pct&arg2=bloombits&max=100",
	
	"END"
)

column1 = new Array(
	"column1",
	"!icache",
	"arg=icachedirty&graph=pct&arg2=icachesize&max=100",
	"arg=icachehit&graph=pctdiff&arg2=icachelookup&max=100",
	"arg=icachewrite&graph=diff",
	"arg=icacheprefetch&graph=diff",
	
	"!dcache",
	"arg=dcachedirty&graph=pct&arg2=dcachesize&max=100",
	"arg=dcachehit&graph=pctdiff&arg2=dcachelookup&max=100",
	"arg=dcachelookup&graph=diff",
	"arg=dcachewrite&graph=diff",
	
	"!lump cache",
	"arg=lcachehit&graph=pctdiff&arg2=lcachelookup&max=100",
	"arg=lcachewrite&graph=diff",
	
	"END"
)

column2 = new Array(
	"column2",

	"!stalls",
	"arg=icachestall",
	"arg=dcachestall",
	"arg=dblockstall",
	"arg=lumpstall",
	
	"!timings",
	"arg=bloomlookuptime&graph=divdiff&arg2=bloomlookup",
	"arg=icachelookuptime&graph=divdiff&arg2=icachelookup",
	"arg=lcachelookuptime&graph=divdiff&arg2=lcachelookup",
	"arg=dcachelookuptime&graph=divdiff&arg2=dcachelookup",
	"arg=rpcreadtime&graph=divdiff&arg2=rpcread",
	"arg=rpcwritetime&graph=divdiff&arg2=rpcwrite",
	"arg=rpcreadcachedtime&graph=divdiff&arg2=rpcreadcached",
	"arg=rpcreaduncachedtime&graph=divdiff&arg2=rpcreaduncached",
	"arg=rpcwritenewtime&graph=divdiff&arg2=rpcwritenew",
	"arg=rpcwriteoldtime&graph=divdiff&arg2=rpcwriteold",
	
	"END"
)

col0info = new Array(column0.length)
col1info = new Array(column1.length)
col2info = new Array(column2.length)

function cleardebug() {
	var p = document.getElementById("debug")
	p.innerHTML = ""
}

function debug(s) {
	var p = document.getElementById("debug")
	if(p.innerHTML == "")
		p.innerHTML = "<a href=\"javascript:cleardebug()\">clear</a>\n"
	p.innerHTML += "<br>"+s
}

function Ginfo(y, fill, name) {
	var g = new Object()
	g.y = y
	g.fill = fill
	g.name = name
	return g
}

function cleartable(t) {
	for(var i=t.rows.length-1; i>=0; i--)
		t.deleteRow(i)
}

function textofname(name)
{
	for(var i=0; i<graphname.length; i+=2)
		if(name == graphname[i])
			return graphname[i+1]
}

function graphrow(row, span, name, dt, wid, ht, fill, text) {
	var url = "/graph?"+name
	url = url+"&min=0"
	url = url+"&t0=-"+dt
	url = url+"&wid="+wid
	url = url+"&ht="+ht
	url = url+"&fill="+fill

	var s = "<td colSpan="+span
	s = s+" valign=bottom"
	s = s+" align=center"
	s = s+" width="+wid
	s = s+" height="+ht
	s = s+" style=\"background-image: url("+url+");\""
	s = s+">"+textofname(name)+text+"</td>"
	row.innerHTML = s
}


function graphcell(cell, name, dt, wid, ht, fill) {
	cell.vAlign = "bottom"
	cell.align = "center"
	cell.width = wid
	cell.height = ht
}

function redraw() {
	redrawgraphs()
	redrawsettings()
}

function redrawgraphs() {
	var t = document.getElementById("statgraphs")
	
	cleartable(t)
	for(var i=0; i<4; i++)
		t.insertRow(i)

	graphrow(t.rows[0], 3, biggraph, 86400, 900, 30, 0, " &ndash; showing 24 hours")
	graphrow(t.rows[1], 3, biggraph, 3600, 900, 30, 1, " &ndash; showing 1 hour")
	t.rows[2].innerHTML = "<td height=10></td>"
	
	var r = t.rows[3]
	graphtable(r.insertCell(0), column0, col0info, 0)
	graphtable(r.insertCell(1), column1, col1info, 2)
	graphtable(r.insertCell(2), column2, col2info, 4)
}

function graphtable(bigcell, list, infolist, fill) {
	bigcell.innerHTML = "<table id=\""+list[0]+"\"></table>"
	bigcell.vAlign = "top"
	var t = document.getElementById(list[0])
	t.onclick = columnclick

	for(var i=1; i<list.length; i++){
		var r = t.insertRow(t.rows.length)
		name = list[i]
		infolist[i] = Ginfo(t.offsetHeight, fill, name)
		if(name == "END")
			break
		if(name.substring(0,1) == "!"){
			name = name.substring(1)
			if(i > 1){
				r.innerHTML = "<td height=10></td>"
				r = t.insertRow(t.rows.length)
			}
			r.innerHTML = "<td align=center><b>"+name+"</b>"
		}else{
			graphrow(r, 1, name, 600, 300, 30, fill++, "")
		}
	}
}

function xpos(obj) {
	var x = 0
	if(obj.fixedx)
		return obj.fixedx
	if(obj.offsetParent){
		while(obj.offsetParent){
			x += obj.offsetLeft
			obj = obj.offsetParent
		}
	}else if(obj.x)
		x = obj.x
	return x
}
		
function ypos(obj) {
	var y = 0
	if(obj.fixedy)
		return obj.fixedy
	if(obj.offsetParent){
		while(obj.offsetParent){
			y += obj.offsetTop
			obj = obj.offsetParent
		}
	}else if(obj.y)
		y = obj.y
	return y
}

function scrollleft() {
	return document.body.scrollLeft
}

function scrolltop() {
	return document.body.scrollTop
}

function columnclick(e) {
	if(e.which && e.which != 1)
		return;
	var g = findgraph(scrollleft()+e.clientX, scrolltop()+e.clientY)
	if(g && g.name.substring(0,1) != "!"){
		biggraph = g.name
		var t = document.getElementById("statgraphs")
		graphrow(t.rows[0], 3, biggraph, 86400, 900, 30, 0, " &ndash; showing 24 hours")
		graphrow(t.rows[1], 3, biggraph, 3600, 900, 30, 1, " &ndash; showing 1 hour")
	}
}

function findgraph(x, y) {
	var g
	
	if(g = findgraphin(x, y, "column2", col2info))
		return g
	if(g = findgraphin(x, y, "column1", col1info))
		return g
	if(g = findgraphin(x, y, "column0", col0info))
		return g
	return
}

function findgraphin(x, y, tname, info) {
	var t = document.getElementById(tname)
	if(x < xpos(t))
		return
	y = y - ypos(t)
	for(var i=info.length-2; i>=1; i--){
		if(y > info[i].y)
			return info[i]
	}
	return
}

function setof(name, val, list) {
	var s = ""
	for(var i=0; i<list.length; i++){
		if(val == list[i])
			s = s+" <b>"+val+"</b>"
		else
			s = s+" <a href=\"javascript:set('"+name+"', '"+list[i]+"')\">"+list[i]+"</a>"
	}
	return s
}

function loglinks(list) {
	var s = ""
	for(var i=0; i<list.length; i++){
		s = s+" <a href=\"/log/"+list[i]+"\">"+list[i]+"</a>"
	}
	return s
}

first = 1
function redrawsettings() {
	if(first){
		loadsettings()
		first = 0
	}
	var s = ""
	s = s+"<font size=-1>\n"
	s = s+"logging:"+setof("logging", logging, loggingchoices)
	s = s+" &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; "
	s = s+"stats:"+setof("stats", stats, statschoices)
	s = s+"\n<p/>\n"
	s = s+"compression:"+setof("compress", compress, compresschoices1)
	s = s+"<br>"+setof("compress", compress, compresschoices2)
	s = s+"\n<p/>\n"
	s = s+"<a href=/index>index</a> | <a href=/storage>storage</a> | "
	s = s+"log:"+loglinks(logs)
	s = s+"</font>"
	document.getElementById("settings").innerHTML = s
}

function set(name, value) {
	eval(name+"= \""+value+"\"")
	redrawsettings()
	// Works in FireFox, not in Safari
	parent.hidden.location.href = "/set/"+name+"/"+value
}
