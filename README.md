This is a port of many Plan 9 libraries and programs to Unix.

Installation
------------

To install, run ./INSTALL.  It builds mk and then uses mk to
run the rest of the installation.  

For more details, see install(1), at install.txt in this directory
and at http://swtch.com/plan9port/man/man1/install.html.

Documentation
------------

See http://swtch.com/plan9port/man/ for more documentation.
(Documentation is also in this tree, but you need to run
a successful install first.  After that, "9 man 1 intro".)

Intro(1) contains a list of man pages that describe new features
or differences from Plan 9.

Helping out
------------

If you'd like to help out, great!  The TODO file contains a small list.

If you port this code to other architectures, please share your changes
so others can benefit.

Please use Git (see below) and to prepare patches for code review
(see codereview(1))

Git
---

You can use Git to keep your local copy up-to-date as we make 
changes and fix bugs.  See the git(1) man page here ("9 man git")
for details on using git. XXX NOT READY YET XXX.

TODO(rsc): Write git(1) man page.

Contact
-------

- Mailing list: http://groups.google.com/group/plan9port-dev
- Issue tracker: http://code.swtch.com/plan9port/issues/
- Submitting changes: http://swtch.com/go/codereview

- Maintainer: Russ Cox <rsc@swtch.com>
