function loadsettings() {
	logging = "off"
	loggingchoices = new Array("0", "1")

	stats = "on"
	statschoices = new Array("0", "1")

	compress = "whack"
	compresschoices1 = new Array("none", 
		"flate1", "flate2", "flate3", "flate4", "flate5",
		"flate6", "flate7", "flate8", "flate9")
	compresschoices2 = new Array("smack1", "smack2", "smack3", "whack")

	logs = new Array("all", "libventi/server", "disk", "lump", "block", "proc", "quiet", "rpc")
}

