'\"                Copyright (c) 1997 Lucent
'\"                  All Rights Reserved
'\"     
'\"#ident	"@(#)macros:strings.mm	3.1"
'\"	UNIX Memorandum Macros, DWB 3.1, April 1990
'\"	Company-specifics:  Lucent Bell Laboratories
'''\"	'''\"tab  begins comments.
'''\"	No comments should appear on the same line as the string definition.
'''\"	
'''\"	The following string is used by the macro MT.
'''\"	]S defined as logo character
.ds ]S \s36\(LH\s0
'''\"	}Z defined as Company Name
.ds }Z Lucent Bell Laboratories
'''\"
'''\"   Strings for proprietary markings at bottom of page.
'''\"	Free Strings:  ]Q ]R ]H ]L ]V ]W ]X ]k ]l
'''\"	
'''\"	Register ;V = user-specified year for copyright date
.nr ;V \n(yr
'''\"	LUCENT PROPRIETARY MARKINGS
'''\"	The following strings are used by the macro PM:
'''\"	
'''\"	Marking Type:  PROPRIETARY
'''\"	Invocation:  .PM 1  or  .PM P
'''\"	Strings: ]M ]O
.ds ]M \f2LUCENT \- PROPRIETARY\fP
.ds ]O \f1Use pursuant to Company Instructions.\fP
'''\"
'''\"	Marking Type:  RESTRICTED
'''\"	Invocation:  .PM 2  or  .PM RS
'''\"	Strings: ]A ]F ]G
.ds ]A \f2LUCENT \- PROPRIETARY (RESTRICTED)\fP
.ds ]F \f1Solely for authorized persons having a need to know
.ds ]G pursuant to Company Instructions.\fP
'''\"
'''\"	Marking Type:  REGISTERED
'''\"	Invocation:  .PM 3  or  .PM RG
'''\"	Strings:  ]I ]J ]K
.ds ]I \f2LUCENT \- PROPRIETARY (REGISTERED)\fP
.ds ]J \f1Solely for authorized persons having a need to know
.ds ]K and subject to cover sheet instructions.\fP
'''\"
'''\"	Marking Type:  SEE COVER PAGE
'''\"	Invocation:  .PM 4  or  .PM CP
'''\"	Strings: ]U
.ds ]U \f1SEE PROPRIETARY NOTICE ON COVER PAGE\fP
'''\"
'''\"	Marking Type:  COPYRIGHT
'''\"	Invocation:  .PM 5  or  .PM CR
'''\"	Strings: ]i ]j
.ds ]i \f1Copyright \(co 20\\n(;V Lucent\fP
.ds ]j \f1All Rights Reserved.\fP
'''\"
'''\"	Marking Type:  UNPUBLISHED WORK
'''\"	Invocation:  .PM 6  or  .PM UW
'''\"	Strings: ]M ]m ]o ]p ]i ]q ]j
.ds ]m \f1THIS DOCUMENT CONTAINS PROPRIETARY INFORMATION OF
.ds ]o LUCENT AND IS NOT TO BE DISCLOSED OR USED EXCEPT IN
.ds ]p ACCORDANCE WITH APPLICABLE AGREEMENTS.\fP
.ds ]q \f1Unpublished & Not for Publication\fP
