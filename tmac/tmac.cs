'''\"					coversheet to match version 8/06/87
'''\"						from comp center 1.30 7/20/87
'''\"					1/22/97 spacing change in addresses;
'''\"						email on separate line - mdm
'''\" registers
'''\"	a - abstract continuation flag - 0 (no), >0 (yes)
'''\"	b - mercury selections counter
'''\"	c - distribution continuation flag - 0 (no), 1 (yes)
'''\"	d - flag for length calculation
'''\"	e - complete copy basic distribution length
'''\"	g - complete copy overflow distribution length
'''\"	h - cover sheet basic distribution length
'''\"	i - cover sheet overflow distribution length
'''\"	k - keyword flag - 0 (none), 1 (some) - reused as scratch
'''\"	l - number of vertical units per line - troff
'''\"	m - memorandum type flag - 1 TM, 2 IM, 3 TC
'''\"	n - document number counter
'''\"	o - title flag - 0 (no), 1 (yes - vertical size of title diversion)
'''\"	p - proprietary notice flag - 0 (none), 1 (default), 2(BR)
'''\"	r - security flag - 0 (no), 1 (yes)
'''\"	s - software flag - 0 (no), 1 (yes)
'''\"	t - mark title position
'''\"	u - author count
'''\"	q v w x y z- scratch - but remembered
'''\"	aa - ship to itds
'''\" strings
'''\"	a) b)- mercury info
'''\"	d) - date
'''\"	k) - keywords
'''\"	N1 - first document number
'''\"	p) q) r)- proprietary 1
'''\"	s) - time stamp string
'''\"	t) - memo type (TM, IM, TC)
'''\"	v) - document nos
'''\"	w) - filing case nos
'''\"	x) - work project nos
'''\"	e( - earlier document
'''\"	m( n( o( p(- authors 1-3 sig
'''\"	r( - responsible person
'''\"	s( - S software string
'''\"	t( - memo type ("for Technical Memorandum", etc.)
'''\"	x( - Mailing Label or DRAFT
'''\"	N2 N3- 2nd 3rd document number
'''\" 	Fi - up to 3 filing cases
'''\"	Xi - up to 3 work program numbers
'''\" diversions
'''\"	WB - abstract
'''\"	ZI - author info section
'''\"	ZC - complete copy addressee primary
'''\"	ZO - complete copy addressee overflow
'''\"	ZS - cover sheet addressee primary
'''\"	ZD - cover sheet addressee overflow
'''\"	ZN - document number info
'''\"	WT - title
'''\"
'''\" initialization
'''\"
'''\"		get ms if not loaded
.if !\n(PS .so /usr/lib/tmac/tmac.s
.	\" to foil ms
.if !'\*(d)'' \{\
.	tm You're trying to load the coversheet macros twice - havoc will result
.	tm I'm quitting to keep you from wasting paper
.	ex \}
.rn FE F6
.nr ST 0
.nr CS 1
.ch NP 16i
.ch FO 16i
.ch FX 16i
.ch BT 16i
.nr FM .01i
.nr 1T 1
.nr BE 1
.nr PI 5n
.if !\n(PD .nr PD 0.3v
.pl 11i
.de FT
.fp 1 H
.fp 2 HI
.fp 3 HB
.fp 4 HX
.ps 10
.vs 12
..
.de FB
.ie !'\\*(TF'' .FP \\*(TF
.el .FP times
.ps 10
.vs 12
..
.FT
.de FE
.F6
.nr F4 +\\n(FP
..
.nr a 0 1
.nr b 0 1
.nr c 0
.nr d 0
.nr e 0
.nr g 0
.nr h 6
.nr i 0
.nr k 0
.nr m 0
.nr n 0 1
.nr o 0
.nr p 1
.nr q 0
.nr r 0
.nr s 0
.nr t 0
.nr u 0 1
.nr v 0
.nr w 0
.nr x 0
.nr y 0
.nr z 0
.nr dv 0
.if '\*(.T'aps' .nr dv 1
'''\"	initialize units per vertical space
.nr l 120
.nr lp 66
.nr np 2 1
.af np i
.nr tp 2 1
.nr tc 2
.af tc i
.nr la 0
.nr a1 0
.nr a2 0
.nr ar 0
.nr u! 1
.nr ud 1
.nr ra 1
.di ZI
.di
.di ZN
.di
.di ZC
.di
.di ZO
.di
.di ZS
.di
.di ZD
.di
'''\"initialize date string - keep for 1st pg of tm
.if \n(mo-0 .ds d) January
.if \n(mo-1 .ds d) February
.if \n(mo-2 .ds d) March
.if \n(mo-3 .ds d) April
.if \n(mo-4 .ds d) May
.if \n(mo-5 .ds d) June
.if \n(mo-6 .ds d) July
.if \n(mo-7 .ds d) August
.if \n(mo-8 .ds d) September
.if \n(mo-9 .ds d) October
.if \n(mo-10 .ds d) November
.if \n(mo-11 .ds d) December
.as d) " \n(dy, 20\n(yr
.ds DY \*(d)
'''			\" initialize strings
.ds m!
.ds m(
.ds n!
.ds n(
.ds o!
.ds o(
.ds x!
.ds y!
.ds z!
'''			\" initialize proprietary notice
.ds o) "Lucent Technologies \(em PROPRIETARY
.ds p) "Use pursuant to Company Instructions
.ds q)
.ds r)
'''			\" initialize trademark symbol
.ds MT \v'-0.5m'\s-4TM\s+4\v'0.5m'
.ds s) 0
'''			\"initialize csmacro version string
.ds ve MCSL (07/12/90)
....in 0
'''\"
'''			\" macros to collect information
'''\"
.de DT 				\" macro for date
'''\"					store date if non-empty
.if !'\\$1'' .ds d) \\$1 \\$2 \\$3 \\$4
.ds DY \\$1 \\$2 \\$3 \\$4
..
.de TI				\" macro for title -TI = mm(TL)
.br
.nr aa 0
.nr TV 1
.ds x( "Mailing Label \}
.in 0
.fi
.ll 4.25i
'''\"					diversion for title ZT = mm(tI)
.di WT
..
.de AH				\" macro for author info AH = mm(AU)
'''\"		name loc dept ext room mail_addr company
'''\"					don't count author unless non-empty
.br
.di
.if !'\\$1'' .nr u \\n+u
.nr AV \\nu
.if \\nu=1 \{\
.	br
'''\"					end title diversion on first author
.	di
.	nr o \\n(dn
.	ll
.	nf
.	ds d! \\$3
.	nr m2 \\$3/10 \}
.ta 0.3i 3.i 4i 5.5i
.br
.ds D\\nu \\$2 \\$3
.ds \\nuL \\$5, x\\$4
'''\"					append to author list ZI = mm(aV)
.da ZI
	\\$1	\\$2 \\$5	\\$4	\\$7
.br
.da
.if !'\\$6'' \{\
.da ZI
	(\\$6)
.br
.da \}
'''\"					end append; info for signature lines
.AA \\nu "\\$1" \\$3 \\$2 "\\$4" \\$5 \\$6
.ta 0.5i 1.0i 1.5i 2.0i 2.5i
..
.de AA
.\"ft 3
.di M\\$1
\\$2
.di
.di A\\$1
\\$2
.if !'\\$3'' Org. \\$3
.if !'\\$4'' \\$4 \\$6
.if !'\\$5'' \\$5
.if !'\\$7'' \\$7
.sp .05i
.br
.di
.ft
.sy echo \\$2 >/tmp/tt\\n($$
.sy sed -f /usr/lib/tmac/name.sed /tmp/tt\\n($$ >/tmp/tx\\n($$
.so /tmp/tx\\n($$
.sy rm /tmp/tx\\n($$ /tmp/tt\\n($$
.if \\$1<2 .ds T1 \\*(T1-gre
.da G9
\\$4-\\$3-\\*(T1
.br
.da
.if !'\\*(d!'\\$3' \{\
.	nr u! \\n(u!+1
.	nr m3 \\$3/10
.	if !\\n(m2=\\n(m3 .nr ud \\n(ud+1 \}
.ie \\$1<4 \{\
.	as m! \\l'2.25i'	
.	as m( 	\\$2 \}
.el .ie \\$1<7 \{\
.	as n! \\l'2.25i'	
.	as n(	\\$2 \}
.el \{\
.	as o! \\l'2.25i'	
.	as o(	\\$2 \}
..
.de TO	\"begin list of im to people
.nr TO 1
.di 1T
..
.de ET		\"end list of im to people & output pg
.di
.SR
..
.de AP				\" at&t responsible person AP = mm(rP)
.br
.di
.if !'\\$1'' .ds r( \\$1
.rm AP
..
.de SA				\" macro for abstract info SA = mm(AS)
.br
.ie \\n(.$ \{\ 
.ds a( \\$1
.if '\\$1'no' .ds a(\}
.el .ds a( ABSTRACT
.nr CS 0
.di
.fi
.nr LL 7.0i
.FB
.ft 1
.di WB				\" WB = mm(aS)
..
.de SE 				\" macro for end of abstract info SE = mm(AE)
.br
.di
.nr la \\n(dn
.nr CS 1
.ll
.FT
.ft 1
.nf
..
.de KW				\" macro for keyword info KW = mm(OK)
.ds k)
.if !'\\$1'' .as k) \\$1
.if !'\\$2'' .as k); \\$2
.if !'\\$3'' .as k); \\$3
.if !'\\$4'' .as k); \\$4
.if !'\\$5'' .as k); \\$5
.if !'\\$6'' .as k); \\$6
.if !'\\$7'' .as k); \\$7
.if !'\\$8'' .as k); \\$8
.if !'\\$9'' .as k); \\$9
'''\"					set k flag if we have some keywords
.ie !'\\*(k)'' .nr k 1
.el .nr k 0
..
.de TY				\" macro for document type TY = mm(MT)
.if '\\$1'TM' \{\
.	nr m 1
.	ds t) TM
.	ds QF TECHNICAL MEMORANDUM
.	ds t( "for Technical Memorandum \}
.if '\\$1'IM' \{\
.	nr m 2
.	ds t) IM
.	ds QF INTERNAL MEMORANDUM
.	ds t( "for Internal Memorandum \}
.if '\\$1'TC' \{\
.	nr m 3
.	ds t) TC
.	ds QF TECHNICAL CORRESPONDENCE
.	ds t( "for Technical Correspondence \}
.ie '\\$2'y' .nr s 1
.el .nr s 0
..
.de NU				\" macro for document number info NU = mm(dN fC wP)
.ie \\ns=1 .ds s( S
.el .ds s(
.ie \\n(wp=0 \{\
.	ie '\\$5'' .ds CX 000000-0000
.	el .ds CX \\$5
.	ds X1 \\*(CX
.	nr wp \\n(wp+1 \}
.el \{\
.	ds CX \\$5
.	if !'\\$5'' \{\
.		if \\n(wp=1 .ds X2 \\*(CX
.		if \\n(wp=2 .ds X3 \\*(CX
.		if \\n(wp=3 .ds X4 \\*(CX
.		if \\n(wp=4 .ds X5 \\*(CX
.		nr wp \\n(wp+1
.		ds x) s\}\}
.if !'\\$4'' \{\
.	ie !\\n(fc=0 \{\
.		if \\n(fc=1 .ds F2 \\$4
.		if \\n(fc=2 .ds F3 \\$4
.		if \\n(fc=3 .ds F4 \\$4
.		if \\n(fc=4 .ds F5 \\$4
.		ds w) s
.		nr fc \\n(fc+1 \}
.	el \{\
.		ds F1 \\$4
.		nr fc \\n(fc+1 \} \}
.ie !'\\$1'' \{\
.	ds NN \\$1-\\$2-\\$3\\*(t)\\*(s(
.	if \\nn=0 .ds N1 \\*(NN
.	if \\nn=1 \{\
.		ds v) s
.		ds N2 \\*(NN\}
.	if \\nn=2 .ds N3 \\*(NN
.	if \\nn=3 .ds N4 \\*(NN
.	if \\nn=4 .ds N5 \\*(NN
.	ie \\nn<4 .as x! \\l'2.25i'	
.	el ie \\nn<7 .as y! \\l'2.25i'	
.	el .as z! \\l'2.25i'	
.	nr n \\n+n \}
.if !'\\$6'' \{\
.	ie !\\n(a!=0 \{\
.		if \\n(a!=1 .as Z1 "	\\$6
.		if \\n(a!=2 .as Z1 \\$6
.		nr a! \\n(a!+1 \}
.	el \{\
.		ds Z1 "	\\$6
.		nr a! \\n(a!+1 \} \}
.el .ds NN
.ta 0.8i 3.5i 5.55i
.br
.da ZN				\" ZN = mm(dM fC wO)
	\\*(NN	\\$4	\\*(CX
.br
.da
.ta 0.5i 1.0i 1.5i
..
.de MY				\" macro for mercury selections MY = mm(mE)
.ds a)
.ds b)
.if '\\$1'y' \{\
.	as a) "	CHM - Chemistry and Materials
.	nr b \\n+b \}
.if '\\$2'y' \{\
.	as a) "	CMM - Communications
.	nr b \\n+b \}
.if '\\$3'y' \{\
.	as a) " 	CMP - Computing
.	nr b \\n+b
.	if \\nb=3 .rn a) b) \}
.if '\\$4'y' \{\
.	as a) "	ELC - Electronics
.	nr b \\n+b
.	if \\nb=3 .rn a) b) \}
.if '\\$5'y' \{\
.	as a) "	LFS - Life Sciences
.	nr b \\n+b
.	if \\nb=3 .rn a) b) \}
.if '\\$6'y' \{\
.	as a) "	MAS - Mathematics and Statistics
.	nr b \\n+b
.	if \\nb=3 .rn a) b) \}
.	if \\nb<6 \{\
.	if '\\$7'y' \{\
.		as a) "	PHY - Physics
.		nr b \\n+b
.		if \\nb=3 .rn a) b) \} \}
.	if \\nb<6 \{\
.	if '\\$8'y' \{\
.		as a) "	MAN - Manufacturing
.		nr b \\n+b \} \}
.if \\nb=3 .rn b) a)
..
.de RL				\" lucent distribution ok RL = mm(rA or fA)
.if '\\$1'n' .nr ra 2
.rm RL
..
.de ED				\" earlier document number ED = mm(eD)
.if !'\\$1'' .ds e( \\$1
.rm ED
..
.de PR				\" macro for proprietary marking PR = mm(PM)
.if '\\$1'BP' .nr p 1
.if '\\$1'BR' \{\
.	nr p 2
.	ds o) "Lucent Technologies \(em PROPRIETARY (RESTRICTED)
.	ds p) "Solely for authorized persons having a need to know
.	ds q) "     pursuant to Company Instructions \}
.if '\\$1'0' .nr p 0
..
.de GS				\" GS = mm(gS)
.nr r 1
..
.de CI
..
.de XE
'''\"				basic distribution leng-to be tailored-set e & h
.if \\nd=0 \{\
.	nr d 1
.	if \\nr=0 .nr e \\ne+2
.	if \\nu<=3 .nr e \\ne+3
.	if \\nu<=6 .nr e \\ne+3
.	if \\nu<=9 .nr e \\ne+3
.	if \\nn<=3 .nr e \\ne+2
.	if \\nn<=6 .nr e \\ne+2
.	if \\nn<=9 .nr e \\ne+2
.	if \\n(ra<=2 .nr e \\ne-3
.	nr e \\ne+27
.	nr h \\ne \}
..
.de CO				\" macro for complete copy addressees CO = mm(cC)
.XE
.ta 2.0i
.nf
.br
.ie \\ne>0 \{\
.	da ZC				\" ZC = mm(cA)
.	ds y( \\$1
.	ie '\\$1'y' .so /usr/lib/tmac/complet.1127
.	el .if  !'\\$1'' .so /usr/lib/tmac/complet.\\*(y(
.	dt \\ne OC \}
.el .da ZC
..
.de OC				\" macro for complete copy overflow - OC = mm(cD)
.ta 2.0i
.br
.da
.da ZO				\" ZO = mm(cO)
.ie \\n(ar>0 .dt \\n(arv ZW
.el .dt 55 ZW
..
.de ZW
.br
.da
.ie \\n(dn>0 .g (\\n(dn)/\\nl+4
.da CZ
..
.de CV				\" macro for cover sheet only addresses CV = mm(cS)
.XE
.ta 2.0i
.nf
.br
.ie \\nh>0 \{\
.	da ZS				\" ZS = mm(dA)
.	ds y( \\$1
.	ie '\\$1'y' .so /usr/lib/tmac/cover.1127
.	el .if !'\\$1'' .so /usr/lib/tmac/cover.\\*(y(
.	dt \\nh OV \}
.el .da ZD
..
.de OV				\" macro for cover sheet only overflow OV = mm(cT)
.ta 2.0i
.br
.da
.da ZD				\" ZD = mm(cO)
.ie \\n(ar>0 .dt \\n(arv WW
.el .dt 55 WW
..
.de WW
.br
.da
.if \\n(dn>0 .nr i (\\n(dn)/\\nl+4	\" was ie with no el
.da DZ
..
.de CE				\" ending all distribution diversions CE = mm(cE)
.br
.if "\\n(.z"ZC" \{\
.	nr g 0
.	rm OC \}
.if "\\n(.z"ZO" \{\
.	nr g -1
.	rm OC \}
.if "\\n(.z"CZ" \{\
.	nr g -2
.	rm OC \}
.if "\\n(.z"ZS" \{\
.	nr i 0
.	rm OV \}
.if "\\n(.z"ZD" \{\
.	nr i -1
.	rm OV \}
.if "\\n(.z"DZ" \{\
.	nr i -2
.	rm )V \}
.da
.if \\ng=-1 \{\
.	ie \\n(dn>0 .nr g (\\n(dn)/\\nl+4
.	el .nr g 0 \}
.if \\ni=-1 \{\
.	ie \\n(dn>0 .nr i (\\n(dn)/\\nl+4
.	el .nr i 0 \}
..
'''\"
'''\" macros to help format document
'''\"
.de HD
.po .5i	\"was .25
.if "\\*(.T"aps" .tl '--''--'
.sp|0.2i
..
.de FC				\" footer macro FC = mm(fO)
.pl 11.0i
'bp
..
.de ST			\" macro for abstract overflow trap ST = mm(yY)
.ZB
.rm ST				\" ZB = mm(aT)
..
.de ZB
.ch ST 16i			\" macro for abstract overflow trap ZB = mm(aT)
.if \\na>0 \{\
.	ft 2
.	ce
(continued)
.	ft 1 \}
.pl 11.0i
.nr a \\n+a
.rn ZB XX
'bp
.rn XX ZB
.wh -0.35i ZB
.HC				\" HC = mm(cH)
.HX				\" HX = mm(tH)
'sp 0.05i
.ce
.ft 3
Abstract (continued)
.ft 1
.in 0.2i
'sp 1
.FB
..
.de TK				\" macro for thick lines TKK = mm(tK)
.ps 24
\l'7.5i'
.ps
..
.de HX				\" macro for Title headings and text HX = mm(tH)
.TK
'sp 0.05i
'''\"					mark t - Title heading
.mk t
.ft 3
Title:
.ft
'sp|\\ntu
.in 0.7i
.WT
.in 0
.ta 0.5i
.nr q \\no/\\nl
.ie \\nq>2 'sp|\\ntu+\\nq
.el 'sp|\\ntu+2
.TK
.					\" m1 - mark end of title section - save
.mk m1
..
.de HC				\" macro for continuation header HC = mm(cH)
.nr np \\n+(np
.nf
.in 0
.FT
.ft 3
.ta 4.80i
.nr tc \\n(tp
	\\*(N1\f2 (page \\n(np of \\n(tc)
.sp 0.1i
..
.de DL				\" macro for distribution list headers DL = mm(dH)
.ft 3
.ta 1.0i 4.75i
	\\$1	\\$2
.sp 0.05i
.ft 1
.ta 0.5i 1.0i
..
.de EJ				\" macro for ejecting continuation page EJ = mm(eP)
'bp
.wh 0 HD
'''\"				put out continuation page header & title section
.HC
.HX
..
.de CP				\" macro for continuation page CP = mm(cP)
'''\"					calc vert. units for cc overflow (if any)
.ie \\nv<=\\n(.t .nr v 1
.el .nr v 0
.if \\nv=1 .if \\nw<=\\n(.t .nr v 2
.				\" check if cont page needs to be ejected
.in 0
.if \\nc=1 \{\
.				\" - if no abstract overflow
.	if \\na=0 .EJ
.	if \\na>0 \{\
.				\"or if abstract over but no room for list overfl
.		ie \\nv<2 .EJ
.				\" just tk line if abstract over & room for list
.		el .TK \}
.	FT
.	ie \\ne=0 \{\
.		if \\ng>4 .if \\ni>4 .DL "Complete Copy" "Cover Sheet Only"
.		if \\ng>4 .if !\\ni>4 .DL "Complete Copy" ""
.		if !\\ng>4 .if \\ni>4 .DL "" "Cover Sheet Only" \}
.	el \{\
.		if \\ng>4 .if \\ni>4 .DL "Complete Copy (continued)" "Cover Sheet Only (continued)"1
.		if \\ng>4 .if !\\ni>4 .DL "Complete Copy (continued)" ""
.		if !\\ng>4 .if \\ni>4 .DL "" "Cover Sheet Only (continued)" \}
.	mk z
.	nr q \\n(.t/\\nl
'''\"					put out complete copy list overflow
.	in 0.2i
.	ZO
.	in 0
.	mk x
.	sp|\\nzu
.	in 4i
'''\"					put out cover sheet list overflow
.	ZD
.	mk y
.	in 0
.	if \\nx-\\ny .sp|\\nxu
.	TK \}
..
.de ZP					\"compute total pages and diversion lengths
'''\"				calculate vert. units for cc overflow (if any)
.ie \\ng>4 .nr v (\\ng)*\\nl
.el .nr v 0
'''\"					also for cs overflow (if any)
.ie \\ni>4 .nr w (\\ni)*\\nl
.el .nr w 0
.ie \\nv>\\nw .nr j \\nv
.el .nr j \\nw
.					\" set c=1 if either g or i >0
.if \\ng>4 .nr c 1
.if \\ni>4 .nr c 1
.					\" calculate total pages in job (default 2)
.					\" a1 - page 1 portion abstract (units)
.nr a1 \\nyu-\\nxu-1v
.ie \\n(la>\\n(a1 \{\
.					\" ar - remainder abstract (units)
.	nr ar \\n(la-\\n(a1
.	nr tp \\n+(tp
.					\" a2 - available continuation page space
.					\" m1 is mark after tk line after title
.					\" 2v for Abstract (continued) + one blank
.	nr a2 11.0i-\\n(m1-2v
.	ZZ \}
.el .if \\nc>0 .nr tp \\n+(tp
..
.de ZZ				\" ZZ = mm(t1)
.ie \\n(ar>\\n(a2 \{\
.	nr ar \\n(ar-\\n(a2
.	nr tp \\n+(tp
.	ZZ \}
.el .if \\n(ar+\\nj>\\n(a2 .nr tp \\n+(tp
..
'''\"
'''\" main macro to handle output of cover sheet
'''\"		mm(CS)
.de SC
.nr CS 0
.nr ST 1
.if \\nu=0 \{\
.	tm WARNING: author must be supplied \}
.if \\no=0 \{\
.	tm WARNING: document title must be supplied \}
.if \\nm=0 \{\
.	tm WARNING: memorandum type undefined or unknown \}
.if \\nm=1 .if \\nb=0 \{\
.	tm WARNING: technical memoranda must have at least one mercury class \}
.if \\nn=0 \{\
.	tm WARNING: document number must be supplied \}
.XE
.ll 7.5i
.ft 1
.if \\n(nl .bp
.in 0
.HD
'''\" the rs is to restore spacing - ditches big space at top
.rs
.sp1
.sp 0.05i
.nf
.ps 16
.ft 3
.ta 4.85i
.					\" put out page 1 heading
	Document Cover Sheet
.wh 0 HD
.sp 0.1i
.ta 0.15i 4.55i
	\s36\(FA\s0	\\*(t( 
.ft
.ps 10
.HX
.sp 0.05i
.ft 3
.ie \\nu>1 .ds u) s
.el .ds u)
.ta 0.5i 3.0i 3.95i 5.25i
	Author\\*(u) (Computer Address)	Location	Phone Number	Company (if other than BL)
.ft
'''\"					output author info
.ZI
.if !'\\*(r('' \{\
.	ta 0.3i 2.6i
	\\*(r(	(Responsible BL Person) \}
.TK
.sp 0.05i
.ft 3
.ta 1.0i 3.3i 5.55i
	Document No\\*(v).	Filing Case No\\*(w).	Project No\\*(x).
.ft
.sp 0.05i
'''\"					output document number
.ZN
.TK
'''\"					output keywords if they exist
.if \\nk>0 \{\
.	ft 3
Keywords:
.	ft
.	sp 0.05i
.	ti 0.2i
\\*(k)
.	TK \}
'''\"					output mercury info if it exists
.if \\nb>0 \{\
.	ft 3
MERCURY Announcement Bulletin Sections
.	ft
.	sp 0.05i
.	ta 0.6i 3.1i 5.6i
.	ps 8
.	if \\nb>3 \\*(b)
\\*(a)
.	ps
.	TK \}
.ft 3
Abstract
.ft
.mk x
.nr b1 \\nx/\\n(.v+1
.nr b2 (\\n(b1*\\n(.v)-\\nx
.sp \\n(b2u
.mk x
'''\" calculate position (19v includes 2v to print version at bottom of page)
.nr y \\n(lpv-19v
.if \\n(F4>0 .nr y \\ny-\\n(F4
.sp|\\nyu
.sp -1
.ZP
'''\"					handle abstract page 1 continuation
.ie \\n(la>\\n(a1 \{\
.	ce
.	ft2
(continued on page iii)
.	ft1
.	br \}
.el .sp1
.if \\n(F4>0 \{\
.	FA
.	FG \}
.TK
.ps 8
.vs 10
.nr qq \\$1+\\$2+\\n(tp
\f3Total Pages\f1 (including document cover sheet):  \\s+2\\n(qq\\s-2
.ie !'\\*(e('' \{\
Supersedes or amends document number \\*(e(. \}
.el .sp
.ps
.vs
.mk z
.sp .67i
'''\"					output proprietary notice if it exists
.if \\np>0 \{\
.ft 2
.ti (4i-\\w'\\*(o)'u)/2u
\\*(o)
.ft
.ti (4i-\\w'\\*(p)'u)/2u
\\*(p)
.ti (4i-\\w'\\*(q)'u)/2u
\\*(q) \}
.sp |\\nzu+11v
.ta 5.35i
\\s8\\*(ve\\s0
.ie !'\\*(s)'' \{\
\s8Timestamp: \\*(s)\s0	BELL LABORATORIES \}
.el \{\
	BELL LABORATORIES \}
.sp|\\nzu
.sp 1
.ft 3
.ti 5.25i
\\*(x(
.ft
.sp|\\nxu
.in 0.2i
.nf
'''\"					abstract
.if !\\n(la=\\n(a1 \{\
.	wh -0.25i ST \}
.pl \\nyu
.ta 0.5i 1.0i 1.5i 2.0i 2.5i
.FB
.ft 1
'''\"					output the abstract
.WB
.if \\n(la=\\n(a1 .sp-1
.rn ZB XX
.wh -0.25i FC
'''\"					output continuation page
.CP
'bp
.FT
.ft 1
.in 0
.wh 0 HD
.nf
.ft 3
.ta 5.00i
.nr tc \\n(tp
Initial Distribution Specifications	\\*(N1\f2 (page ii of \\n(tc)\f3
.ft 1
.TK
.if \\ne>0 \{\
.	DL "     Complete Copy" "     Cover Sheet Only"
.	mk z
'''\"					put out complete copy list
.	in 0.2i
.	ZC
.	in 0
.	if !\\ng=0 \{\
.		ft 2
.		ti 1.25i
(continued)
.		ft 1 \}
.	sp|\\nzu
.	in 4i
'''\"					put out cover sheet list
.	ZS
.	if !\\ni=0 \{\
.		ft 2
.		ti 4.75i
(continued)
.		ft 1 \}
.	in 0
'''\"					starter space value - then tailor
.	sp|5
.	sp \\ne
.	TK \}
.if \\nr=1 \{\
\f3Government Security Classified\f1
.	ft 1
.	sp -0.05i
.	TK \}
\f3Future Lucent Technologies Distribution by ITDS\f1
.ti 0.5i
.sp 0.05i
.ie \\n(ra=2 \{\
\f3DO NOT RELEASE\f1 to any Lucent Technologies employee without appropriate approval for each request. \}
.el \{\
\f3RELEASE\f1 to any Lucent Technologies employee (excluding contract employees). \}
.TK
'''\"					put out author signature section
.ft 3
Author Signature\\*(u)
.ft 1
.sp1
.ta 2.635i 5.25i
\\*(m!
.ta 0.25i 2.875i 5.5i
\\*(m(
.if \\nu>3 \{\
.	sp 0.1i
.	ta 2.635i 5.25i
\\*(n!
.	ta 0.25i 2.875i 5.5i
\\*(n( \}
.if \\nu>6 \{\
.	sp 0.1i
.	ta 2.635i 5.25i
\\*(o!
.	ta 0.25i 2.875i 5.5i
\\*(o( \}
.sp -0.1i
.TK
'''\"					output organizational approval section
.ie \\np>1 \{\
\f3Organizational Approval\f1  (Department Head approval \f3required\ff1 for \f2\\*(o)\f1.) \}
.el \{\
\f3Organizational Approval\f1  (Optional) \}
.sp 1
.ta 2.635i 5.25i
\\*(x!
.	ta 0.25i 2.875i 5.5i
\\*(Z1
.if \\nn>3 \{\
.	sp .1i
.ta 2.635i 5.25i
\\*(y! \}
.if \\nn>6 \{\
.	sp .1i
\\*(z! \}
.sp -0.1i
.TK
'''\"					recipient section always output
.ft 3
For Use by Recipient of Cover Sheet:
.ft 1
.ps -3
.vs -4
.sp.05i
.mk z
 Computing network users may order copies via the \f2library \-k\f1 command;
  for information, type \f2man library\f1 after the UNIX prompt.
'''.sp1
.rn fo xx
 Otherwise:
  Enter PAN if BL (or SS# if non-BL). \l'1.5i'
  Return this sheet to any ITDS location.
.sp|\\nzu
.in 4i
Internal Technical Document Service
'''.sp1
.ta 1i 2i 3i
( ) AK 2H-28	( ) IH 7M-103	( ) DR 2F-19	( ) NW-ITDS
( ) ALC 1B-102	( ) MV 1L-19	( ) INH 1C-114	( ) PR 5-2120
( ) CB 30-2011	( ) WH 3E-204	( ) IW 2Z-156
( ) HO 4F-112		( ) MT 3B-117
.in
.ps
.vs
.SR
..
.	\"IZ - initialization
.de IZ
.FB
.nr TN 0
.em EM
.po 1i
.nr PO 1i
.if \\n(FM=0 .nr FM 1i
.nr YY 0-\\n(FMu
.if !\\n(PD .if n nr PD 1v
.if t .if !\\n(PD .nr PD 0.3v
.wh 0 NP
.wh \\n(.pu-\\n(FMu FO
.ch FO 16i
.wh \\n(.pu-\\n(FMu FX
.ch FO \\n(.pu-\\n(FMu
.if t .wh -\\n(FMu/2u BT
.if n .wh -\\n(FMu/2u-1v BT
..
.\"		macro to restore ms foiling
.de SR
.nr BE 0
.nr 1T 1
.nr FM 0
.nr PD 0
.nr HM 0
.nr KG 0
.nr FP 0
.nr GA 0
.nr FP 0
.\" changed rn F5 FE added rn FJ FS
.rn FK FE
.rn FJ FS
.if '\\$1'' .bp
.if !'\\$1'' \{\
.di ZA
.ce
\\*(a(
.sp
.WB
.di
.rn ZA WB\}
.nr FC -1
.nr % 1
.IZ
.rm IZ
.if '\\$1'' .RT
.ds MN \\*(N1 \\*(N2 \\*(N3 \\*(N4 \\*(N5
.nr MM \\nn
.nr MC \\n(fc
.nr MG \\n(wp
.nr NA \\nu
.if '\\n(ST'1' \{\
'''.so /usr/lib/tmac/tmac.rscover XXX
.so \*(.P/lib/tmac/tmac.rscover
. \" a line for troff to eat
.S1 \}
.ll 6i
.nr LL 6i
.rr a b c d e f g h i j k
.rr l m n o p q r s t u
.rr v w x y z np tp nc tc
.rr ud u! m2 dv
.rr lp np la a1 a2 ar wp fc m1
.rm DT TI AH SE KW TY NU MY
.rm PR CI CO OC CV OV CE HD
.rm FC ST TK HX HC DL EJ
.rm CP SC a) b) k) N1 p) q) r)
.rm N2 N3 N4 N5 X1 X2 X3 X4 X5
.rm X1 X2 X3 X4 X5 F1 F2 F3 F4
.rm F5
.rm d) o) s) ve m! n! o! e( r(
.rm x! y! z! x( d! ve u)
.rm t) w) x) y) z) a( b( c( m(
.rm n( o( p( s( t( SA ZI ZC ZO
.rm ZS ZD ZN FT FB CX NN GS
.rm ZB XX ZP ZZ
.rm TM IM MF MR LT OK RP TR
.rm TX AU AX AI AE SY S2 S0
.rm S3
..
.de RP
.nr ST 2
.SS
..
.de TR
.nr ST 3
.ds MN \\$1
.SS
..
.de SS		\"RP or TR rename the world so old macros called
.rm SG
.nr CS 0
.nr BE 0
.nr 1T 0
.nr FM 0
.nr PD 0
.nr HM 0
.nr KG 0
.nr FP 0
.nr GA 0
.nr FP 0
.ll 6i
.nr LL 6i
.\" changed rn F5 FE added rn FJ FS
.rn F6 FE
.nr FC -1
.nr % 1
.IZ
.rm IZ
.pn 0
.de TI
.TL
\\..
.de SA
.AB \\\\$1
\\..
.de SE
.AE
\\..
.de AH
.AU
\\\\$1
\\..
.de DT
.ND \\\\$1 \\\\$2 \\\\$3
\\..
.br
.rr a b c d e f g h i j k
.rr l m n o p q r s t u
.rr v w x y z np tp nc tc
.rr ud u! m2 dv
.rr lp np la a1 a2 ar wp fc m1
.rm DT KW TY NU MY
.rm PR CI OC OV HD
.rm FC ST TK HX HC DL EJ
.rm CP SC a) b) k) N1 p) q) r)
.rm d) o) s) ve m! n! o! e( r(
.rm x! y! z! x( d! ve u)
.rm t) w) x) y) z) a( b( c( m(
.rm n( o( p( s( t( ZI ZC ZO
.rm ZS ZD ZN FT FB CX NN GS
.rm ZB XX ZP ZZ
.rm TM IM MF MR LT
..
