
biggraph = "rpctotal/diff"

graphname = new Array(
	"*/diskbw",
		"<b>disk</b> bytes/second",
	"*/netbw",
		"<b>network</b> bytes/second",
	"*/iobw",
		"total: <b>disk+net</b> bytes/second",

	"apartreadbyte/diff",
		"arena read bytes/second",
	"apartwritebyte/diff",
		"arena write bytes/second",

	"bloomfalsemiss/pctdiff=bloomlookup",
		"bloom false hit %",
	"bloomhit/pctdiff=bloomlookup",
		"bloom miss %",
	"bloomlookuptime/divdiff=bloomlookup",
		"bloom lookup time",
	"bloomones/pct=bloombits",
		"bloom usage %",

	"dcachedirty/pct=dcachesize",
		"dcache dirty %",
	"dcachehit/pctdiff=dcachelookup",
		"dcache hit %",
	"dcachelookuptime/divdiff=dcachelookup",
		"dcache lookup time",
	"dcachelookup/diff",
		"dcache lookups/second",
	"dcachewrite/diff",
		"dcache writes/second",

	"icachedirty/pct=icachesize",
		"icache dirty %",
	"icachehit/pctdiff=icachelookup",
		"icache hit %",
	"icachelookuptime/divdiff=icachelookup",
		"icache lookup time",
	"icacheprefetch/diff",
		"icache prefetches/second",
	"icachewrite/diff",
		"icache writes/second",

	"isectreadbyte/diff",	
		"isect read bytes/second",
	"isectwritebyte/diff",
		"isect write bytes/second",

	"lcachehit/pctdiff=lcachelookup",
		"lump cache hit %",
	"lcachelookuptime/divdiff=lcachelookup",
		"lump cache lookup time",
	"lcachewrite/diff",
		"lcache writes/second",

	"rpcreadbyte/diff",
		"read RPC bytes/second",
	"rpctotal/diff",
		"RPCs/second",
	"rpcwritebyte/diff",
		"write RPC bytes/second",
	"rpcreadtime/divdiff=rpcread",
		"read RPC time",
	"rpcwritetime/divdiff=rpcwrite",
		"write RPC time",
	"rpcreadcachedtime/divdiff=rpcreadcached",
		"cached read RPC time",
	"rpcreaduncachedtime/divdiff=rpcreaduncached",
		"uncached read RPC time",
	"rpcwritenewtime/divdiff=rpcwritenew",
		"fresh write RPC time",
	"rpcwriteoldtime/divdiff=rpcwriteold",
		"dup write RPC time",

	"sumreadbyte/diff",
		"checksum bytes/second",

	"dblockstall",
		"threads stalled: dblock",
	"dcachestall",
		"threads stalled: dcache",
	"icachestall",
		"threads stalled: icache",
	"lumpstall",
		"threads stalled: lump",

	"END"
)

column0 = new Array(
	"column0",
	"!bandwidth",
	"*/iobw",
	"*/netbw",
	"rpcreadbyte/diff",
	"rpcwritebyte/diff",
	"*/diskbw",
	"isectreadbyte/diff",
	"isectwritebyte/diff",
	"apartreadbyte/diff",
	"apartwritebyte/diff",
	"sumreadbyte/diff",
	
	"!bloom filter",
	"bloomhit/pctdiff=bloomlookup",
	"bloomfalsemiss/pctdiff=bloomlookup",
	"bloomones/pct=bloombits",
	
	"END"
)

column1 = new Array(
	"column1",
	"!icache",
	"icachedirty/pct=icachesize",
	"icachehit/pctdiff=icachelookup",
	"icachewrite/diff",
	"icacheprefetch/diff",
	
	"!dcache",
	"dcachedirty/pct=dcachesize",
	"dcachehit/pctdiff=dcachelookup",
	"dcachelookup/diff",
	"dcachewrite/diff",
	
	"!lump cache",
	"lcachehit/pctdiff=lcachelookup",
	"lcachewrite/diff",
	
	"END"
)

column2 = new Array(
	"column2",

	"!stalls",
	"icachestall",
	"dcachestall",
	"dblockstall",
	"lumpstall",
	
	"!timings",
	"bloomlookuptime/divdiff=bloomlookup",
	"icachelookuptime/divdiff=icachelookup",
	"lcachelookuptime/divdiff=lcachelookup",
	"dcachelookuptime/divdiff=dcachelookup",
	"rpcreadtime/divdiff=rpcread",
	"rpcwritetime/divdiff=rpcwrite",
	"rpcreadcachedtime/divdiff=rpcreadcached",
	"rpcreaduncachedtime/divdiff=rpcreaduncached",
	"rpcwritenewtime/divdiff=rpcwritenew",
	"rpcwriteoldtime/divdiff=rpcwriteold",
	
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
	var url = "/graph/"+name
	url = url+"/min=0"
	url = url+"/t0=-"+dt
	url = url+"/wid="+wid
	url = url+"/ht="+ht
	url = url+"/fill="+fill

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
	debug("t.rows.length="+t.rows.length)
	graphrow(t.rows[1], 3, biggraph, 3600, 900, 30, 1, " &ndash; showing 1 hour")
	debug("t.rows.length="+t.rows.length)
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
		return
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
	for(var i=info.length-2; i>=1; i--)
		if(y > info[i].y)
			return info[i]
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
