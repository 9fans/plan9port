This is a port of many Plan 9 libraries and programs to Unix.

* Installation

To install, run ./INSTALL.  It builds mk and then uses mk to
run the rest of the installation.  

For more details, see install(1), at install.txt in this directory
and at http://swtch.com/plan9port/man/man1/install.html.

* Documentation

See http://swtch.com/plan9port/man/ for more documentation.
(Documentation is also in this tree, but you need to run
a successful install first.  After that, "9 man 1 intro".)

Intro(1) contains a list of man pages that describe new features
or differences from Plan 9.

* Helping out

If you'd like to help out, great!  The TODO file contains a small list.

If you port this code to other architectures, please share your changes
so others can benefit.

Please use diff -u or CVS (see below) to prepare patches.

* CVS

You can use CVS to keep your local copy up-to-date as we make 
changes and fix bugs.  See the cvs(1) man page here ("9 man cvs")
for details on using cvs.

* Contact

plan9port-dev@googlegroups.com
Russ Cox <rsc@swtch.com>
