Summary: Simple regular expression library from Plan 9
Name: libregexp9
Version: 2.0
Release: 1
Group: Development/C
Copyright: Public Domain
Packager: Russ Cox <rsc@post.harvard.edu>
Source: http://pdos.lcs.mit.edu/~rsc/software/libregexp9-2.0.tgz
URL: http://pdos.lcs.mit.edu/~rsc/software/#libregexp9
Requires: libfmt libutf

%description
Libregexp9 is a port of Plan 9's regexp library.
It is small and simple and provides the traditional
extended regular expressions (as opposed to the
current extended regular expressions, which add {}
and various \x character classes, among other 
complications).

http://plan9.bell-labs.com/magic/man2html/2/regexp
%prep
%setup

%build
make

%install
make install

%files
/usr/local/include/regexp9.h
/usr/local/lib/libregexp9.a
/usr/local/man/man3/regexp9.3
/usr/local/man/man7/regexp9.7
