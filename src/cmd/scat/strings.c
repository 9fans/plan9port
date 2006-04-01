char *greek[]={ 0,	/* 1-indexed */
	"alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", "theta",
	"iota", "kappa", "lambda", "mu", "nu", "xsi", "omicron", "pi", "rho",
	"sigma", "tau", "upsilon", "phi", "chi", "psi", "omega"
};

Rune greeklet[]={ 0,
	0x3b1, 0x3b2, 0x3b3, 0x3b4, 0x3b5, 0x3b6, 0x3b7, 0x3b8, 0x3b9, 0x3ba, 0x3bb,
	0x3bc, 0x3bd, 0x3be, 0x3bf, 0x3c0, 0x3c1, 0x3c3, 0x3c4, 0x3c5, 0x3c6, 0x3c7,
	0x3c8, 0x3c9
};

char *constel[]={ 0,	/* 1-indexed */
	"and", "ant", "aps", "aql", "aqr", "ara", "ari", "aur", "boo", "cae",
	"cam", "cap", "car", "cas", "cen", "cep", "cet", "cha", "cir", "cma",
	"cmi", "cnc", "col", "com", "cra", "crb", "crt", "cru", "crv", "cvn",
	"cyg", "del", "dor", "dra", "equ", "eri", "for", "gem", "gru", "her",
	"hor", "hya", "hyi", "ind", "lac", "leo", "lep", "lib", "lmi", "lup",
	"lyn", "lyr", "men", "mic", "mon", "mus", "nor", "oct", "oph", "ori",
	"pav", "peg", "per", "phe", "pic", "psa", "psc", "pup", "pyx", "ret",
	"scl", "sco", "sct", "ser", "sex", "sge", "sgr", "tau", "tel", "tra",
	"tri", "tuc", "uma", "umi", "vel", "vir", "vol", "vul"
};
Name names[]={
	"gx",	Galaxy,
	"pl",	PlanetaryN,
	"oc",	OpenCl,
	"gb",	GlobularCl,
	"nb",	DiffuseN,
	"c+n",NebularCl,
	"ast",	Asterism,
	"kt",	Knot,
	"***",	Triple,
	"d*",	Double,
	"*",	Single,
	"pd",	PlateDefect,
	"galaxy",	Galaxy,
	"planetary",	PlanetaryN,
	"opencluster",	OpenCl,
	"globularcluster",	GlobularCl,
	"nebula",	DiffuseN,
	"nebularcluster",NebularCl,
	"asterism",	Asterism,
	"knot",	Knot,
	"triple",	Triple,
	"double",	Double,
	"single",	Single,
	"nonexistent",	Nonexistent,
	"unknown",	Unknown,
	"platedefect",	PlateDefect,
	0,		0
};
