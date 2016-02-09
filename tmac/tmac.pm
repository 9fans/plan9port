.\" 10/22/92 activate next line before installing
.pi /home/pcanning/share/plan9port/bin/auxpm
.
.		\" IZ - initialization
.de IZ
.fp 10 S			\" force a font out into prefix
.nr PS 10		\" point size
.nr VS 12		\" line spacing
.ps \\n(PS
.ie \\n(VS>=41 .vs \\n(VSu
.el .vs \\n(VSp
.nr LL 6i		\" line length
.ll \\n(LLu
.nr LT \\n(.l		\" title length
.lt \\n(LTu
.if !\\n(HM .nr HM 1i   \" top of page
.if !\\n(FM .nr FM 1i	\" footer margin
.if !\\n(FO .nr FO \\n(.p-\\n(FM	\" bottom of page
.			\" to set text ht to N, set FO to N + \n(HM.  default is 10i
.pl 32767u		\" safety first: big pages for pm
.if !\\n(PO .nr PO \\n(.ou	\" page offset
.nr PI 5n		\" .PP paragraph indent
.nr QI 5n		\" .QS indent
.nr DI 5n		\" .DS indent
.nr PD 0.3v		\" paragraph vertical separation
.nr TS 0.5v		\" space around tables
.nr Kf 0.5v		\" space around .KF/.KE
.nr Ks 0.5v		\" space around .KS/.KE
.
.nr P1 .4i		\" indent for .P1/.P2
.nr dP 1		\" delta point size for programs in .P1/.P2
.nr dV 2p		\" delta vertical for programs
.nr dT 8		\" delta tab stop for programs
.nr DV .5v		\" space before start of program
.nr IP 0		\" ?
.nr IR 0		\" ?
.nr I1 \\n(PIu
.ev 1
.if !\\n(FL .nr FL \\n(LLu	\" footnote length
.ll \\n(FLu
.ps 8			\" text size & leading in footnote
.vs 10p
.ev
.if \\*(CH .ds CH "\(hy \\\\n(PN \(hy
.ds # #\\\\n(.c \\\\n(.F
.
.
.ME	\" initialize date strings
.rm ME
.	\"  accents:  \*'e \*`e \*:u \*^e \*~n \*va \*,c
.ds ' \h'\w'e'u*4/10'\z\(aa\h'-\w'e'u*4/10'
.ds ` \h'\w'e'u*4/10'\z\(ga\h'-\w'e'u*4/10'
.ds : \\v'-0.6m'\\h'(1u-(\\\\n(.fu%2u))*0.13m+0.00m'\\z.\\h'0.2m'\\z.\\h'-((1u-(\\\\n(.fu%2u))*0.13m+0.20m)'\\v'0.6m'
.ds ^ \\\\k:\\h'-\\\\n(.fu+1u/2u*2u+\\\\n(.fu-1u*0.13m+0.06m'\\z^\\h'|\\\\n:u'
.ds ~ \\\\k:\\h'-\\\\n(.fu+1u/2u*2u+\\\\n(.fu-1u*0.13m+0.06m'\\z~\\h'|\\\\n:u'
.ds v \\\\k:\\\\h'+\\\\w'e'u/4u'\\\\v'-0.6m'\\\\s6v\\\\s0\\\\v'0.6m'\\\\h'|\\\\n:u'
.ds , \\\\k:\\\\h'\\\\w'c'u*0.4u'\\\\z,\\\\h'|\\\\n:u'
..
.
.
.		\" SP - generate paddable space
.de SP
.br
.nr X 1v
.if \\n(.$ .nr X \\$1v
.ie '\\$2'exactly' \{\
\v'\\nXu'\ \h'-\w'\ 'u'\c
.sp \\$1\}
.el .X "SP \\nX \\$2"
..
.		\" NE - need space on this page
.de NE
.nr X 1v
.if \\n(.$ .nr X \\$1v
.X "NE \\nX \\$2"
..
.		\" BP, FL, FC - begin page, flush figures, flush column
.de BP
.br
.X CMD BP
..
.de FL
.br
.X CMD FL
..
.de FC
.br
.X CMD FC
..
.		\" X - generate an x X ... command in the output
.de X
....ie '\\n(.z'' \\!x X \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
....el \\!.X "\\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
...
.if !'\\n(.z'' .if \\n(.$=1 \\!.X "\\$1
.if !'\\n(.z'' .if \\n(.$=2 \\!.X "\\$1 \\$2
.if !'\\n(.z'' .if \\n(.$=3 \\!.X "\\$1 \\$2 \\$3
.if !'\\n(.z'' .if \\n(.$>3 \\!.X "\\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
.if '\\n(.z'' .if \\n(.$=1 \\!x X \\$1 \\*#
.if '\\n(.z'' .if \\n(.$=2 \\!x X \\$1 \\$2 \\*#
.if '\\n(.z'' .if \\n(.$=3 \\!x X \\$1 \\$2 \\$3 \\*#
.if '\\n(.z'' .if \\n(.$=4 \\!x X \\$1 \\$2 \\$3 \\$4 \\*#
.if '\\n(.z'' .if \\n(.$>4 \\!x X \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9 \\*#
..
.		\" DA - force date
.de DA
.if \\n(.$ .ds DY \\$1 \\$2 \\$3 \\$4
.ds CF \\*(DY
..
.		\" ND - set new or no date
.de ND
.ds DY \\$1 \\$2 \\$3 \\$4
.rm CF
..
.de ME		\" ME - set month strings
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
.if "\\*(DY"" .ds DY \\*(MO \\n(dy, 19\\n(yr
..
.		\" FP - font position for a family
.de FP
.if '\\$1'palatino'\{\
.	fp 1 PA
.	fp 2 PI
.	fp 3 PB
.	fp 4 PX\}
.if '\\$1'lucidasans'\{\
.	fp 1 R LucidaSans
.	fp 2 I LucidaSansI
.	fp 3 B LucidaSansB
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
.		\" TL - title
.de TL
.br
.if !\\n(1T .BG
....hy 0
.ft 3
.ps \\n(PS+2p
.vs \\n(VS+2p
.ll \\n(LLu
.ce 100		\" turned off in .RT
.sp .5i
..
.		\" AU - remember author(s)
.de AU
.ft 1
.ps \\n(PS
.ie \\n(VS>=41 .vs \\n(VSu
.el .vs \\n(VSp
.SP .5
..
.		\" AI - author's institution
.de AI
.SP .25
.ft 2
..
.		\" AB - begin abstract
.de AB
.nr AB 1	  \" we're in abstract
.if !\\n(1T .BG
.ft 1
.ps \\n(PS
.vs \\n(VSp
.ce
.in +\\n(.lu/12u
.ll -\\n(.lu/12u
.SP 1
.ie \\n(.$ \\$1
.el ABSTRACT
.SP .75 
.RT
..
.		\" AE - end of abstract
.de AE
.br
.nr AB 0
.in 0
.ll \\n(LLu
.ps \\n(PS
.ie \\n(VS>=41 .vs \\n(VSu
.el .vs \\n(VSp
.SP
..
.		\" 2C - 2 columns
.de 2C
.MC 2
..
.		\" 1C - 1 column
.de 1C
.MC 1
..
.		\" MC - multiple columns
.de MC
.br
.if \\n(1T .RT
.if \\n(1T .NP
.if !\\n(OL .nr OL \\n(LL
.if \\n(CW=0 .nr CW \\n(LL*7/15
.if \\n(GW=0 .nr GW \\n(LL-(2*\\n(CW)
.nr x \\n(CW+\\n(GW
.if "\\$1"" .MC 2
.if \\$1=1 \{\
.	X MC 1 0
.	nr LL \\n(OLu\}
.if \\$1=2 \{\
.	X MC 2 \\nx
.	nr LL \\n(CWu\}
.ll \\n(LLu
.if \\$1>2 .tm -mpm can't handle more than two columns
.if \\n(1T .RT
..
.		\" TS - table start, TE - table end;  also TC, TQ, TH
.de TS
.br
.if !\\n(1T .RT
.SP \\n(TSu TS
.X "US TS
.if \\$1H .TQ
.nr IX 1
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
.		\" TE - table end
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
.rm a+ b+ c+ d+ e+ f+ g+ h+ i+ j+ k+ l+ n+ m+
.rr 32 33 34 35 36 37 38 40 79 80 81 82
.rr a| b| c| d| e| f| g| h| i| j| k| l| m|
.rr a- b- c- d- e- f- g- h- i- j- k- l- m-
.X "END US TE
.SP \\n(TSu TE
.bp
..
.		\" EQ - equation, breakout and display
.de EQ
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
.		\" EN - end of equation
.de EN 
.br
.di
.rm EZ
.nr ZN \\n(dn
.if \\n(ZN .if !\\n(YE .LP
.if !\\n(ZN .if !"\\*(EL"" .nr ZN 1
.if \\n(ZN \{\
.	SP .5v EQ
.	X "US EQ"\}
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
.if \\n(ZN .X "END US EQ"
.if \\n(ZN .SP .5v EN
.if \\n(ZN .bp
..
.		\" PS - start picture
.de PS			\" $1 is height, $2 is width, in inches
.br
.nr X 0.35v
.if \\$1>0 .X "SP \\nX PS"
.ie \\$1>0 .nr $1 \\$1
.el .nr $1 0
.X "US PS \\$1
.in (\\n(.lu-\\$2)/2u
..
.		\" PE - end of picture
.de PE
.in
.X "END US PE
.nr X .65v
.if \\n($1>0 .X "SP \\nX PE"
.bp
..
.de IS	\" for -mpm only
.KS
..
.de IE
.KE
.bp
..
.		\" NP - new page
.de NP
.ev 2
.bp
.if \\n(KF=0 \{\
.	nr PX \\n(.s
.	nr PF \\n(.f
.	nr PV \\n(.v
.	lt \\n(LTu
.	ps \\n(PS
.	vs \\n(PS+2
.	ft 1
.	if \\n(PO .po \\n(POu	\" why isn't this reset???
.	PT \\$1
.	bp
.	rs
.	BT
.	bp
.	nr %# +1
.	ps \\n(PX
.	vs \\n(PVu
.	ft \\n(PF \}
.ev
..
.
.ds %e .tl '\\*(LH'\\*(CH'\\*(RH'
.ds %o .tl '\\*(LH'\\*(CH'\\*(RH'
.ds %E .tl '\\*(LF'\\*(CF'\\*(RF'
.ds %O .tl '\\*(LF'\\*(CF'\\*(RF'
.
.		\" PT - page title
.de PT
.nr PN \\n(%#
.X "PT \\n(%#
.sp \\n(HMu/2u
.if \\n(OL .lt \\n(OLu		\" why isn't this reset???
.if \\n(BT>0 .if \\n(%#%2 \\*(%o
.if \\n(BT>0 .if !\\n(%#%2 \\*(%e
.if \\n(BT=0 .tl '\0'''		\" put out something or spacing is curdled
.X "END PT \\n(%#
..
.		\" BT - bottom title
.de BT
.X "BT \\n(%#
.sp |\\n(FMu/2u+\\n(FOu-1v
.if \\n(%#%2 \\*(%O
.if !\\n(%#%2 \\*(%E
.nr BT \\n(BT+1
.X "END BT \\n(%#
..
.		\" KS - non-floating keep
.de KS
.br
.if "\\n(.z"" .NP  \" defends poorly against including ht of page stuff in diversion for .B1
.X "US KS 0
.nr KS +1
.SP \\n(Ksu
..
.		\" KF - floating keep
.de KF
.ev 1
.br
.if \\n(KS>0 .tm KF won't work inside KS, line \\n(.c, file \\n(.F
.if \\n(KF>0 .tm KF won't work inside KF, line \\n(.c, file \\n(.F
.nr KF 1
.nr 10 0
.	if !'\\$1'' .nr 10 \\$1u
.	if '\\$1'bottom' .nr 10 \\n(FOu-1u
.	if '\\$1'top' .nr 10 \\n(HM
.	if \\n(10 .X "UF \\n(10 KF"
.	if !\\n(10 .X "UF \\n(HM KF"
.	nr X \\n(FOu-2u
.	if \\n(10 .X "UF \\n(10 KF"
.	if !\\n(10 .X "UF \\nX KF"
.nr SJ \\n(.u
.ps \\n(PS
.if \\n(VS>40 .vs \\n(VSu
.if \\n(VS<=39 .vs \\n(VSp
.ll \\n(LLu
.lt \\n(LTu
.SP \\n(Kfu
..
.		\" KE - end of KS/KF
.de KE
.bp
.ie \\n(KS>0 \{\
.	SP \\n(Ksu
.	X "END US KS
.	nr KS -1 \}
.el .ie \\n(KF>0 \{\
.	SP \\n(Kfu
.	nr KF 0
.	X "END UF KF"
.	if \\n(SJ .fi
.	ev \}
.el .tm .KE without preceding .KS or .KF, line \\n(.c, file \\n(.F
..
.
.		\" DS - display. .DS C center; L left-adjust; I indent (default)
.de DS		\"  $2 = amount of indent
.KS
.nf
.\\$1D \\$2 \\$1
.ft 1
.if !\\n(IF \{\
.	ps \\n(PS
.	if \\n(VS>40 .vs \\n(VSu
.	if \\n(VS<=39 .vs \\n(VSp\}
..
.de D
.ID \\$1
..
.de CD
.XD
.ce 1000
..
.de ID
.XD
.if \\n(.$=0 .in +\\n(DIu
.if \\n(.$=1 .if "\\$1"I" .in +\\n(DIu
.if \\n(.$=1 .if !"\\$1"I" .in +\\$1n
.if \\n(.$>1 .in +\\$2n
.....in +0.5i
.....if \\n(.$ .if !"\\$1"I" .if !"\\$1"" .in \\n(DIu
.....if \\n(.$ .if !"\\$1"I" .if !"\\$1"" .in +\\$1n
..
.de LD
.XD
..
.de XD
.nf
.nr OI \\n(.i
.SP \\n(DVu
..
.		\" BD - block display: save everything, then center it.
.de BD
.XD
.nr BD 1
.nf
.in \\n(OIu
.di DD
..
.		\" DE - display end
.de DE
.ce 0
.if \\n(BD>0 .XF
.nr BD 0
.in \\n(OIu
.SP \\n(DVu
.KE
.fi
..
.		\" XF - finish a block display to be recentered.
.de XF
.di
.if \\n(dl>\\n(BD .nr BD \\n(dl
.if \\n(BD<\\n(.l .in (\\n(.lu-\\n(BDu)/2u
.nr EI \\n(.l-\\n(.i
.ta \\n(EIuR
.nf
.DD
.in \\n(OIu
..
.
.
.		\" SH - (unnumbered) section heading
.de SH
.RT
.nr X 1v
.nr Y 3v
.if \\n(1T .NP
.if \\n(1T .X "NE \\nY SH"	\" should these be reversed, change Y to 4v
.if \\n(1T .X "SP \\nX SH
.ft 3
..
.		\" NH - numbered heading
.de NH
.RT
.nr X 1v
.nr Y 3v
.if \\n(1T .NP
.if \\n(1T .X "NE \\nY NH"	\" should these be reversed, change Y to 4v
.if \\n(1T .X "SP \\nX NH
.ft 3
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
.if \\n(NS-1 .as SN \\n(H2.
.if \\n(NS-2 .as SN \\n(H3.
.if \\n(NS-3 .as SN \\n(H4.
.if \\n(NS-4 .as SN \\n(H5.
\\*(SN
..
.		\" RT - reset at beginning of each PP, LP, etc.
.de RT
.if !\\n(AB .if !\\n(1T .BG
.ce 0
.if !\\n(AB .if !\\n(KF .if !\\n(IF .if !\\n(IX .if !\\n(BE .di
.if \\n(QP \{\
.	ll +\\n(QIu
.	in -\\n(QIu
.	nr QP -1\}
.if !\\n(AB \{\
.	ll \\n(LLu\}
.if !\\n(IF .if !\\n(AB \{\
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
.if !\\n(AB .ft 1
.ta 5n 10n 15n 20n 25n 30n 35n 40n 45n 50n 55n 60n 65n 70n 75n 80n
.fi
..
.		\" BG - begin, execute at first TL, AB, NH, SH, PP, etc.
.de BG		\"	IZ has been called, so registers have some value
.br
.if \\n(CW>0 .if \\n(LL=0 .nr LL \\n(CW+\\n(CW+\\n(GW
.ll \\n(LLu
.lt \\n(LLu
.po \\n(POu
.nr YE 1		\" ok to cause break in .EQ (earlier ones won't)
.ev 0
.hy 14
.ev
.ev 1
.hy 14
.ev
.ev 2
.hy 14
.ev
.nr 1T 1
.X "PARM NP \\n(HM
.X "PARM FO \\n(FO
.if !\\n(%# .nr %# 1
..
.		\" PP - paragraph
.de PP
.RT
.if \\n(1T .NP
.if \\n(1T .X "SP \\n(PD PP"
.if \\n(1T .X "BS 2 PP"
.ti +\\n(PIu
..
.		\" LP - left aligned paragraph
.de LP
.RT
.if \\n(1T .NP
.if \\n(1T .X "SP \\n(PD LP"
.if \\n(1T .X "BS 2 LP"
..
.		\" IP - indented paragraph
.de IP
.RT
.if !\\n(IP .nr IP +1
.if \\n(1T .NP
.if \\n(1T .X "SP \\n(PD PP"
.if \\n(1T .X "BS 2 IP"
.nr IU \\n(IR+1
.if \\n(.$>1 .nr I\\n(IU \\$2n+\\n(I\\n(IRu
.if \\n(I\\n(IU=0 .nr I\\n(IU \\n(PIu+\\n(I\\n(IRu
.in \\n(I\\n(IUu
.nr TY \\n(TZ-\\n(.i
.nr JQ \\n(I\\n(IU-\\n(I\\n(IR
.ta \\n(JQu \\n(TYuR
.if \\n(.$ \{\
.ti \\n(I\\n(IRu
\&\\$1\t\c\}
..
.		\" QP - quoted paragraph (within IP)
.de QP
.RT
.if \\n(1T .NP
.if \\n(1T .X "SP \\n(PD QP"
.if \\n(1T .X "BS 2 QP"
.nr QP 1
.in +\\n(QIu
.ll -\\n(QIu
.ti \\n(.iu
..
.		\" RS - prepare for double indenting
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
.		\" RE - retreat to the left
.de RE
.nr IS \\n(IP
.RT
.nr IP \\n(IS
.if \\n(IR>0 .nr IR -1
.in \\n(I\\n(IRu
..
.		\" B - bold font
.de B
.nr PQ \\n(.f
.ft 3
.if \\n(.$ \&\\$1\\f\\n(PQ\\$2
..
.		\" BI - bold italic
.de BI
.nr PQ \\n(.f
.ft 4
.if \\n(.$ \&\\$1\\f\\n(PQ\\$2
..
.		\" R - Roman font
.de R
.nr PQ \\n(.f
.ft 1
.if \\n(.$ \&\\$1\f\\n(PQ\\$2
..
.		\" I - italic font
.de I
.nr PQ \\n(.f
.ft 2
.if \\n(.$ \&\\$1\^\f\\n(PQ\\$2
..
.		\" CW - constant width font from -ms
.de CW
.nr PQ \\n(.f
.if \\n(.$=0 .ft CW
.if \\n(.$>0 \%\&\\$3\f(CW\\$1\\f\\n(PQ\\$2
..
.de IT		\" ditto to italicize argument
.nr Sf \\n(.f
\%\&\\$3\f2\\$1\f\\n(Sf\&\\$2
..
.		\" TA - tabs set in ens or chars
.de TA
.ta \\$1n \\$2n \\$3n \\$4n \\$5n \\$6n \\$7n \\$8n \\$9n
..
.		\" SM - make smaller size
.de SM
.ie \\n(.$ \&\\$3\s-2\\$1\s0\\$2
.el .ps -2
..
.		\" LG - make larger size
.de LG
.ie \\n(.$ \&\\$3\s+2\\$1\s0\\$2
.el .ps +2
..
.		\" NL - return to normal size
.de NL
.ps \\n(PS
..
.		\" FS - begin footnote
.de FS
.if \\n(IF>0 .tm .FS within .FS/.FE, line \\n(.c, file \\n(.F
.if \\n(KF>0 .tm .FS won't work inside .KF, line \\n(.c, file \\n(.F
.if \\n(KS>0 .tm .FS won't work inside .KS, line \\n(.c, file \\n(.F
.nr IF 1
.ev 1
.ps \\n(PS-2
.ie \\n(VS>=41 .vs \\n(VSu-2p
.el .vs \\n(VSp-2p
.ll \\n(LLu
.br
.nr X \\n(FOu
.X "BF \\nX FS
.SP .3v
....FA	\" deleted by authority of cvw, 10/17/88
..
.		\" FE - end footnote
.de FE
.if !\\n(IF .tm .FE without .FS, line \\n(.c, file \\n(.F
.br
.X "END BF FE
.bp
.ev
.nr IF 0
..
.		\" FA - the line for a footnote
.de FA
\l'1i'
.br
..
.		\" Tm - message to be passed on
.de Tm
.ev 2
.if \\n(.$=1 .X "TM \\$1
.if \\n(.$=2 .X "TM \\$1 \\$2
.if \\n(.$=3 .X "TM \\$1 \\$2 \\$3
.if \\n(.$=4 .X "TM \\$1 \\$2 \\$3 \\$4
.if \\n(.$=5 .X "TM \\$1 \\$2 \\$3 \\$4 \\$5
.if \\n(.$=6 .X "TM \\$1 \\$2 \\$3 \\$4 \\$5 \\$6
.if \\n(.$=7 .X "TM \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7
.if \\n(.$=8 .X "TM \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8
.if \\n(.$=9 .X "TM \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
.br
.ev
..
.de MH
AT&T Bell Laboratories
Murray Hill, New Jersey 07974
..
.de HO
AT&T Bell Laboratories
Holmdel, New Jersey 07733
..
.de WH
AT&T Bell Laboratories
Whippany, New Jersey 07981
..
.de IH
AT&T Bell Laboratories
Naperville, Illinois 60540
..
.		\" UL - underline argument, don't italicize
.de UL
\\$1\l'|0\(ul'\\$2
..
.		\" UX - print $2 UNIX $1
.de UX
.ie \\n(UX \\$2\s-1UNIX\s0\\$1
.el \{\
\\$2\s-1UNIX\\s0\\$1\(rg
.nr UX 1\}
..
.		\" QS - start quote
.de QS
.br
.LP
.in +\\n(QIu
.ll -\\n(QIu
..
.		\" QE - end quote
.de QE
.br
.ll +\\n(QIu
.in -\\n(QIu
.LP
..
.		\"  B1 - begin boxed stuff
.de B1
.br
.di BB
.nr BC 0
.if "\\$1"C" .nr BC 1
.nr BE 1
..
.		\" B2 - end boxed stuff
.de B2 
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
.if \\n(BC>0 .in -(\\n(.lu-\\n(BWu)/2u
.if \\n(BQ .fi
.br
..
.		\" BX - boxed stuff
.de BX
\(br\|\\$1\|\(br\l'|0\(rn'\l'|0\(ul'
..
.
.	\" macros for programs, etc.
.
.ig
	programs are displayed between .P1/.P2 pairs
	default is to indent by 1/2 inch, nofill, dP smaller
	.P1 x causes an indent of x instead.

	.P3 can be used to specify optional page-break points
	inside .P1/.P2
..
.
.		\" P1 - start of program
.de P1
.nr $1 \\n(P1
.if \\n(.$ .nr $1 \\$1n
.br
.X "SP \\n(DV P1"
.X "US P1"
.in \\n($1u
.nf
.nr v \\n(.v
.ps -\\n(dP
.vs -\\n(dVu
.ft CW
.nr t \\n(dT*\\w'x'u
.ta 1u*\\ntu 2u*\\ntu 3u*\\ntu 4u*\\ntu 5u*\\ntu 6u*\\ntu 7u*\\ntu 8u*\\ntu 9u*\\ntu 10u*\\ntu 11u*\\ntu 12u*\\ntu 13u*\\ntu 14u*\\ntu
..
.		\" P2 - end of program
.de P2
.br
.ps \\n(PS
.vs \\nvu
.ft 1
.in
.X "END US P1
.X "SP \\n(DV P2"
.fi
..
.		\" P3 - provides optional unpadded break in P1/P2
.de P3
.nr x \\n(DV
.nr DV 0
.P2
.P1 \\n($1u
.nr DV \\nx
..
.de [
[
..
.de ]
]
..
.IZ
.rm IZ
.so /home/pcanning/share/plan9port/tmac/tmac.srefs
