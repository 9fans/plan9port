       GV.scaleBase:	       1
       GV.scales:	       Natural size,   1.000,  screen  \n\
			       Pixel based,    1.000,  pixel   \n\
			       0.100,	       0.100	       \n\
			       0.125,	       0.125	       \n\
			       0.250,	       0.250	       \n\
			       0.500,	       0.500	       \n\
			       0.707,	       0.707	       \n\
			       1.000,	       1.000	       \n\
			       1.414,	       1.414	       \n\
			       2.828,	       2.828	       \n\
			       3.000,	       3.000	       \n\
			       4.000,	       4.000	       \n\
			       8.000,	       8.000	       \n\
			       10.00,	       10.00

GV.gsQuiet:		True
GV.gsSafer:		False
GV.gsArguments:		
GV.gsX11AlphaDevice:    -sDEVICE=x11 -dTextAlphaBits=4 -dGraphicsAlphaBits=4 -dMaxBitmap=10000000 -dNOPLATFONTS
GV.gsX11Device:		-sDEVICE=x11
GV.gsCmdConvPDF:	gs -dNOPAUSE -dQUIET -dBATCH -sDEVICE=pswrite -sOutputFile='%s' -f '%s' -c save pop quit
GV.gsCmdScanPDF:	gs -dNODISPLAY -dQUIET -sPDFname='%s' -sDSCname='%s' pdf2dsc.ps -c quit
GV.gsInterpreter:       gs
