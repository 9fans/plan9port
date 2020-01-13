.lg 0
.ds sd #9/tmac
.\"	RT -  reset everything to normal state
.de RT
.if \\n(CS \{\
.SR 1
.BG\}
.if !\\n(1T .BG
.ce 0
.if !\\n(IK .if !\\n(IF .if !\\n(IX .if !\\n(BE .if !\\n(FT .di
.ul 0
.if \\n(QP \{\
.	ll +\\n(QIu
.	in -\\n(QIu
.	nr QP -1\}
.if \\n(NX<=1 .if \\n(AJ=0 .if \\n(FT=0 .ll \\n(LLu
.if !\\n(IF \{\
.	ps \\n(PS
.	ie \\n(VS>=41 .vs \\n(VSu
.	el .vs \\n(VSp\}
.ie \\n(IP \{\
.	in \\n(I\\n(IRu
.	nr IP -1\}
.el .if !\\n(IR \{\
.	nr I1 \\n(PIu
.	nr I2 0
.	nr I3 0
.	nr I4 0
.	nr I5 0\}
.ft 1
.ta 5n 10n 15n 20n 25n 30n 35n 40n 45n 50n 55n 60n 65n 70n 75n 80n
.hy \\n(HY
.fi
..
.	\"IZ - initialization
.de IZ
.so \\*(sd/tmac.sdisp
.nr TN 0
.em EM
. \"  ACCENTS  say \*'e or \*`e to get e acute or e grave both were 4/10
.ds ' \h'\w'e'u*1/10'\z\(aa\h'-\w'e'u*1/10'
.ds ` \h'\w'e'u*2/10'\z\(ga\h'-\w'e'u*2/10'
. \"  UMLAUT  \*:u, etc.
.if t .ds : \\v'-0.6m'\\h'(1u-(\\\\n(.fu%2u))*0.13m+0.00m'\\z.\\h'0.2m'\\z.\\h'-((1u-(\\\\n(.fu%2u))*0.13m+0.20m)'\\v'0.6m'
.if n .ds : \z"
. \" TILDE and CIRCUMFLEX
.ds ^ \\\\k:\\h'-\\\\n(.fu+1u/2u*2u+\\\\n(.fu-1u*0.13m+0.06m'\\z^\\h'|\\\\n:u'
.ds ~ \\\\k:\\h'-\\\\n(.fu+1u/2u*2u+\\\\n(.fu-1u*0.13m+0.06m'\\z~\\h'|\\\\n:u'
.	\" czech v symbol
.ds v \\\\k:\\\\h'+\\\\w'e'u/4u'\\\\v'-0.6m'\\\\s6v\\\\s0\\\\v'0.6m'\\\\h'|\\\\n:u'
.		\" cedilla
.ds , \\\\k:\\\\h'\\\\w'c'u*0.4u'\\\\z,\\\\h'|\\\\n:u'
.so \\*(sd/tmac.srefs
.ch FO \\n(YYu
.if !\\n(FM .nr FM 1i
.nr YY -\\n(FMu
.nr XX 0 1
.nr IP 0
.nr PI 5n
.nr QI 5n
.nr I0 \\n(PIu
.nr PS 10
.nr VS 12
.nr HY 14
.ie n \{\
.	if !\\n(PD .nr PD 1v
.	nr DV 1v\}
.el \{\
.	if !\\n(PD .nr PD 0.3v
.	nr DV .5v\}
.nr ML 3v
.ps \\n(PS
.ie \\n(VS>=41 .vs \\n(VSu
.el .vs \\n(VSp
.nr IR 0
.nr I0 0
.nr I1 \\n(PIu
.nr TB 0
.nr SJ \\n(.j
.nr LL 6i
.ll \\n(LLu
.nr LT \\n(.l
.lt \\n(LTu
.ev 1
.if !\\n(FL .nr FL \\n(LLu*11u/12u
.ll \\n(FLu
.ps 8
.vs 10p
.ev
.if \\*(CH .ds CH "\(hy \\\\n(PN \(hy
.wh 0 NP
.wh -\\n(FMu FO
.ch FO 16i
.wh -\\n(FMu FX
.ch FO -\\n(FMu
.if t .wh -\\n(FMu/2u BT
.if n .wh -\\n(FMu/2u-1v BT
. \" no overstriking bold or italic; switch underlining to bold italic
. \" (sad historical botch, the .uf font must be 2, 3, or 4)
.if n .uf 4
.if n .bd 3
.nr CW 0-1
.nr GW 0-1
..
.de TM
.if !\\n(IM .if !\\n(MN .pn 0
.so \\*(sd/tmac.scover
.if !\\n(IM .if !\\n(MN .rm IM MF MR
.if n .if !\\n(.T .pi /usr/bin/col
.nr ST 1
.ds QF TECHNICAL MEMORANDUM
.br
.ds MN \\$1
.if !"\\$1"" .nr MM 1
.if !"\\$2"" .nr MC 1
.if !"\\$3"" .nr MG 1
.nr TN 1
.if \\n(.$-1 .ds CA \\$2
.if \\n(.$-2 .ds CC \\$3
.rm RP S0 S2 AX
..
.		\" IM - internal memorandum
.de IM
.nr IM 1
.TM "\\$1" "\\$2" "\\$3"
.rm QF
.RA
.rm RA RP MF MR
..
.		\" MF - memorandum for file.
.de MF
.nr MN 1
.TM "\\$1" "\\$2" "\\$3"
.rm MR
.rm IM
.RA
.rm RA RP TM
..
.		\" MR - memo for record
.de MR
.nr MN 2
.TM "\\$1" "\\$2" "\\$3"
.ds QF MEMORANDUM FOR RECORD
.rm MF
.RA
.rm RA RP IM TM
..
.	\" LT - letter
.de LT
.if !\\n(PO .ie n .nr PO 1.5i
.el .nr PO 1.3i
.po \\n(POu
.LP
.rs
.if !"\\$1"" \{\
.	vs -2p
.if "\\$1"LT" .ta 3.9i 4.45i
.if !"\\$1"LT" .ta 3.9i 4.45i
.	sp .2i
.	nf
.	if "\\$1"LT" 	\s36\(FA\s0
.	if !"\\$1"LT" 	\s36\(LH\s0
.	br
\s7\l'7i'\s0
.sp
.	br
.	if !"\\$2"" .ds xR "		\\$2
.	ds xP 908-582-3000
.	if !"\\$3"" .ds xP \\$3
.	if "\\$1"LT" \s8\f(HBBell Laboratories\fP		\fH600 Mountain Avenue
.	if !"\\$1"LT" \s8\f(HBBell Laboratories\fP		\fH600 Mountain Avenue
.	if !"\\$2"" \\*(xR
		Murray Hill, NJ 07974-0636
		\\*(xP
.	if !"\\$4"" 		\\$4
.	if !"\\$5"" 		\\$5
.	if !"\\$6"" 		\\$6
.	if !"\\$7"" 		\\$7
.ft 1
.ps
.	sp -.75i
.	vs
.	fi \}
.if n \{\
.	sp 1i
.	in 4.55i\}
.if t \{\
.	sp 1.45i
.	in 3.5i\}
.ll 8i
\\*(DY
.ll
.in 0
.br
.if t .sp 3
.if n \{\
.	sp
.	na\}
.nf
.rm CF
.de SG	\" nested defn
.sp 2
.ta 3.5i
	Sincerely,
.sp 3
	\\\\$1
.ds CH
\\..
..
.de OK
.br
.di
.di OD
..
.de RP		\" released paper
.nr ST 2
.pn 0
.rm SG CS TM QF IM MR MF EG
.br
..
.de TR		\" Comp. Sci. Tech Rept series.
.nr ST 3
.pn 0
.ds MN \\$1
.rm SG CS TM QF IM MR M EG
.br
..
.	\"FP - font position for a family
.de FP
.ds TF \\$1
.if '\\$1'palatino'\{\
.	fp 1 R PA
.	fp 2 I PI
.	fp 3 B PB
.	fp 4 BI PX\}
.if '\\$1'lucidabright'\{\
.	fp 1 R LucidaBright
.	fp 2 I LucidaBright-Italic
.	fp 3 B LucidaBright-Demi
.	fp 4 BI LucidaBright-DemiItalic
.	fp 5 CW LucidaSansCW\}
.if '\\$1'lucidasans'\{\
.	fp 1 R LucidaSans
.	fp 2 I LucidaSansI
.	fp 3 B LucidaSansB
.	fp 5 CW LucidaCW\}
.if '\\$1'luxisans'\{\
.	fp 1 R LuxiSans
.	fp 2 I LuxiSans-Oblique
.	fp 3 B LuxiSans-Bold
.	fp 4 BI LuxiSans-BoldOblique
.	fp 5 CW LuxiMono\}
.if '\\$1'dejavu'\{\
.	fp 1 R DejaVuSerif
.	fp 2 I DejaVuSerifOblique
.	fp 3 B DejaVuSerifBold
.	fp 4 BI DejaVuSerifBoldOblique
.	fp 5 CW DejaVuMonoSans\}
.if '\\$1'dejavusans'\{\
.	fp 1 R DejaVuSans
.	fp 2 I DejaVuSansOblique
.	fp 3 B DejaVuSansBold
.	fp 4 BI DejaVuSansBoldOblique
.	fp 5 CW DejaVuMonoSans\}
.if '\\$1'syntax'\{\
.	fp 1 R Syntax
.	fp 2 I SyntaxI
.	fp 3 B SyntaxB
.	fp 5 CW LucidaCW\}
.if '\\$1'century'\{\
.	ie '\\*(.T'202'\{\
.		fp 1 NR Centsb
.		fp 2 NI CentI
.		fp 3 NB CentB
.		fp 4 NX CentBI\}
.	el \{\
.		fp 1 NR
.		fp 2 NI
.		fp 3 NB
.		fp 4 NX\}\}
.if '\\$1'helvetica'\{\
.	fp 1 H
.	fp 2 HI
.	fp 3 HB
.	fp 4 HX\}
.if '\\$1'bembo'\{\
.	ie '\\*(.T'202'\{\
.		fp 1 B1 Bembo
.		fp 2 B2 BemboI
.		fp 3 B3 BemboB
.		fp 4 B4 BemboBI\}
.	el \{\
.		fp 1 B1
.		fp 2 B2
.		fp 3 B3
.		fp 4 B4\}\}
.if '\\$1'optima'\{\
.	fp 1 R Optima
.	fp 2 I OptimaI
.	fp 3 B OptimaB
.	fp 4 BI OptimaBI\}
.if '\\$1'souvenir'\{\
.	fp 1 R Souvenir
.	fp 2 I SouvenirI
.	fp 3 B SouvenirB
.	fp 4 BI SouvenirBI\}
.if '\\$1'melior'\{\
.	fp 1 R Melior
.	fp 2 I MeliorI
.	fp 3 B MeliorB
.	fp 4 BI MeliorBI\}
.if '\\$1'times'\{\
.	fp 1 R
.	fp 2 I
.	fp 3 B
.	fp 4 BI\}
..
.	\"TL - title and initialization
.de TL
.br
.nr TV 1
.if \\n(IM .rm CS
.if \\n(MN .rm CS
.ME
.rm ME
.di WT
.na
.fi
.ie h .ll \\n(LLu
.el \{\
.ll 5.0i
.if n .if \\n(TN .ll 29
.if t .if \\n(TN .ll 3.5i \}
.ft 3
.ps \\n(PS
.if !\\n(TN \{\
.	ps +2
.	vs \\n(.s+2
.	rm CS\}
.hy 0
.if h .ce 999
..
.de TX
.rs
.sp .5i
.ce 1000
.if n .ul 1000
.ps 12
.ft 3
.vs 15p
.ne 4
.hy 0
.WT
.hy \\n(HY
.ce 0
.ul 0
..
.	\"	AU - author(s)
.de AU
.nr AV 1
.ad \\n(SJ
.br
.di
.br
.nf
.nr NA +1
.ds R\\n(NA \\$1
.ds E\\n(NA \\$2
.di A\\n(NA
.ll \\n(LLu
.ie t \{\
.	ie !\\n(TN .ft 2
.	el \{\
.		ft 3
.		ll 1.4i\}\}
.el \{\
.	ie !\\n(TN .ft 1
.	el \{\
.		ft 3
.		ll 16\}\}
.ps \\n(PS
.if h .ce 999
..
.de AX
.ft 1
.rs
.ce 1000
.if n .ul 0
.ps \\n(PS
.ie \\n(VS>=41 .vs \\n(VSu
.el .vs \\n(VSp
.if t \{\
.	sp
.	A1
.	sp 0.5
.	ns
.	I1
.	if \\n(NA-1 .sp
.	A2
.	if \\n(NA-1 .sp 0.5
.	ns
.	I2
.	if \\n(NA-2 .sp
.	A3
.	if \\n(NA-2 .sp 0.5
.	ns
.	I3
.	if \\n(NA-3 .sp
.	A4
.	if \\n(NA-3 .sp 0.5
.	ns
.	I4
.	if \\n(NA-4 .sp
.	A5
.	if \\n(NA-4 .sp 0.5
.	ns
.	I5
.	if \\n(NA-5 .sp
.	A6
.	if \\n(NA-5 .sp 0.5
.	ns
.	I6
.	if \\n(NA-6 .sp
.	A7
.	if \\n(NA-6 .sp 0.5
.	ns
.	I7
.	if \\n(NA-7 .sp
.	A8
.	if \\n(NA-7 .sp 0.5
.	ns
.	I8
.	if \\n(NA-8 .sp
.	A9
.	if \\n(NA-8 .sp 0.5
.	ns
.	I9\}
.if n \{\
.	sp 2
.	A1
.	sp
.	ns
.	I1
.	if \\n(NA-1 .sp 2
.	A2
.	if \\n(NA-1 .sp
.	ns
.	I2
.	if \\n(NA-2 .sp 2
.	A3
.	if \\n(NA-2 .sp
.	ns
.	I3
.	if \\n(NA-3 .sp 2
.	A4
.	if \\n(NA-3 .sp
.	ns
.	I4
.	if \\n(NA-4 .sp 2
.	A5
.	if \\n(NA-4 .sp
.	ns
.	I5
.	if \\n(NA-5 .sp 2
.	A6
.	if \\n(NA-5 .sp
.	ns
.	I6
.	if \\n(NA-6 .sp 2
.	A7
.	if \\n(NA-6 .sp
.	ns
.	I7
.	if \\n(NA-7 .sp 2
.	A8
.	if \\n(NA-7 .sp
.	ns
.	I8
.	if \\n(NA-8 .sp 2
.	A9
.	if \\n(NA-8 .sp
.	ns
.	I9\}
..
.	\"AI - authors institution
.de AI
.br
.ft 1
.di
.di I\\n(NA
.nf
..
.	\"AB - begin an abstract
.de AB
.br
.di
.ul 0
.ce 0
.nr 1T 1
.nr IK 1
.nr KI 1
.di WB
.rs
.nr AJ 1
.ce 1
.ft 2
.if n .ul
.ll \\n(LLu
.ie \\n(.$ \{\
.	if !"\\$1"-" .if !"\\$1"no" \\$1
.	if !"\\$1"-" .if !"\\$1"no" .sp\}
.el \{\
ABSTRACT
.sp\}
.hy \\n(HY
.ul 0
.ce 0
.fi
.ft 1
.nr OJ \\n(.i
.in +\\n(.lu/12u
.ll -\\n(.lu/12u
.br
.ps \\n(PS
.ie \\n(VS>=41 .vs \\n(VSu
.el .vs \\n(VSp
.ti +\\n(PIu
..
.	\"AE - end of an abstract
.de AE
.br
.di
.ll \\n(LLu
.ps \\n(PS
.ie \\n(VS>=41 .vs \\n(VSu
.el .vs \\n(VSp
.nr 1T 0
.nr IK 0
.in \\n(OJu
.nr AJ 0
.di
.ce 0
.if \\n(ST=2 .SY
.if \\n(ST<3 .rm SY
..
.	\"S2 - release paper style
.	\"SY - cover sheet of released paper
.de SY
.ll \\n(LLu
.ns
.if \\n(TV .TX
.if \\n(AV .AX
.rs
.ce 0
.nf
.sp 3
.ls 1
.pn 2
.WB
.ls
.sp 3v
\\*(DY
.sp |9i
.if \\n(FP .FA
.FG
.if \\n(GA=1 .nr GA 2
.fi
..
.	\"S2 - first text page, released paper format
.de S2
.ce 0
.br
.SY
.rm SY
.bp 1
.if \\n(TV .TX
.if \\n(AV .AX
.rs
.ce 0
.ft 1
.ad \\n(SJ
..
.	\"S0- mike lesk conserve paper style
.de S0
.ce 0
.br
.ll \\n(LLu
.if \\n(TV+\\n(AV .ns
.if \\n(TV .TX
.if \\n(AV .AX
.if \\n(TV+\\n(AV .rs
.ce 0
.if \\n(TV .sp 2
.ls 1
.if \\n(FP \{\
.	FJ
.	nf
.	FG
.	fi
.	FK
.	nr FP 0\}
.nf
.WB
.ls
.fi
.ad \\n(SJ
..
.	\"S3 - CSTR style
.de S3
.rs
.sp |2.25i
.ce 1000
.I1
.if \\n(NA>1 \{\
.	sp .5
.	I2\}
.if \\n(NA>2 \{\
.	sp .5
.	I3\}
.if \\n(NA>3 \{\
.	sp .5
.	I4\}
.if \\n(NA>4 \{\
.	sp .5
.	I5\}
.if \\n(NA>5 \{\
.	sp .5
.	I6\}
.if \\n(NA>6 \{\
.	sp .5
.	I7\}
.if \\n(NA>7 \{\
.	sp .5
.	I8\}
.if \\n(NA>8 \{\
.	sp .5
.	I9\}
.sp |4i
.	\"check how long title is: can space extra .25 inch if short
.di EZ
.WT
.di
.if \\n(dn<1.5v .if \\n(NA=1 .sp .25i
.ft 1
Computing Science Technical Report No. \\*(MN
.sp
.if t .ft 3
.if n .ul 100
.ps 12
.vs 15p
.hy 0
.WT
.hy \\n(HY
.ft 1
.if n .ul 0
.ps 10
.vs 12p
.sp
.ft 1
.A1
.A2
.A3
.A4
.A5
.A6
.A7
.A8
.A9
.ce 0
.sp |8.5i
.ce 0
\\*(DY
.DZ
.bp 0
.ft 1
.S2
..
.	\"SG - signature
.de SG
.br
.KS
.in +2u*\\n(.lu/3u
.sp 4
.A1
.if \\n(NA>1 .sp 4
.A2
.if \\n(NA>2 .sp 4
.A3
.if \\n(NA>3 .sp 4
.A4
.if \\n(NA>4 .sp 4
.A5
.if \\n(NA>5 .sp 4
.A6
.if \\n(NA>6 .sp 4
.A7
.if \\n(NA>7 .sp 4
.A8
.if \\n(NA>8 .sp 4
.A9
.in
.nf
.if \\n(.$<1 .G9
.sp -1
.if \\n(.$>=1 \\$1
.if \\n(.$>=2 \\$2
.if \\n(.$>=3 \\$3
.if \\n(.$>=4 \\$4
.if \\n(.$>=5 \\$5
.if \\n(.$>=6 \\$6
.if \\n(.$>=7 \\$7
.if \\n(.$>=8 \\$8
.if \\n(.$>=9 \\$9
.fi
.br
.KE
..
.	\"Tables.  TS - table start, TE - table end
.de TS
.br
.if !\\n(1T .RT
.ul 0
.ti \\n(.iu
.if t .sp 0.5
.if n .sp
.if \\$1H .TQ
.nr IX 1
..
.de TQ
.di TT
.nr IT 1
..
.de TH
.if \\n(.d>0.5v \{\
.	nr T. 0
.	T# 0\}
.di
.nr TQ \\n(.i
.nr HT 1
.in 0
.mk #a
.mk #b
.mk #c
.mk #d
.mk #e
.mk #f
.TT
.in \\n(TQu
.mk #T
..
.de TE
.nr IX 0
.if \\n(IT .if !\\n(HT \{\
.	di
.	nr EF \\n(.u
.	nf
.	TT
.	if \\n(EF .fi\}
.nr IT 0
.nr HT 0
.if n .sp 1
.if t .sp 0.5
.rm a+ b+ c+ d+ e+ f+ g+ h+ i+ j+ k+ l+ n+ m+
.rr 32 33 34 35 36 37 38 40 79 80 81 82
.rr a| b| c| d| e| f| g| h| i| j| k| l| m|
.rr a- b- c- d- e- f- g- h- i- j- k- l- m-
..
.so \*(sd/tmac.skeep
.de EQ  \"equation, breakout and display
.nr EF \\n(.u
.rm EE
.nr LE 1	\" 1 is center
.ds EL \\$1
.if "\\$1"L" \{\
.	ds EL \\$2
.	nr LE 0\}
.if "\\$1"C" .ds EL \\$2
.if "\\$1"R" \{\
.	ds EL \\$2 \" 2 is right adjust
.	nr LE 2\}
.if "\\$1"I" \{\
.	nr LE 0
.	if "\\$3"" .ds EE \\h'|10n'
.	el .ds EE \\h'\\$3'
.	ds EL \\$2\}
.if \\n(YE .nf
.di EZ
..
.de EN  \" end of a displayed equation
.br
.di
.rm EZ
.nr ZN \\n(dn
.if \\n(ZN .if !\\n(YE .LP
.if !\\n(ZN .if !"\\*(EL"" .nr ZN 1
.if \\n(ZN \{\
.	ie "\\n(.z"" \{\
.		if t .if !\\n(nl=\\n(PE .sp .5
.		if n .if !\\n(nl=\\n(PE .sp 1\}
.	el \{\
.		if t .if !\\n(.d=\\n(PE .sp .5
.		if n .if !\\n(.d=\\n(PE .sp 1\}\}
'pc
.if \\n(BD .nr LE 0 \" don't center if block display or mark/lineup
.if \\n(MK \{\
.	if \\n(LE=1 .ds EE \\h'|10n'
.	nr LE 0\}
'lt \\n(.lu
.if !\\n(EP .if \\n(ZN \{\
.	if \\n(LE=1 .tl \(ts\(ts\\*(10\(ts\\*(EL\(ts
.	if \\n(LE=2 .tl \(ts\(ts\(ts\\*(10\\*(EL\(ts
.	if !\\n(LE \{\
.		if !\\n(BD .tl \(ts\\*(EE\\*(10\(ts\(ts\\*(EL\(ts
.		if \\n(BD .if \\n(BD<\\w\(ts\\*(10\(ts .nr BD \\w\(ts\\*(10\(ts
.		if \\n(BD \!\\*(10\\t\\*(EL\}\}
.if \\n(EP .if \\n(ZN \{\
.	if \\n(LE=1 .tl \(ts\\*(EL\(ts\\*(10\(ts\(ts
.	if \\n(LE=2 .tl \(ts\\*(EL\(ts\(ts\\*(10\(ts
.	if !\\n(LE \{\
.		if !\\n(BD .tl \(ts\\*(EL\\*(EE\\*(10\(ts\(ts\(ts
.		if \\n(BD .if \\n(BD<\\w\(ts\\*(10\(ts .nr BD \\w\(ts\\*(10\(ts
.		if \\n(BD \!\\h'-\\\\n(.iu'\\*(EL\\h'|0'\\*(10\}\}
'lt \\n(LLu
'pc %
.if \\n(YE .if \\n(EF .fi
.if t .if \\n(ZN .sp .5
.if n .if \\n(ZN .sp
.ie "\\n(.z"" .nr PE \\n(nl
.el .nr PE \\n(.d
..
.de PS	\" start picture
.	\" $1 is height, $2 is width, both in inches
.if \\$1>0 .sp .35
.ie \\$1>0 .nr $1 \\$1
.el .nr $1 0
.in (\\n(.lu-\\$2)/2u
.ne \\$1
..
.de PE	\" end of picture
.in
.if \\n($1>0 .sp .65
..
.			\" .P1/.P2 macros for programs
.
.nr XP 1	\" delta point size for program
.nr XV 1p	\" delta vertical for programs
.nr XT 8	\" delta tab stop for programs
.nr DV .5v	\" space before start of program
.
.de P1
.nr P1 .4i	\" program indent in .P1
.if \\n(.$ .nr P1 \\$1
.br
.nr v \\n(.v
.di p1
.in \\n(P1u
.nf
.ps -\\n(XP
.vs -\\n(XVu
.ft CW
.nr t \\n(XT*\\w'x'u
.ta 1u*\\ntu 2u*\\ntu 3u*\\ntu 4u*\\ntu 5u*\\ntu 6u*\\ntu 7u*\\ntu 8u*\\ntu 9u*\\ntu 10u*\\ntu 11u*\\ntu 12u*\\ntu 13u*\\ntu 14u*\\ntu
..
.
.de P2
.br
.ps \\n(PS
.vs \\n(VSp
.vs \\nvu
.ft 1
.in -\\n(P1u
.di
.br
.sp \\n(DVu
.br
.if \\n(.$=0 .ne \\n(dnu  \" -\\n(DVu
.nf
.p1
.sp \\n(DVu
.br
.fi
..
.
.de ME
.nr SJ \\n(.j
.if \\n(LL .nr LT \\n(LL
.nr YE 1
.if !\\n(PO .nr PO \\n(.o
.if \\n(mo-0 .ds MO January
.if \\n(mo-1 .ds MO February
.if \\n(mo-2 .ds MO March
.if \\n(mo-3 .ds MO April
.if \\n(mo-4 .ds MO May
.if \\n(mo-5 .ds MO June
.if \\n(mo-6 .ds MO July
.if \\n(mo-7 .ds MO August
.if \\n(mo-8 .ds MO September
.if \\n(mo-9 .ds MO October
.if \\n(mo-10 .ds MO November
.if \\n(mo-11 .ds MO December
.if \\n(dw-0 .ds DW Sunday
.if \\n(dw-1 .ds DW Monday
.if \\n(dw-2 .ds DW Tuesday
.if \\n(dw-3 .ds DW Wednesday
.if \\n(dw-4 .ds DW Thursday
.if \\n(dw-5 .ds DW Friday
.if \\n(dw-6 .ds DW Saturday
.nr yP (\\n(yr+2000)/100)
.nr yD (\\n(yr%100
.af yD 01
.if "\\*(DY"" .ds DY \\*(MO \\n(dy, \\n(yP\\n(yD
.if "\\*(CF"" .if n .ds CF "\\*(DY
..
.	\"EM end up macro - process left over keep-release
.de EM
.br
.if \\n(AJ .tm Syntax error: no .AE
.if \\n(IF .ab Missing .FE somewhere
.if t .if \\n(TB=0 .wh -1p CM
.if \\n(TB \{\&\c
'	bp
.	NP
.	ch CM 160\}
..
.	\"NP new page
.de NP
.rr PE
.if \\n(FM+\\n(HM>=\\n(.p \{\
.	tm Margins bigger than page length.
.	ab
.	ex\}
.if t .CM
.if !\\n(HM .nr HM 1i
'sp \\n(HMu/2u
.ev 1
.nr PX \\n(.s
.nr PF \\n(.f
.nr PV \\n(.v
.lt \\n(LTu
.ps \\n(PS
.vs \\n(PS+2
.ft 1
.if \\n(PO .po \\n(POu
.PT
.ps \\n(PX
.vs \\n(PVu
.ft \\n(PF
.ev
'sp |\\n(HMu
.nr XX 0 1
.nr YY 0-\\n(FMu
.ch FO 16i
.ch FX 17i
.ch FO \\n(.pu-\\n(FMu
.ch FX \\n(.pu-\\n(FMu
.if \\n(MF .FV
.nr MF 0
.mk
.os
.ev 1
.if !\\n(TD .if \\n(TC<5  .XK
.nr TC 0
.ns
.ev
.nr TQ \\n(.i
.nr TK \\n(.u
.if \\n(IT \{\
.	in 0
.	nf
.	TT
.	in \\n(TQu
.	if \\n(TK .fi\
\}
.mk #T
....if t .if \\n(.o+\\n(LL>7.75i .tm Offset + line length exceeds 7.75 inches, too wide
..
.de XK
.nr TD 1
.nf
.ls 1
.in 0
.rn KJ KL
.KL
.rm KL
.if "\\n(.z"KJ" .di
.nr TB 0
.if "\\n(.z"KJ" .nr TB 1
.br
.in
.ls
.fi
.if (\\n(nl+1v)>(\\n(.p-\\n(FM) \{\
.	if \\n(NX>1 .RC
.	if \\n(NX<=1 .bp\}
.nr TD 0
..
.de KD
.nr KM 0
.if "\\n(.z"" .if \\$2>0 .if \\n(nl>\\n(HM \{\
.	if (\\n(nl+1v)<(\\n(.p-\\n(FM) .di KJ \" full page figure must have new page
.	sp 15i\}
.if "\\n(.z"" .if \\n(nl>\\n(HM .if \\$2=0 .if (\\n(nl+1v)>(\\n(.p-\\n(FM) .sp 15i
.if "\\n(.z"KJ" .nr KM 1 \" KM is 1 if in a rediversion of keeps
.if \\n(KM>0 \!.KD \\$1 \\$2
.nr KR \\n(.t
.if \\n(nl<=\\n(HM .nr KR 32767
.if \\n(KM=0 \{\
.	if \\n(KR<\\$1 \{\
.		di KJ
.		nr KM 1\}
.	if \\$2>0 .if (\\n(nl+1v)>(\\n(.p-\\n(FM) .sp 15i\}
.rs
.if \\n(KM=0 .if \\$2>0 .sp \\n(.tu-\\$1u
..
.de PT
.lt \\n(LLu
.pc %
.nr PN \\n%
.if \\n%-1 .tl \\*(LH\\*(CH\\*(RH
.lt \\n(.lu
..
.	\"FO - footer of page
.de FO
.rn FO FZ
.if \\n(IT>0 \{\
.	nr T. 1
.	if \\n(FC=0  .T# 1
.	br\}
.nr FC +1
.if \\n(NX<2 .nr WF 0
.nr dn 0
.if \\n(FC<=1 .if \\n(XX .XF
.rn FZ FO
.nr MF 0
.if \\n(dn  .nr MF 1
.if !\\n(WF \{\
.	nr YY 0-\\n(FMu
.	ch FO \\n(YYu\}
.if !\\n(dn .nr WF 0
.if \\n(FC<=1 .if \\n(XX=0 \{\
.	if \\n(NX>1 .RC
.	if \\n(NX<=1 'bp\}
.nr FC -1
.if \\n(ML>0 .ne \\n(MLu
..
.	\"2C - begin double column
.de 2C
.MC \" default MC is double column
..
.de MC \" multiple columns- arg is line length
.nr L1 \\n(LL*7/15
.if \\n(CW>=0 .nr L1 \\n(CWu
.if \\n(.$ .nr L1 \\$1n
.if \\n(GW>=0 .nr GW \\n(GWu
.if \\n(.$>1 .nr GW \\$2n
.nr NQ \\n(LL/\\n(L1
.if \\n(NQ<1 .nr NQ 1
.if \\n(NQ>2 .if (\\n(LL%\\n(L1)=0 .nr NQ -1
.if !\\n(1T \{\
.	BG
.	if n .sp 4
.	if t .sp 2\}
.if \\n(NX=0 .nr NX 1
.if !\\n(NX=\\n(NQ \{\
.	RT
.	if \\n(NX>1 .bp
.	mk
.	nr NC 1
.	po \\n(POu\}
.if \\n(NQ>1 .hy \\n(HY
.nr NX \\n(NQ
.if \\n(NX>1 .nr CW \\n(L1
.ll \\n(L1u
.nr FL \\n(L1u*11u/12u
.if \\n(NX>1 .if \\n(GW<0 .nr GW (\\n(LL-(\\n(NX*\\n(L1))/(\\n(NX-1)
.nr RO \\n(L1+\\n(GW
.ns
..
.de RC
.ie \\n(NC>=\\n(NX .C2
.el .C1
..
.de C1
.rt
.po +\\n(ROu
.nr NC +1
.if \\n(NC>\\n(NX .nr NC 1
.nr XX 0 1
.nr YY 0-\\n(FMu
.if \\n(MF .FV
.ch FX \\n(.pu-\\n(FMu
.ev 1
.if \\n(TB .XK
.nr TC 0
.ev
.nr TQ \\n(.i
.if \\n(IT \{\
.	in 0
.	TT
.	in \\n(TQu\}
.mk #T
.ns
..
.de C2
.po \\n(POu
.nr NC +1
.if \\n(NC>\\n(NX .nr NC 1
'bp
..
.	\"1C - return to single column format
.de 1C
.MC \\n(LLu
.hy \\n(HY
..
.de MH
Bell Laboratories
Murray Hill, New Jersey 07974
..
.de PY
Bell Laboratories
Piscataway, New Jersey 08854
..
.de BT
.nr PX \\n(.s
.nr PF \\n(.f
.ft 1
.ps \\n(PS
'lt \\n(LTu
.po \\n(POu
.if \\n%>0 .tl \(ts\\*(LF\(ts\\*(CF\(ts\\*(RF\(ts
.ft \\n(PF
.ps \\n(PX
..
.	\"PP - paragraph
.de PP
.RT
.if \\n(1T .sp \\n(PDu
.ti +\\n(PIu
..
.	\"SH - (unnumbered) section heading
.de SH
.ti \\n(.iu
.RT
.ie \\n(1T .sp 1
.el .BG
.RT
.ne 4
.ft 3
.if n .ul 1000
..
.	\"NH - numbered heading
.de N{
.RT
.ie \\n(1T .sp 1
.el .BG
.RT
.ne 4
.ft 3
.if n .ul 1000
.nr NS \\$1
.if !\\n(.$ .nr NS 1
.if !\\n(NS .nr NS 1
.nr H\\n(NS +1
.if !\\n(NS-4 .nr H5 0
.if !\\n(NS-3 .nr H4 0
.if !\\n(NS-2 .nr H3 0
.if !\\n(NS-1 .nr H2 0
.if !\\$1 .if \\n(.$ .nr H1 1
.ds SN \\n(H1.
.ti \\n(.iu
.if \\n(NS-1 .as SN \\n(H2.
.if \\n(NS-2 .as SN \\n(H3.
.if \\n(NS-3 .as SN \\n(H4.
.if \\n(NS-4 .as SN \\n(H5.
..
.de NH
.N{ \\$1
\\*(SN
..
.	\"BG - begin, execute at first PP
.de BG
.br
.ME
.rm ME
.di
.ce 0
.nr KI 0
.hy \\n(HY
.nr 1T 1
.nr CS 0
.S\\n(ST
.rm S0 S1 S2 S3 OD OK TX AX WT CS TM IM MF MR RP I1 I2 I3 I4 I5 CB E1 E2
.de TL
.ft 3
.sp
.if n .ul 100
.ce 100
.ps +2
\\..
.de AU
.ft 2
.if n .ul 0
.ce 100
.sp
.NL
\\..
.de AI
.ft 1
.ce 100
.if n .ul 0
.if n .sp
.if t .sp .5
.NL
\\..
.RA
.rm RA
.rn FJ FS
.rn FK FE
.nf
.ev 1
.ps \\n(PS-2
.vs \\n(.s+2p
.ev
.if !\\n(KG .nr FP 0
.if \\n(GA>1 .if \\n(KG=0 .nr GA 0 \" next UNIX must be flagged.
.nr KG 0
.if \\n(FP \{\
.	FS
.	FG
.	FE\}
.br
.if n .if \\n(TV .sp 2
.if t .if \\n(TV .sp 1
.fi
.ll \\n(LLu
.ev 1
.if !\\n(FL .nr FL \\n(LLu*11u/12u
.ll \\n(FLu
.ev
..
.de RA \"redefine abstract macros
.de AB
.br
.if !\\n(1T .BG
.ce 1
.sp 1
.ie \\n(.$ \{\
.	if !"\\$1"-" .if !"\\$1"no" \{\
\\$1
.sp\}\}
.el \{\
ABSTRACT
.sp\}
.sp 1
.nr AJ 1
.in +\\n(.lu/12u
.ll -\\n(.lu/12u
.RT
\\..
.de AE
.nr AJ 0
.br
.in 0
.ll \\n(LLu
.ie \\n(VS>=41 .vs \\n(VSu
.el .vs \\n(VSp
\\..
..
.	\"IP - indented paragraph
.de IP
.RT
.if !\\n(IP .nr IP +1
.ie \\n(ID>0 .sp \\n(IDu
.el .sp \\n(PDu
.nr IU \\n(IR+1
.if \\n(.$>1 .nr I\\n(IU \\$2n+\\n(I\\n(IRu
.if \\n(I\\n(IU=0 .nr I\\n(IU \\n(PIu+\\n(I\\n(IRu
.in \\n(I\\n(IUu
.nr TY \\n(TZ-\\n(.i
.nr JQ \\n(I\\n(IU-\\n(I\\n(IR
.ta \\n(JQu \\n(TYuR
.if \\n(.$ \{\
.ti \\n(I\\n(IRu
\&\\$1\t\c
.\}
..
.	\"LP - left aligned (block) paragraph
.de LP
.ti \\n(.iu
.RT
.if \\n(1T .sp \\n(PDu
..
.de QP
.ti \\n(.iu
.RT
.if \\n(1T .sp \\n(PDu
.ne 1.1
.nr QP 1
.in +\\n(QIu
.ll -\\n(QIu
.ti \\n(.iu
..
.	\"IE - synonym for .LP
.de IE
.LP
..
.	\"RS - prepare for double indenting
.de RS
.nr IS \\n(IP
.RT
.nr IP \\n(IS
.nr IU \\n(IR
.nr IR +1
.if !\\n(I\\n(IR .nr I\\n(IR \\n(I\\n(IU+\\n(PIu
.in \\n(I\\n(IRu
.nr TY \\n(TZ-\\n(.i
.ta \\n(TYuR
..
.	\"RE - retreat to the left
.de RE
.nr IS \\n(IP
.RT
.nr IP \\n(IS
.if \\n(IR>0 .nr IR -1
.in \\n(I\\n(IRu
..
.de TC
.nr TZ \\n(.lu
.if \\n(.$ .nr TZ \\$1n
.ta \\n(TZuR
..
.de TD
.LP
.nr TZ 0
..
.	\"CM - cut mark
.de CM
.po 0
.lt 7.6i
.ft 1
.ps 10
.vs 4p
.if "\\*(.T"aps" .tl '--''--'
.po
.vs
.lt
.ps
.ft
..
.		\" fontname(CW) fontstr(\f(CW) first_arg goes_after goes_before
.de OF		\" this is completely WRONG if any argument contains "'s
.nr PQ \\n(.f
.hy 0
.if t .if "\\$3"" .ft \\$1
.if t .if !"\\$3"" \{\
\&\\$5\\$2\\$3\\f\\n(PQ\\$4
.hy \\n(HY\}
.if n \{\
.	if \\n(.$=5 \&\\$5
.	ie "\\$3"" .ul 1000
.	el .ul 1
.	if \\n(.$=3 \&\\$3
.	if \\n(.$>3 \&\\$3\\c
.	if \\n(.$>3 \&\\$4
.	hy \\n(HY\}
..
.	\"B - bold font
.de B
.OF 3 \\f3 "\\$1" "\\$2" "\\$3"
..
.de BI	\" bold italic -- only on 202
.OF 4 \\f4 "\\$1" "\\$2" "\\$3"
..
.	\"R - Roman font
.de R
.nr PQ \\n(.f
.ft 1
.ie \\n(.$>0 \&\\$1\f\\n(PQ\\$2
.el .if n .ul 0
..
.	\"I - italic font
.de I
.OF 2 \\f2 "\\$1" "\\$2" "\\$3"
..
.	\"CW - constant width font
.de CW
.nr PQ \\n(.f
.if t .if \\n(.$>0 \%\&\\$3\f(CW\\$1\f\\n(PQ\&\\$2
.if t .if \\n(.$=0 .OF CW \\f(CW "\\$1" "\\$2" "\\$3"
.if n .OF CW \\f(CW "\\$1" "\\$2" "\\$3"
..
.	\"TA - tabs set in ens or chars
.de TA
.ta \\$1n \\$2n \\$3n \\$4n \\$5n \\$6n \\$7n \\$8n \\$9n
..
.	\"SM - make smaller size
.de SM
.ie \\n(.$ \&\\$3\s-2\\$1\s0\\$2
.el .ps -2
..
.	\"LG - make larger size
.de LG
.ps +2
..
.	\"NL - return to normal size
.de NL
.ps \\n(PS
..
.	\"DA - force date; ND - no date or new date.
.de DA
.if \\n(.$ .ds DY \\$1 \\$2 \\$3 \\$4
.ds CF \\*(DY
..
.de ND
.ME
.rm ME
.ds DY \\$1 \\$2 \\$3 \\$4
.rm CF
..
.de FN
.FS
..
.	\"FS - begin footnote
.de FJ
'ce 0
.nr IA \\n(IP
.nr IB \\n(.i
.ev1
.ll \\n(FLu
.da FF
.br
.if \\n(IF \{\
.	tm Footnote within footnote-illegal.
.	ab\}
.nr IF 1
.if !\\n+(XX-1 .FA
..
.	\"FE - footnote end
.de FK
.br
.in 0
.nr IF 0
.di
.ev
.if !\\n(XX-1 .nr dn +\\n(.v
.nr YY -\\n(dn
.if !\\n(NX .nr WF 1
.if \\n(dl>\\n(CW .nr WF 1
.ie (\\n(nl+\\n(.v)<=(\\n(.p+\\n(YY) .ch FO \\n(YYu
.el \{\
.	if \\n(nl>(\\n(HM+1.5v) .ch FO \\n(nlu+\\n(.vu
.	if \\n(nl+\\n(FM+1v>\\n(.p .ch FX \\n(.pu-\\n(FMu+2v
.	if \\n(nl<=(\\n(HM+1.5v) .ch FO \\n(HMu+(4u*\\n(.vu)\}
.nr IP \\n(IA
'in \\n(IBu
..
.\"	First page footer.
.de FS
.ev1
.br
.ll \\n(FLu
.da FG
..
.de FE
.br
.di
.nr FP \\n(dn
.if !\\n(1T .nr KG 1 \"not in abstract repeat next page.
.if "\\n(.z"OD" .nr KG 0 \" if in OK, don't repeat.
.ev
..
.de FA
.if n __________________________
.if t \l'1i'
.br
..
.de FV
.FS
.nf
.ls 1
.FY
.ls
.fi
.FE
..
.de FX
.if \\n(XX \{\
.	di FY
.	ns\}
..
.de XF
.if \\n(nlu+1v>(\\n(.pu-\\n(FMu) .ch FX \\n(nlu+1.9v
.ev1
.nf
.ls 1
.FF
.rm FF
.nr XX 0 1
.br
.ls
.di
.fi
.ev
..
.de FL
.ev1
.nr FL \\$1n
.ll \\$1
.ev
..
.de HO
Bell Laboratories
Holmdel, New Jersey 07733
..
.de WH
Bell Laboratories
Whippany, New Jersey 07981
..
.de IH
Bell Laboratories
Naperville, Illinois 60540
..
.de UL \" underline argument, don't italicize
.ie t \\$1\l'|0\(ul'\\$2
.el .I "\\$1" "\\$2"
..
.de UX
.ie \\n(GA \\$2\s-1UNIX\s0\\$1
.el \{\
.ie n \{\\$2UNIX\\$1*
.FS
* UNIX is a
.ie \\$3=1 Footnote
.el registered trademark
of X/Open.
.FE\}
.el \\$2\s-1UNIX\\s0\\$1\\f1\(rg\\fP
.nr GA 1\}
..
.de US
the
.UX
operating system\\$1
..
.de QS
.br
.LP
.in +\\n(QIu
.ll -\\n(QIu
..
.de QE
.br
.ll +\\n(QIu
.in -\\n(QIu
.LP
..
.de B1 \" begin boxed stuff
.br
.di BB
.nr BC 0
.if "\\$1"C" .nr BC 1
.nr BE 1
..
.de B2 \" end boxed stuff
.br
.nr BI 1n
.if \\n(.$>0 .nr BI \\$1n
.di
.nr BE 0
.nr BW \\n(dl
.nr BH \\n(dn
.ne \\n(BHu+\\n(.Vu
.nr BQ \\n(.j
.nf
.ti 0
.if \\n(BC>0 .in +(\\n(.lu-\\n(BWu)/2u
.in +\\n(BIu
.ls 1
.BB
.ls
.in -\\n(BIu
.nr BW +2*\\n(BI
.sp -1
\l'\\n(BWu\(ul'\L'-\\n(BHu'\l'|0\(ul'\h'|0'\L'\\n(BHu'
.nr BW -2*\\n(BI
.if \\n(BC>0 .in -(\\n(.lu-\\n(BWu)/2u
.if \\n(BQ .fi
.br
..
.de AT
.nf
.sp
.ne 2
Attached:
..
.de CT
.nf
.sp
.ne 2
.ie \\n(.$ Copy to \\$1:
.el Copy to:
..
.de BX
.ie t \(br\|\\$1\|\(br\l'|0\(rn'\l'|0\(ul'
.el \(br\\kA\|\\$1\|\\kB\(br\v'-1v'\h'|\\nBu'\l'|\\nAu'\v'1v'\l'|\\nAu'
..
.IZ
.rm IZ
.de [
[
..
.de ]
]
..
